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

#endif
