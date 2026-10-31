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
#include "ui/widgets.h"

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
	case SCENE_PUMBA:
		return &scene_pumba;
	case SCENE_JULEPE:
		return &scene_julepe;
	case SCENE_TUTE_SUBASTADO:
		return &scene_tute_subastado;
	case SCENE_REVERSI:
		return &scene_reversi;
	case SCENE_BURRO:
		return &scene_burro;
	case SCENE_TRESENRAYA:
		return &scene_tresenraya;
	case SCENE_FLOTA:
		return &scene_flota;
	case SCENE_PARCHIS:
		return &scene_parchis;
	case SCENE_OMAHA:
		return &scene_omaha;
	case SCENE_DRAWPOKER:
		return &scene_drawpoker;
	case SCENE_VIDEOPOKER:
		return &scene_videopoker;
	case SCENE_MAHJONG:
		return &scene_mahjong;
	case SCENE_NEGOCIO:
		return &scene_negocio;
	case SCENE_RIICHI:
		return &scene_riichi;
	case SCENE_BUSCAMINAS:
		return &scene_buscaminas;
	case SCENE_PAREJAS:
		return &scene_parejas;
	case SCENE_TABLEROS:
		return &scene_tableros;
	case SCENE_TOMA6:
		return &scene_toma6;
	case SCENE_FREECELL:
		return &scene_freecell;
	case SCENE_BINGO:
		return &scene_bingo;
	case SCENE_TRESILLO:
		return &scene_tresillo;
	case SCENE_CUATRILLO:
		return &scene_cuatrillo;
	case SCENE_MIERDA:
		return &scene_mierda;
	case SCENE_OCA:
		return &scene_oca;
	case SCENE_BUTIFARRA:
		return &scene_butifarra;
	case SCENE_TRUC:
		return &scene_truc;
	case SCENE_TRUCO:
		return &scene_truco;
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
	case SCENE_FICHAS:
		return &scene_fichas;
	default:
		return &scene_menu;
	}
}

/* Las partidas se pausan al ir al menú; el resto de pantallas no. */
static bool is_game(SceneId id)
{
	return id != SCENE_MENU && id != SCENE_PROFILE && id != SCENE_RIVALS && id != SCENE_CARDBACK &&
		id != SCENE_FICHAS && id != SCENE_TABLEROS && id < SCENE_COUNT;
}

/* Juegos de cartas (los de tablero, fichas o bolas no llevan música) */
static bool is_card_game(SceneId id)
{
	switch (id) {
	case SCENE_CONECTA4:
	case SCENE_DAMAS:
	case SCENE_DOMINO:
	case SCENE_REVERSI:
	case SCENE_OCA:
	case SCENE_PARCHIS:
	case SCENE_TRESENRAYA:
	case SCENE_FLOTA:
	case SCENE_MAHJONG:
	case SCENE_NEGOCIO:
	case SCENE_RIICHI:
	case SCENE_BUSCAMINAS:
	case SCENE_BINGO:
	case SCENE_RUMMIKUB:
		return false;
	default:
		return is_game(id);
	}
}

/* Música de los juegos de cartas, en bucle: la pone el jugador en la SD */
#define GAME_MUSIC "sdmc:/3ds/JuegosDeCartas/juegos.wav"

/* Pausa: la partida se queda quieta debajo hasta que se reanuda o se sale */
static Button s_btn_resume, s_btn_quit;

static void pause_start(void)
{
	button_set(&s_btn_resume, 40, 70, 240, 44, GLYPH_A " Reanudar", C2D_Color32(40, 140, 60, 255), KEY_A);
	button_set(&s_btn_quit, 40, 134, 240, 44, GLYPH_X " Salir", C2D_Color32(190, 60, 50, 255), KEY_X);
	audio_play(SFX_SELECT);
}

static void pause_draw_top(void)
{
	gfx_rect(0, 0, TOP_W, SCREEN_H, 0, C2D_Color32(0, 0, 0, 150));
	gfx_panel(120, 90, 160, 60, 0, C2D_Color32(110, 20, 25, 235), COLOR_GOLD);
	gfx_text("Pausa", TOP_W / 2, 104, 0, 0.9f, COLOR_GOLD, C2D_AlignCenter);
}

static void pause_draw_bottom(void)
{
	gfx_rect(0, 0, BOTTOM_W, SCREEN_H, 0, C2D_Color32(20, 4, 6, 225));
	gfx_text("Partida en pausa", BOTTOM_W / 2, 24, 0, 0.6f, COLOR_GOLD, C2D_AlignCenter);
	button_draw(&s_btn_resume, true);
	button_draw(&s_btn_quit, false);
	gfx_text_wrap("Si sales, la partida se da por acabada y vuelven a tu monedero las fichas que tenías en la mesa.",
		24, 190, 0, 0.42f, C2D_Color32(230, 215, 190, 255), 272);
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
	bool paused = false;
	scene_for(current)->enter();

	Input in = { 0 };
	const float dt = 1.0f / 60.0f;

	while (aptMainLoop()) {
		input_read(&in);
		audio_music_update();
		const Scene* scene = scene_for(current);

		if (fading_out) {
			fade += dt / FADE_TIME;
			if (fade >= 1.0f) {
				fade = 1.0f;
				fading_out = false;
				if (pending == SCENE_QUIT)
					break;
				/* La música del menú la pone el menú; la de los juegos, aquí */
				if (is_card_game(pending)) {
					if (!is_card_game(current))
						audio_music_start(GAME_MUSIC, 0);
				} else if (is_game(current)) {
					audio_music_stop();
				}
				current = pending;
				scene = scene_for(current);
				scene->enter();
				/* Una actualización sin entrada antes del primer dibujo, para
				 * que la escena tenga todo preparado (botones, textos...) */
				Input none = { 0 };
				scene->update(&none, 0.0f);
			}
		} else if (paused) {
			if (button_update(&s_btn_resume, &in) || (in.down & (KEY_B | KEY_START))) {
				paused = false;
			} else if (button_update(&s_btn_quit, &in)) {
				paused = false;
				if (scene->leave)
					scene->leave();
				pending = SCENE_MENU;
				fading_out = true;
			}
		} else {
			if (fade > 0.0f)
				fade -= dt / FADE_TIME;
			SceneId next = scene->update(&in, dt);
			if (next == SCENE_MENU && is_game(current)) {
				paused = true;
				pause_start();
			} else if (next != current) {
				pending = next;
				fading_out = true;
			}
		}

		gfx_frame_begin();
		for (int eye = 0; eye < 2; eye++) {
			if (!gfx_begin_top(eye))
				continue;
			scene->draw_top();
			if (paused)
				pause_draw_top();
			gfx_fade(fade);
		}
		gfx_begin_bottom();
		scene->draw_bottom();
		if (paused)
			pause_draw_bottom();
		gfx_fade(fade);
		gfx_frame_end();
	}

	/* Si se cierra la aplicación en mitad de una partida, las fichas que
	 * había en la mesa vuelven al monedero */
	const Scene* last = scene_for(current);
	if (is_game(current) && last->leave)
		last->leave();

	audio_exit();
	gfx_exit();
	return 0;
}
