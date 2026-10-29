#include "ui/audio.h"

#include <3ds.h>
#include <stdio.h>
#include <string.h>

#define SAMPLE_RATE 22050
#define CHANNELS    8
#define MIN_GAP_MS  40 /* evita que un mismo efecto se acumule */

static const char* const FILES[SFX_COUNT] = {
	"card", "flip", "chip", "select", "confirm", "error", "win", "lose"
};

static bool s_enabled;
static s16* s_data[SFX_COUNT];
static u32 s_samples[SFX_COUNT];
static u64 s_last_play[SFX_COUNT];
static ndspWaveBuf s_wavebufs[CHANNELS];
static int s_next_channel;
/* La voz va por su propio canal, con la frase cargada en memoria */
#define VOICE_CHANNEL CHANNELS
#define MUSIC_CHANNEL (CHANNELS + 1)
#define MUSIC_BUFS    2
#define MUSIC_FRAMES  8192 /* por búfer */

static FILE* s_music;
static long s_music_data;     /* dónde empiezan las muestras en el fichero */
static u32 s_music_left;      /* tramas que quedan del fichero */
static u32 s_music_pos;       /* tramas ya leídas */
static u32 s_music_limit;     /* tramas que se tocan antes de volver a empezar */
static int s_music_channels;
static s16* s_music_buf[MUSIC_BUFS];
static ndspWaveBuf s_music_wb[MUSIC_BUFS];
static s16* s_voice;
static ndspWaveBuf s_voice_buf;

static void load(Sfx sfx)
{
	char path[64];
	snprintf(path, sizeof path, "romfs:/sfx/%s.raw", FILES[sfx]);
	FILE* f = fopen(path, "rb");
	if (!f)
		return;
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	s_data[sfx] = linearAlloc(size);
	if (s_data[sfx] && fread(s_data[sfx], 1, size, f) == (size_t)size) {
		s_samples[sfx] = size / sizeof(s16);
		DSP_FlushDataCache(s_data[sfx], size);
	}
	fclose(f);
}

void audio_init(void)
{
	if (R_FAILED(ndspInit()))
		return;
	s_enabled = true;
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	float mix[12] = { 0 };
	mix[0] = mix[1] = 0.8f;
	for (int ch = 0; ch <= MUSIC_CHANNEL; ch++) {
		ndspChnReset(ch);
		ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
		ndspChnSetRate(ch, SAMPLE_RATE);
		ndspChnSetFormat(ch, NDSP_FORMAT_MONO_PCM16);
		ndspChnSetMix(ch, mix);
	}
	for (int i = 0; i < SFX_COUNT; i++)
		load((Sfx)i);
}

void audio_exit(void)
{
	if (!s_enabled)
		return;
	audio_music_stop();
	for (int ch = 0; ch <= VOICE_CHANNEL; ch++)
		ndspChnWaveBufClear(ch);
	ndspExit();
	if (s_voice)
		linearFree(s_voice);
	s_voice = NULL;
	for (int i = 0; i < SFX_COUNT; i++)
		if (s_data[i])
			linearFree(s_data[i]);
	s_enabled = false;
}

void audio_play(Sfx sfx)
{
	if (!s_enabled || sfx >= SFX_COUNT || !s_samples[sfx])
		return;
	u64 now = osGetTime();
	if (now - s_last_play[sfx] < MIN_GAP_MS)
		return;
	s_last_play[sfx] = now;

	int ch = s_next_channel;
	s_next_channel = (s_next_channel + 1) % CHANNELS;
	ndspChnWaveBufClear(ch);
	ndspWaveBuf* wb = &s_wavebufs[ch];
	memset(wb, 0, sizeof *wb);
	wb->data_vaddr = s_data[sfx];
	wb->nsamples = s_samples[sfx];
	ndspChnWaveBufAdd(ch, wb);
}

void audio_say(const char* name)
{
	if (!s_enabled || !name)
		return;
	char path[64];
	snprintf(path, sizeof path, "romfs:/voz/%s.raw", name);
	FILE* f = fopen(path, "rb");
	if (!f)
		return;
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	/* Se para la frase anterior antes de soltar su memoria */
	ndspChnWaveBufClear(VOICE_CHANNEL);
	if (s_voice)
		linearFree(s_voice);
	s_voice = size > 0 ? linearAlloc(size) : NULL;
	bool ok = s_voice && fread(s_voice, 1, size, f) == (size_t)size;
	fclose(f);
	if (!ok)
		return;
	DSP_FlushDataCache(s_voice, size);
	memset(&s_voice_buf, 0, sizeof s_voice_buf);
	s_voice_buf.data_vaddr = s_voice;
	s_voice_buf.nsamples = size / sizeof(s16);
	ndspChnWaveBufAdd(VOICE_CHANNEL, &s_voice_buf);
}

/* ---------------------------------------------------------------------------
 * Música desde la SD
 * ------------------------------------------------------------------------- */

static u32 read_u32(const u8* p)
{
	return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24;
}

/* Lee el siguiente trozo en el búfer i; false si no queda nada que tocar */
static bool music_fill(int i)
{
	u32 want = MUSIC_FRAMES;
	if (s_music_pos >= s_music_limit || s_music_left == 0) {
		/* Vuelta a empezar */
		fseek(s_music, s_music_data, SEEK_SET);
		s_music_left += s_music_pos;
		s_music_pos = 0;
	}
	if (want > s_music_left)
		want = s_music_left;
	if (want > s_music_limit - s_music_pos)
		want = s_music_limit - s_music_pos;
	size_t frame = s_music_channels * sizeof(s16);
	size_t got = fread(s_music_buf[i], frame, want, s_music);
	if (got == 0)
		return false;
	s_music_pos += got;
	s_music_left -= got;
	DSP_FlushDataCache(s_music_buf[i], got * frame);
	ndspWaveBuf* wb = &s_music_wb[i];
	memset(wb, 0, sizeof *wb);
	wb->data_vaddr = s_music_buf[i];
	wb->nsamples = got;
	ndspChnWaveBufAdd(MUSIC_CHANNEL, wb);
	return true;
}

bool audio_music_start(const char* path, float max_seconds)
{
	audio_music_stop();
	if (!s_enabled)
		return false;
	FILE* f = fopen(path, "rb");
	if (!f)
		return false;
	u8 head[12];
	if (fread(head, 1, 12, f) != 12 || memcmp(head, "RIFF", 4) || memcmp(head + 8, "WAVE", 4)) {
		fclose(f);
		return false;
	}
	/* Busca los trozos "fmt " y "data" */
	int channels = 0, bits = 0;
	u32 rate = 0, data_size = 0;
	long data_at = -1;
	u8 ch[8];
	while (fread(ch, 1, 8, f) == 8) {
		u32 size = read_u32(ch + 4);
		if (!memcmp(ch, "fmt ", 4)) {
			u8 fmt[16];
			if (size < 16 || fread(fmt, 1, 16, f) != 16)
				break;
			channels = fmt[2] | fmt[3] << 8;
			rate = read_u32(fmt + 4);
			bits = fmt[14] | fmt[15] << 8;
			fseek(f, size - 16 + (size & 1), SEEK_CUR);
		} else if (!memcmp(ch, "data", 4)) {
			data_at = ftell(f);
			data_size = size;
			break;
		} else {
			fseek(f, size + (size & 1), SEEK_CUR);
		}
	}
	if (data_at < 0 || bits != 16 || (channels != 1 && channels != 2) || rate < 8000 || rate > 48000) {
		fclose(f);
		return false;
	}
	for (int i = 0; i < MUSIC_BUFS; i++) {
		s_music_buf[i] = linearAlloc(MUSIC_FRAMES * channels * sizeof(s16));
		if (!s_music_buf[i]) {
			fclose(f);
			audio_music_stop();
			return false;
		}
	}
	s_music = f;
	s_music_data = data_at;
	s_music_channels = channels;
	s_music_pos = 0;
	s_music_left = data_size / (channels * sizeof(s16));
	s_music_limit = max_seconds > 0 ? (u32)(max_seconds * rate) : s_music_left;
	if (s_music_limit > s_music_left)
		s_music_limit = s_music_left;
	ndspChnReset(MUSIC_CHANNEL);
	ndspChnSetInterp(MUSIC_CHANNEL, NDSP_INTERP_LINEAR);
	ndspChnSetRate(MUSIC_CHANNEL, rate);
	ndspChnSetFormat(MUSIC_CHANNEL, channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
	/* Más baja que la voz y los efectos */
	float mix[12] = { 0 };
	mix[0] = mix[1] = 0.35f;
	ndspChnSetMix(MUSIC_CHANNEL, mix);
	for (int i = 0; i < MUSIC_BUFS; i++)
		music_fill(i);
	return true;
}

void audio_music_update(void)
{
	if (!s_music)
		return;
	for (int i = 0; i < MUSIC_BUFS; i++)
		if (s_music_wb[i].status == NDSP_WBUF_DONE)
			music_fill(i);
}

void audio_music_stop(void)
{
	if (s_enabled)
		ndspChnWaveBufClear(MUSIC_CHANNEL);
	if (s_music)
		fclose(s_music);
	s_music = NULL;
	for (int i = 0; i < MUSIC_BUFS; i++) {
		if (s_music_buf[i])
			linearFree(s_music_buf[i]);
		s_music_buf[i] = NULL;
	}
	memset(s_music_wb, 0, sizeof s_music_wb);
}

bool audio_music_playing(void)
{
	return s_music != NULL;
}
