/* Baraja Española para Nintendo 3DS: siete y media, cinquillo, brisca, escoba, chinchón y mus. */
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/input.h"
#include "ui/mii.h"

#define FADE_TIME 0.25f

static const Scene* scene_for(SceneId id)
{
	switch (id) {
	case SCENE_SIETEYMEDIA:
		return &scene_sieteymedia;
	case SCENE_CINQUILLO:
		return &scene_cinquillo;
	case SCENE_BRISCA:
		return &scene_brisca;
	case SCENE_ESCOBA:
		return &scene_escoba;
	case SCENE_CHINCHON:
		return &scene_chinchon;
	case SCENE_MUS:
		return &scene_mus;
	case SCENE_GUINOTE:
		return &scene_guinote;
	case SCENE_TUTE:
		return &scene_tute;
	case SCENE_TUTE_CABRON:
		return &scene_tute_cabron;
	case SCENE_UNO:
		return &scene_uno;
	case SCENE_POKER:
		return &scene_poker;
	case SCENE_BLACKJACK:
		return &scene_blackjack;
	case SCENE_CORAZONES:
		return &scene_corazones;
	case SCENE_POCHA:
		return &scene_pocha;
	case SCENE_CONECTA4:
		return &scene_conecta4;
	case SCENE_DAMAS:
		return &scene_damas;
	case SCENE_DOMINO:
		return &scene_domino;
	case SCENE_AS23:
		return &scene_as23;
	case SCENE_MONA:
		return &scene_mona;
	case SCENE_CUCU:
		return &scene_cucu;
	case SCENE_TREINTAYUNA:
		return &scene_treintayuna;
	case SCENE_MENTIROSO:
		return &scene_mentiroso;
	case SCENE_CULO:
		return &scene_culo;
	case SCENE_RUMMIKUB:
		return &scene_rummikub;
	case SCENE_SOLITARIO:
		return &scene_solitario;
	case SCENE_PIRAMIDE:
		return &scene_piramide;
	case SCENE_PROFILE:
		return &scene_profile;
	case SCENE_RIVALS:
		return &scene_rivals;
	case SCENE_CARDBACK:
		return &scene_cardback;
	default:
		return &scene_menu;
	}
}

int main(void)
{
	if (!gfx_init()) {
		/* Sin gráficos no se puede seguir: se informa en la consola. */
		consoleInit(GFX_BOTTOM, NULL);
		printf("Error cargando romfs:/gfx\nPulsa START para salir.\n");
		while (aptMainLoop()) {
			hidScanInput();
			if (hidKeysDown() & KEY_START)
				break;
			gspWaitForVBlank();
		}
		gfx_exit();
		return 1;
	}
	srand((unsigned)time(NULL));
	audio_init();
	rivals_init();
	cardback_editor_init();

	/* La primera vez se crea el perfil antes de ir al menú */
	SceneId current = profile_get()->created ? SCENE_MENU : SCENE_PROFILE;
	SceneId pending = current;
	float fade = 1.0f;     /* 1 = negro */
	bool fading_out = false;
	scene_for(current)->enter();

	Input in = { 0 };
	const float dt = 1.0f / 60.0f;

	while (aptMainLoop()) {
		input_read(&in);
		const Scene* scene = scene_for(current);

		if (fading_out) {
			fade += dt / FADE_TIME;
			if (fade >= 1.0f) {
				fade = 1.0f;
				fading_out = false;
				if (pending == SCENE_QUIT)
					break;
				current = pending;
				scene = scene_for(current);
				scene->enter();
				/* Una actualización sin entrada antes del primer dibujo, para
				 * que la escena tenga todo preparado (botones, textos...) */
				Input none = { 0 };
				scene->update(&none, 0.0f);
			}
		} else {
			if (fade > 0.0f)
				fade -= dt / FADE_TIME;
			SceneId next = scene->update(&in, dt);
			if (next != current) {
				pending = next;
				fading_out = true;
			}
		}

		gfx_frame_begin();
		for (int eye = 0; eye < 2; eye++) {
			if (!gfx_begin_top(eye))
				continue;
			scene->draw_top();
			gfx_fade(fade);
		}
		gfx_begin_bottom();
		scene->draw_bottom();
		gfx_fade(fade);
		gfx_frame_end();
	}

	audio_exit();
	gfx_exit();
	return 0;
}
