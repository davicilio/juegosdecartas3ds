#include "ui/miiface.h"

#include <3ds.h>
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

/* ---------------------------------------------------------------------------
 * Descargas (hilo aparte)
 * ------------------------------------------------------------------------- */

/* GET con el servicio httpc de la consola, siguiendo redirecciones. */
static bool http_get(const char* first_url, uint8_t* buf, u32* size)
{
	char url[512];
	snprintf(url, sizeof url, "%s", first_url);
	for (int hop = 0; hop <= MAX_REDIRECTS; hop++) {
		httpcContext ctx;
		if (R_FAILED(httpcOpenContext(&ctx, HTTPC_METHOD_GET, url, 1)))
			return false;
		httpcAddRequestHeaderField(&ctx, "User-Agent", "JuegosDeCartas3DS/1.0");
		httpcSetKeepAlive(&ctx, HTTPC_KEEPALIVE_DISABLED);
		u32 status = 0;
		if (R_FAILED(httpcBeginRequest(&ctx)) || R_FAILED(httpcGetResponseStatusCode(&ctx, &status))) {
			httpcCloseContext(&ctx);
			return false;
		}
		if (status >= 301 && status <= 308 && status != 304) {
			char location[512];
			Result r = httpcGetResponseHeader(&ctx, "Location", location, sizeof location);
			httpcCloseContext(&ctx);
			if (R_FAILED(r))
				return false;
			snprintf(url, sizeof url, "%s", location);
			continue;
		}
		bool ok = false;
		if (status == 200) {
			u32 total = 0, got = 0;
			Result r;
			do {
				r = httpcDownloadData(&ctx, buf + total, MAX_DOWNLOAD - total, &got);
				total += got;
			} while (r == (Result)HTTPC_RESULTCODE_DOWNLOADPENDING && total < MAX_DOWNLOAD);
			ok = R_SUCCEEDED(r) && total > 0;
			*size = total;
		}
		httpcCloseContext(&ctx);
		return ok;
	}
	return false;
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
	bool ok = http_get(url, buf, &size) && size > 8 && memcmp(buf, PNG_SIG, 8) == 0;
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

	s_net = R_SUCCEEDED(httpcInit(0));
	if (s_net) {
		s32 prio = 0x30;
		svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
		s_thread = threadCreate(worker, NULL, 0x8000, prio + 1, -2, false);
		if (!s_thread) {
			httpcExit();
			s_net = false;
		}
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
		httpcExit();
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

bool miiface_online(void)
{
	return s_net && s_failures < MAX_FAILURES;
}
