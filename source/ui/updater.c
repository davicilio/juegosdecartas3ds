#include "ui/updater.h"

#include <3ds.h>
#include <curl/curl.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef APP_BUILD
#define APP_BUILD 0
#endif

#define BASE_URL    "https://github.com/davicilio/juegosdecartas3ds/releases/download/ultima-version/"
#define CERTS       "romfs:/certs.pem"
#define DEFAULT_3DSX "sdmc:/3ds/JuegosDeCartas.3dsx"
#define SOC_SIZE    0x100000

static volatile UpdState s_state = UPD_IDLE;
static volatile int s_remote;
static volatile long long s_now, s_total;
static char s_error[96];
static char s_path[256] = DEFAULT_3DSX;
static bool s_have_path; /* el Homebrew Launcher nos ha dado la ruta del .3dsx */
static Thread s_thread;
static u32* s_soc_buf;
static bool s_net;

int updater_build(void)
{
	return APP_BUILD;
}

void updater_set_path(const char* argv0)
{
	if (argv0 && strncmp(argv0, "sdmc:/", 6) == 0 && strstr(argv0, ".3dsx")) {
		snprintf(s_path, sizeof s_path, "%s", argv0);
		s_have_path = true;
	}
}

bool updater_is_cia(void)
{
	return !envIsHomebrew() && !s_have_path;
}

UpdState updater_state(void)
{
	return s_state;
}

int updater_remote_build(void)
{
	return s_remote;
}

float updater_progress(void)
{
	if (s_total <= 0)
		return -1.0f;
	return (float)s_now / (float)s_total;
}

const char* updater_error(void)
{
	return s_error;
}

static void fail(const char* msg)
{
	snprintf(s_error, sizeof s_error, "%s", msg);
	s_state = UPD_ERROR;
}

static bool net_up(void)
{
	if (s_net)
		return true;
	s_soc_buf = memalign(0x1000, SOC_SIZE);
	if (!s_soc_buf)
		return false;
	if (R_FAILED(socInit(s_soc_buf, SOC_SIZE))) {
		free(s_soc_buf);
		s_soc_buf = NULL;
		return false;
	}
	curl_global_init(CURL_GLOBAL_DEFAULT);
	s_net = true;
	return true;
}

/* ---------------------------------------------------------------------------
 * Descargas con curl (TLS con los certificados raíz de romfs:/certs.pem)
 * ------------------------------------------------------------------------- */

typedef size_t (*Sink)(const void* data, size_t n, void* ctx);

typedef struct {
	Sink sink;
	void* ctx;
	bool failed;
} Writer;

static size_t on_write(char* ptr, size_t size, size_t nmemb, void* user)
{
	Writer* w = user;
	size_t n = size * nmemb;
	if (w->sink(ptr, n, w->ctx) != n) {
		w->failed = true;
		return 0;
	}
	return n;
}

static int on_progress(void* user, curl_off_t total, curl_off_t now, curl_off_t ut, curl_off_t un)
{
	(void)user;
	(void)ut;
	(void)un;
	s_total = total;
	s_now = now;
	return 0;
}

static const char* curl_message(CURLcode res)
{
	switch (res) {
	case CURLE_COULDNT_RESOLVE_HOST:
	case CURLE_COULDNT_CONNECT:
		return "Sin conexión a internet";
	case CURLE_OPERATION_TIMEDOUT:
		return "La conexión ha tardado demasiado";
	case CURLE_PEER_FAILED_VERIFICATION:
	case CURLE_SSL_CACERT_BADFILE:
		return "No se ha podido comprobar que el servidor es GitHub (certificado)";
	case CURLE_SSL_CONNECT_ERROR:
		return "Fallo al establecer la conexión segura";
	default:
		return curl_easy_strerror(res);
	}
}

/* Descarga url y va pasando los datos a sink. Devuelve el código HTTP o -1. */
static long download(const char* url, Sink sink, void* ctx, long timeout)
{
	CURL* c = curl_easy_init();
	if (!c)
		return -1;
	Writer w = { sink, ctx, false };
	curl_easy_setopt(c, CURLOPT_URL, url);
	curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(c, CURLOPT_MAXREDIRS, 5L);
	curl_easy_setopt(c, CURLOPT_CAINFO, CERTS);
	curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
	curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);
	curl_easy_setopt(c, CURLOPT_USERAGENT, "JuegosDeMesaVariados/3DS");
	curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 15L);
	curl_easy_setopt(c, CURLOPT_TIMEOUT, timeout);
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, on_write);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, &w);
	curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, on_progress);
	curl_easy_setopt(c, CURLOPT_BUFFERSIZE, 64L * 1024);
	/* Nada de copias guardadas por el camino: siempre lo último publicado */
	struct curl_slist* headers = curl_slist_append(NULL, "Cache-Control: no-cache");
	headers = curl_slist_append(headers, "Pragma: no-cache");
	curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
	CURLcode res = curl_easy_perform(c);
	long code = -1;
	if (res == CURLE_OK && !w.failed)
		curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
	else if (w.failed)
		snprintf(s_error, sizeof s_error, "No se ha podido guardar lo descargado");
	else
		snprintf(s_error, sizeof s_error, "%s", curl_message(res));
	curl_easy_cleanup(c);
	curl_slist_free_all(headers);
	return code;
}

typedef struct {
	char buf[64];
	size_t n;
} Text;

static size_t to_text(const void* data, size_t n, void* ctx)
{
	Text* t = ctx;
	size_t room = sizeof t->buf - 1 - t->n;
	size_t k = n < room ? n : room;
	memcpy(t->buf + t->n, data, k);
	t->n += k;
	t->buf[t->n] = '\0';
	return n;
}

static size_t to_file(const void* data, size_t n, void* ctx)
{
	return fwrite(data, 1, n, (FILE*)ctx);
}

typedef struct {
	Handle h;
	u64 offset;
} CiaSink;

static size_t to_cia(const void* data, size_t n, void* ctx)
{
	CiaSink* s = ctx;
	u32 written = 0;
	if (R_FAILED(FSFILE_Write(s->h, &written, s->offset, data, (u32)n, 0)) || written != n)
		return 0;
	s->offset += n;
	return n;
}

/* ---------------------------------------------------------------------------
 * Hilos
 * ------------------------------------------------------------------------- */

static void check_thread(void* arg)
{
	(void)arg;
	Text t = { .n = 0 };
	long code = download(BASE_URL "version.txt", to_text, &t, 20);
	if (code != 200) {
		if (code > 0)
			snprintf(s_error, sizeof s_error, "No se encuentra la versión publicada (error %ld)", code);
		s_state = UPD_ERROR;
		return;
	}
	s_remote = atoi(t.buf);
	s_state = s_remote > APP_BUILD ? UPD_AVAILABLE : UPD_UP_TO_DATE;
}

static void install_thread(void* arg)
{
	(void)arg;
	s_now = s_total = 0;
	if (updater_is_cia()) {
		if (R_FAILED(amInit())) {
			fail("No se puede usar el servicio de instalación");
			return;
		}
		CiaSink sink = { 0, 0 };
		if (R_FAILED(AM_StartCiaInstall(MEDIATYPE_SD, &sink.h))) {
			amExit();
			fail("No se puede empezar a instalar");
			return;
		}
		long code = download(BASE_URL "JuegosDeCartas.cia", to_cia, &sink, 600);
		if (code != 200) {
			AM_CancelCIAInstall(sink.h);
			amExit();
			if (code > 0)
				snprintf(s_error, sizeof s_error, "Error al descargar (%ld)", code);
			s_state = UPD_ERROR;
			return;
		}
		Result r = AM_FinishCiaInstall(sink.h);
		amExit();
		if (R_FAILED(r)) {
			fail("La consola no ha aceptado la instalación");
			return;
		}
	} else {
		char tmp[272];
		snprintf(tmp, sizeof tmp, "%s.nuevo", s_path);
		FILE* f = fopen(tmp, "wb");
		if (!f) {
			fail("No se puede escribir en la tarjeta SD");
			return;
		}
		long code = download(BASE_URL "JuegosDeCartas.3dsx", to_file, f, 600);
		bool ok = fclose(f) == 0 && code == 200;
		if (!ok) {
			remove(tmp);
			if (code > 0 && code != 200)
				snprintf(s_error, sizeof s_error, "Error al descargar (%ld)", code);
			s_state = UPD_ERROR;
			return;
		}
		remove(s_path);
		if (rename(tmp, s_path) != 0) {
			fail("No se ha podido sustituir el .3dsx");
			return;
		}
	}
	s_state = UPD_DONE;
}

static void join(void)
{
	if (s_thread) {
		threadJoin(s_thread, U64_MAX);
		threadFree(s_thread);
		s_thread = NULL;
	}
}

static void run(ThreadFunc f, UpdState state)
{
	if (s_state == UPD_CHECKING || s_state == UPD_DOWNLOADING)
		return;
	join();
	if (!net_up()) {
		fail("No se puede usar la red");
		return;
	}
	s_error[0] = '\0';
	s_state = state;
	/* Prioridad algo más baja que el hilo principal, en cualquier núcleo */
	s_thread = threadCreate(f, NULL, 64 * 1024, 0x31, -2, false);
	if (!s_thread)
		fail("No se ha podido empezar");
}

void updater_check(void)
{
	run(check_thread, UPD_CHECKING);
}

void updater_install(void)
{
	run(install_thread, UPD_DOWNLOADING);
}

void updater_exit(void)
{
	join();
	if (s_net) {
		curl_global_cleanup();
		socExit();
		free(s_soc_buf);
		s_soc_buf = NULL;
		s_net = false;
	}
}
