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
	for (int ch = 0; ch <= VOICE_CHANNEL; ch++) {
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
