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
#include "ui/miiface.h"

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
	case SCENE_PROFILE:
		return &scene_profile;
	case SCENE_RIVALS:
		return &scene_rivals;
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
	miiface_init();
	rivals_init();

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

		miiface_frame();
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

	miiface_exit();
	audio_exit();
	gfx_exit();
	return 0;
}
