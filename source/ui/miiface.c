#include "ui/miiface.h"

#include <3ds.h>
#include <curl/curl.h>
#include <malloc.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define CACHE_DIR     "sdmc:/3ds/JuegosDeCartas/miis"
#define MAX_FACES     72
#define FACE_PX       128
#define MAX_DOWNLOAD  (256 * 1024)
#define MAX_REDIRECTS 3
#define MAX_FAILURES  3 /* tras varios fallos seguidos se deja de intentar */
#define SOC_BUF_SIZE  0x100000
#define CA_BUNDLE     "romfs:/cacert.pem"

typedef enum {
	FACE_EMPTY,
	FACE_QUEUED, /* esperando al hilo de descargas */
	FACE_DOWNLOADING,
	FACE_FILE, /* en la SD, falta convertirla en textura */
	FACE_READY,
	FACE_FAILED
} FaceState;

typedef struct {
	uint32_t key;
	volatile FaceState state;
	char code[MII_STUDIO_HEX];
	char expression[24];
	char path[96];
	C3D_Tex tex;
	Tex3DS_SubTexture sub;
	C2D_Image img;
} Face;

static Face s_faces[MAX_FACES];
static LightLock s_lock;
static LightEvent s_wake;
static Thread s_thread;
static volatile bool s_quit;
static bool s_net;
static volatile int s_failures;
static int s_loads_this_frame;
static u32* s_soc_buf;
static char s_last_error[96];

/* ---------------------------------------------------------------------------
 * Descargas (hilo aparte)
 * ------------------------------------------------------------------------- */

typedef struct {
	uint8_t* data;
	size_t size;
} Buffer;

static size_t write_cb(char* ptr, size_t size, size_t nmemb, void* user)
{
	Buffer* b = user;
	size_t n = size * nmemb;
	if (b->size + n > MAX_DOWNLOAD)
		return 0;
	memcpy(b->data + b->size, ptr, n);
	b->size += n;
	return n;
}

/* GET por HTTPS con curl y mbedTLS. El certificado del servidor se comprueba
 * con los certificados raíz de Mozilla que van en el romfs del juego. */
static bool http_get(const char* url, uint8_t* buf, u32* size)
{
	CURL* curl = curl_easy_init();
	if (!curl) {
		snprintf(s_last_error, sizeof s_last_error, "No se pudo iniciar la conexión");
		return false;
	}
	Buffer b = { buf, 0 };
	char err[CURL_ERROR_SIZE] = "";
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &b);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Nintendo 3DS) JuegosDeCartas/1.0");
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, (long)MAX_REDIRECTS);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_CAINFO, CA_BUNDLE);
	CURLcode res = curl_easy_perform(curl);
	long status = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
	curl_easy_cleanup(curl);
	if (res != CURLE_OK) {
		snprintf(s_last_error, sizeof s_last_error, "Error de conexión %d: %.60s", (int)res,
			err[0] ? err : curl_easy_strerror(res));
		return false;
	}
	if (status != 200) {
		snprintf(s_last_error, sizeof s_last_error, "Mii Studio respondió HTTP %ld", status);
		return false;
	}
	*size = b.size;
	return b.size > 0;
}

static bool download(Face* f)
{
	char url[256];
	snprintf(url, sizeof url,
		"https://studio.mii.nintendo.com/miis/image.png?data=%s&type=face&expression=%s&width=%d&bgColor=FFFFFF00",
		f->code, f->expression, FACE_PX);
	uint8_t* buf = malloc(MAX_DOWNLOAD);
	if (!buf)
		return false;
	u32 size = 0;
	static const uint8_t PNG_SIG[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
	bool ok = http_get(url, buf, &size);
	if (ok && (size <= 8 || memcmp(buf, PNG_SIG, 8) != 0)) {
		snprintf(s_last_error, sizeof s_last_error, "Mii Studio no devolvió una imagen");
		ok = false;
	}
	if (ok) {
		FILE* out = fopen(f->path, "wb");
		ok = out && fwrite(buf, 1, size, out) == size;
		if (out)
			fclose(out);
		if (!ok)
			remove(f->path);
	}
	free(buf);
	return ok;
}

static void worker(void* arg)
{
	(void)arg;
	while (!s_quit) {
		Face* job = NULL;
		LightLock_Lock(&s_lock);
		for (int i = 0; i < MAX_FACES && !job; i++)
			if (s_faces[i].state == FACE_QUEUED) {
				job = &s_faces[i];
				job->state = FACE_DOWNLOADING;
			}
		LightLock_Unlock(&s_lock);
		if (!job) {
			LightEvent_Wait(&s_wake);
			continue;
		}
		bool ok = s_failures < MAX_FAILURES && download(job);
		s_failures = ok ? 0 : s_failures + 1;
		if (ok)
			s_last_error[0] = '\0';
		LightLock_Lock(&s_lock);
		job->state = ok ? FACE_FILE : FACE_FAILED;
		LightLock_Unlock(&s_lock);
	}
}

/* ---------------------------------------------------------------------------
 * PNG -> textura (hilo principal)
 * ------------------------------------------------------------------------- */

static unsigned next_pow2(unsigned v)
{
	unsigned p = 8;
	while (p < v)
		p <<= 1;
	return p;
}

static bool load_texture(Face* f)
{
	png_image png;
	memset(&png, 0, sizeof png);
	png.version = PNG_IMAGE_VERSION;
	if (!png_image_begin_read_from_file(&png, f->path))
		return false;
	png.format = PNG_FORMAT_RGBA;
	if (png.width > 512 || png.height > 512) {
		png_image_free(&png);
		return false;
	}
	uint8_t* rgba = malloc(PNG_IMAGE_SIZE(png));
	if (!rgba || !png_image_finish_read(&png, NULL, rgba, 0, NULL)) {
		free(rgba);
		png_image_free(&png);
		return false;
	}
	unsigned w = png.width, h = png.height;
	unsigned tw = next_pow2(w), th = next_pow2(h);
	if (!C3D_TexInit(&f->tex, tw, th, GPU_RGBA8)) {
		free(rgba);
		return false;
	}
	/* La GPU guarda las texturas en bloques de 8x8 en orden Morton */
	u32* dst = f->tex.data;
	memset(dst, 0, tw * th * 4);
	for (unsigned y = 0; y < h; y++)
		for (unsigned x = 0; x < w; x++) {
			const uint8_t* p = &rgba[(y * w + x) * 4];
			u32 idx = ((((y >> 3) * (tw >> 3) + (x >> 3)) << 6) +
				((x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3)));
			dst[idx] = ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
		}
	free(rgba);
	C3D_TexFlush(&f->tex);
	C3D_TexSetFilter(&f->tex, GPU_LINEAR, GPU_LINEAR);
	C3D_TexSetWrap(&f->tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
	f->sub.width = w;
	f->sub.height = h;
	f->sub.left = 0.0f;
	f->sub.top = 1.0f;
	f->sub.right = (float)w / tw;
	f->sub.bottom = 1.0f - (float)h / th;
	f->img.tex = &f->tex;
	f->img.subtex = &f->sub;
	return true;
}

/* ---------------------------------------------------------------------------
 * Interfaz
 * ------------------------------------------------------------------------- */

void miiface_init(void)
{
	LightLock_Init(&s_lock);
	LightEvent_Init(&s_wake, RESET_ONESHOT);
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/JuegosDeCartas", 0777);
	mkdir(CACHE_DIR, 0777);

	s_soc_buf = memalign(0x1000, SOC_BUF_SIZE);
	if (s_soc_buf && R_SUCCEEDED(socInit(s_soc_buf, SOC_BUF_SIZE))) {
		s_net = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
	} else {
		free(s_soc_buf);
		s_soc_buf = NULL;
		snprintf(s_last_error, sizeof s_last_error, "No se pudo iniciar la red");
	}
	if (s_net) {
		s32 prio = 0x30;
		svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
		/* El saludo TLS necesita bastante pila */
		s_thread = threadCreate(worker, NULL, 0x20000, prio + 1, -2, false);
		if (!s_thread)
			s_net = false;
	}
}

void miiface_exit(void)
{
	if (s_thread) {
		s_quit = true;
		LightEvent_Signal(&s_wake);
		threadJoin(s_thread, U64_MAX);
		threadFree(s_thread);
		s_thread = NULL;
	}
	if (s_net)
		curl_global_cleanup();
	if (s_soc_buf) {
		socExit();
		free(s_soc_buf);
		s_soc_buf = NULL;
	}
	for (int i = 0; i < MAX_FACES; i++)
		if (s_faces[i].state == FACE_READY)
			C3D_TexDelete(&s_faces[i].tex);
}

void miiface_frame(void)
{
	s_loads_this_frame = 0;
}

static uint32_t face_key(const MiiParts* mii, const char* expression)
{
	uint32_t h = mii_parts_hash(mii);
	for (const char* c = expression; *c; c++)
		h = (h ^ (uint8_t)*c) * 16777619u;
	return h ? h : 1;
}

const C2D_Image* miiface_get(const MiiParts* mii, const char* expression, bool want_download)
{
	uint32_t key = face_key(mii, expression);
	Face* f = NULL;
	for (int i = 0; i < MAX_FACES; i++)
		if (s_faces[i].state != FACE_EMPTY && s_faces[i].key == key) {
			f = &s_faces[i];
			break;
		}

	if (!f) {
		for (int i = 0; i < MAX_FACES && !f; i++)
			if (s_faces[i].state == FACE_EMPTY)
				f = &s_faces[i];
		if (!f)
			return NULL; /* caché llena: se usa el dibujo simplificado */
		f->key = key;
		snprintf(f->expression, sizeof f->expression, "%s", expression);
		mii_studio_code(mii, 0, f->code);
		snprintf(f->path, sizeof f->path, CACHE_DIR "/%08lx.png", (unsigned long)key);
		struct stat st;
		FaceState s = stat(f->path, &st) == 0              ? FACE_FILE
		            : (want_download && miiface_online()) ? FACE_QUEUED
		                                                  : FACE_FAILED;
		LightLock_Lock(&s_lock);
		f->state = s;
		LightLock_Unlock(&s_lock);
		if (s == FACE_QUEUED)
			LightEvent_Signal(&s_wake);
	}

	if (f->state == FACE_FILE && s_loads_this_frame < 2) {
		/* Se cargan como mucho dos por fotograma para no dar tirones */
		s_loads_this_frame++;
		bool ok = load_texture(f);
		if (!ok)
			remove(f->path);
		LightLock_Lock(&s_lock);
		f->state = ok ? FACE_READY : FACE_FAILED;
		LightLock_Unlock(&s_lock);
	}
	return f->state == FACE_READY ? &f->img : NULL;
}

void miiface_retry(void)
{
	s_failures = 0;
	LightLock_Lock(&s_lock);
	for (int i = 0; i < MAX_FACES; i++)
		if (s_faces[i].state == FACE_FAILED)
			s_faces[i].state = FACE_EMPTY;
	LightLock_Unlock(&s_lock);
}

int miiface_pending(void)
{
	int n = 0;
	for (int i = 0; i < MAX_FACES; i++)
		if (s_faces[i].state == FACE_QUEUED || s_faces[i].state == FACE_DOWNLOADING)
			n++;
	return n;
}

const char* miiface_last_error(void)
{
	return s_last_error;
}

bool miiface_online(void)
{
	return s_net && s_failures < MAX_FAILURES;
}
