#ifndef UI_AUDIO_H
#define UI_AUDIO_H

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

#endif
