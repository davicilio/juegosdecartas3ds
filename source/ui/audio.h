#ifndef UI_AUDIO_H
#define UI_AUDIO_H

#include <stdbool.h>

typedef enum {
	SFX_CARD,
	SFX_FLIP,
	SFX_CHIP,
	SFX_SELECT,
	SFX_CONFIRM,
	SFX_ERROR,
	SFX_WIN,
	SFX_LOSE,
	SFX_COUNT
} Sfx;

/* Si el DSP no está disponible (falta dspfirm.cdc) el juego sigue sin sonido. */
void audio_init(void);
void audio_exit(void);
void audio_play(Sfx sfx);
/* Dice una frase grabada en romfs:/voz/<nombre>.raw (se corta la anterior).
 * Si no está (por ejemplo, al compilar sin generar la voz), no suena nada. */
void audio_say(const char* name);

/* Música de fondo desde un WAV (PCM de 16 bits, mono o estéreo) de la SD.
 * Suena como mucho max_seconds y vuelve a empezar. Devuelve false si el
 * fichero no está o no se entiende. audio_music_update se llama cada frame
 * para ir leyendo el fichero. */
bool audio_music_start(const char* path, float max_seconds);
void audio_music_update(void);
void audio_music_stop(void);
bool audio_music_playing(void);

#endif
