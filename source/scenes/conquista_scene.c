/* Conquista: el mapa de España con sus 50 provincias. Abajo se juega
 * tocando el mapa; arriba, el mapa grande con la información. */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/conquista.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

static const CpuId CPUS[CN_PLAYERS] = { CPU_COUNT, CPU_PACO, CPU_CARMEN, CPU_RAMON };
static const u32 COLOR[CN_PLAYERS] = { 0xD2282D, 0x2864C8, 0x2E9E4A, 0xE8B41E };

/* Proyección del mapa */
typedef struct {
	float k, ox, oy;
} MapView;

#define BAR_H 34.0f /* barra de arriba de la pantalla táctil: botones y mensaje */
static const MapView BOTTOM_MAP = { 22.5f, 12.0f, BAR_H };
static const MapView TOP_MAP = { 25.0f, 4.0f, 0.0f };

/* Contorno aproximado de la península (lon, lat), para pintar la tierra */
static const float COAST[][2] = { { -9.3f, 43.0f }, { -8.9f, 43.4f }, { -8.2f, 43.7f }, { -7.0f, 43.6f }, { -5.8f, 43.6f },
	{ -4.5f, 43.45f }, { -3.5f, 43.5f }, { -2.5f, 43.45f }, { -1.8f, 43.4f }, { -1.4f, 43.1f }, { -0.5f, 42.8f },
	{ 0.7f, 42.85f }, { 1.7f, 42.5f }, { 3.2f, 42.45f }, { 3.2f, 41.9f }, { 2.3f, 41.4f }, { 1.0f, 41.05f },
	{ 0.8f, 40.7f }, { 0.0f, 40.0f }, { -0.3f, 39.5f }, { 0.2f, 38.8f }, { -0.5f, 38.3f }, { -0.8f, 37.6f },
	{ -1.6f, 37.3f }, { -2.1f, 36.75f }, { -3.0f, 36.75f }, { -4.4f, 36.7f }, { -5.3f, 36.1f }, { -6.0f, 36.3f },
	{ -6.4f, 36.8f }, { -7.4f, 37.2f }, { -8.9f, 37.0f }, { -8.8f, 38.0f }, { -9.5f, 38.8f }, { -9.0f, 40.0f },
	{ -8.7f, 41.0f }, { -8.9f, 41.9f }, { -8.9f, 42.5f } };
#define COAST_N ((int)(sizeof COAST / sizeof COAST[0]))

typedef enum { MODE_SETUP, MODE_PLAY } Mode;

static Conquista s_game;
static bool s_started, s_recorded;
static Mode s_mode;
static int s_players = 4;
static int s_cursor = 43; /* Madrid */
static int s_from = -1, s_to = -1, s_move_n = 1;
static float s_cpu_timer, s_end_time, s_dice_time;
static char s_message[128];
static Mood s_mood[CN_PLAYERS];
static Button s_btn_a, s_btn_x, s_btn_b, s_btn_end, s_btn_players, s_btn_play, s_btn_menu;

static const char* name(int p)
{
	return p == 0 ? profile_get()->avatar.name : cpu_name(CPUS[p]);
}

static const Avatar* avatar(int p)
{
	return p == 0 ? &profile_get()->avatar : cpu_avatar(CPUS[p]);
}

static void set_message(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void set_message(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(s_message, sizeof s_message, fmt, ap);
	va_end(ap);
}

static void lonlat_xy(const MapView* v, float lon, float lat, float* x, float* y)
{
	*x = v->ox + (lon + 9.6f) * v->k;
	*y = v->oy + (43.9f - lat) * v->k * 1.12f;
}

static void prov_xy(const MapView* v, int i, float* x, float* y)
{
	lonlat_xy(v, CN_MAP[i].lon, CN_MAP[i].lat, x, y);
}

/* ---------------------------------------------------------------------------
 * Mapa
 * ------------------------------------------------------------------------- */

static void draw_map(const MapView* v, float depth, bool interactive)
{
	const Conquista* g = &s_game;
	float r = v->k * 0.34f;
	/* Mar y tierra */
	float cx, cy;
	lonlat_xy(v, -3.7f, 40.4f, &cx, &cy);
	for (int i = 0; i < COAST_N; i++) {
		float x0, y0, x1, y1;
		lonlat_xy(v, COAST[i][0], COAST[i][1], &x0, &y0);
		lonlat_xy(v, COAST[(i + 1) % COAST_N][0], COAST[(i + 1) % COAST_N][1], &x1, &y1);
		gfx_triangle(cx, cy, x0, y0, x1, y1, depth, C2D_Color32(214, 200, 160, 255));
	}
	/* Portugal, en gris */
	float px, py;
	lonlat_xy(v, -8.1f, 39.6f, &px, &py);
	gfx_text("Portugal", px, py, depth, 0.42f, C2D_Color32(150, 140, 110, 255), C2D_AlignCenter);
	/* Baleares y el recuadro de Canarias */
	float bx, by;
	prov_xy(v, 12, &bx, &by);
	gfx_ellipse(bx, by, r * 2.0f, r * 1.2f, depth, C2D_Color32(214, 200, 160, 255));
	float lx, ly, tx, ty;
	prov_xy(v, 13, &lx, &ly);
	prov_xy(v, 14, &tx, &ty);
	gfx_rect(tx - r * 2.2f, ty - r * 1.9f, lx - tx + r * 4.4f, r * 3.6f, depth, C2D_Color32(40, 90, 140, 255));
	gfx_rect(tx - r * 2.2f + 1, ty - r * 1.9f + 1, lx - tx + r * 4.4f - 2, r * 3.6f - 2, depth, C2D_Color32(70, 130, 190, 255));
	/* Vecindades */
	for (int a = 0; a < CN_PROVINCES; a++)
		for (int k = 0; k < CN_MAX_ADJ && CN_MAP[a].adj[k] >= 0; k++) {
			int b = CN_MAP[a].adj[k];
			if (b < a)
				continue;
			float x0, y0, x1, y1;
			prov_xy(v, a, &x0, &y0);
			prov_xy(v, b, &x1, &y1);
			bool sea = CN_MAP[a].region != CN_MAP[b].region && (a == 12 || b == 12 || a == 13 || b == 13 || a == 14 ||
			                                                      b == 14);
			gfx_line(x0, y0, x1, y1, 1.0f, depth,
				sea ? C2D_Color32(250, 250, 255, 110) : C2D_Color32(120, 100, 70, 150));
		}
	/* Provincias */
	float pulse = 0.5f + 0.5f * sinf(gfx_time() * 6.0f);
	for (int i = 0; i < CN_PROVINCES; i++) {
		float x, y;
		prov_xy(v, i, &x, &y);
		bool target = interactive && s_from >= 0 && s_to < 0 &&
			((g->phase == CN_ATTACK && cn_can_attack(g, s_from, i)) || (g->phase == CN_FORTIFY && cn_can_move(g, s_from, i)));
		if (i == s_from || i == s_to)
			gfx_ellipse(x, y, r + 3, r + 3, depth, COLOR_GOLD);
		else if (target)
			gfx_ellipse(x, y, r + 3, r + 3, depth, C2D_Color32(255, 255, 255, (u8)(120 + 120 * pulse)));
		gfx_ellipse(x, y, r + 1, r + 1, depth, C2D_Color32(30, 25, 20, 255));
		gfx_ellipse(x, y, r, r, depth, gfx_rgb(COLOR[g->owner[i]], 255));
		gfx_textf_plain(x, y - 15.0f * (r / 18.0f), depth, r / 18.0f, COLOR_WHITE, C2D_AlignCenter, "%d", g->troops[i]);
	}
	if (interactive && s_mode == MODE_PLAY) {
		float x, y;
		prov_xy(v, s_cursor, &x, &y);
		u32 k = C2D_Color32(255, 230, 80, (u8)(150 + 100 * pulse));
		gfx_rect(x - r - 3, y - r - 3, 2 * r + 6, 2, depth, k);
		gfx_rect(x - r - 3, y + r + 1, 2 * r + 6, 2, depth, k);
		gfx_rect(x - r - 3, y - r - 3, 2, 2 * r + 6, depth, k);
		gfx_rect(x + r + 1, y - r - 3, 2, 2 * r + 6, depth, k);
	}
}

/* ---------------------------------------------------------------------------
 * Partida
 * ------------------------------------------------------------------------- */

static void start(void)
{
	cn_init(&s_game, s_players, (uint32_t)time(NULL) ^ 0xC0A);
	s_mode = MODE_PLAY;
	s_from = s_to = -1;
	s_recorded = false;
	s_cpu_timer = 0.0f;
	/* El cursor empieza en tu provincia más cercana a Madrid */
	s_cursor = 43;
	for (int k = 0; k < CN_MAX_ADJ && s_game.owner[s_cursor] != 0; k++)
		if (CN_MAP[43].adj[k] >= 0 && s_game.owner[CN_MAP[43].adj[k]] == 0)
			s_cursor = CN_MAP[43].adj[k];
	for (int i = 0; i < CN_PROVINCES && s_game.owner[s_cursor] != 0; i++)
		if (s_game.owner[i] == 0)
			s_cursor = i;
	set_message("Coloca tus %d tropas de refuerzo tocando tus provincias", s_game.to_place);
}

/* Reparto de muestra para el mapa de la pantalla de inicio */
static void preview(void)
{
	cn_init(&s_game, s_players, 12345);
	s_mode = MODE_SETUP;
}

static void cn_enter(void)
{
	if (!s_started) {
		s_started = true;
		preview();
	}
	button_set(&s_btn_menu, 262, 2, 54, 18, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

/* Provincia tocada (la más cercana, si está cerca) */
static int touched(const Input* in)
{
	if (!in->touch_down || in->ty < BAR_H)
		return -1;
	int best = -1;
	float bd = 14.0f * 14.0f;
	for (int i = 0; i < CN_PROVINCES; i++) {
		float x, y;
		prov_xy(&BOTTOM_MAP, i, &x, &y);
		float d = (x - in->tx) * (x - in->tx) + (y - in->ty) * (y - in->ty);
		if (d < bd) {
			bd = d;
			best = i;
		}
	}
	return best;
}

/* La cruceta salta a la provincia más cercana en esa dirección */
static void move_cursor(int dx, int dy)
{
	float x0, y0;
	prov_xy(&BOTTOM_MAP, s_cursor, &x0, &y0);
	int best = -1;
	float bd = 1e9f;
	for (int i = 0; i < CN_PROVINCES; i++) {
		if (i == s_cursor)
			continue;
		float x, y;
		prov_xy(&BOTTOM_MAP, i, &x, &y);
		float ex = x - x0, ey = y - y0;
		float along = ex * dx + ey * dy, across = fabsf(ex * dy - ey * dx);
		if (along <= 0 || across > along * 1.2f)
			continue;
		float d = along + across * 2;
		if (d < bd) {
			bd = d;
			best = i;
		}
	}
	if (best >= 0)
		s_cursor = best;
}

static void after_battle(int from, int to)
{
	const Conquista* g = &s_game;
	s_dice_time = 0.0f;
	if (g->conquered) {
		set_message("¡%s conquista %s!", g->current == 0 ? "Tu ejército" : name(g->current), CN_MAP[to].name);
		mood_set(&s_mood[g->current], EXPR_HAPPY, 1.5f);
		audio_play(SFX_WIN);
	} else if (g->troops[from] < 2) {
		set_message("%s resiste el ataque", CN_MAP[to].name);
		audio_play(SFX_LOSE);
	} else {
		set_message("%s %d contra %d: %s", CN_MAP[to].name, g->die_a, g->die_d,
			g->attacker_won_round ? "cae un defensor" : "cae un atacante");
		audio_play(SFX_CHIP);
	}
}

/* Selección en las fases de ataque y movimiento */
static void select_province(int i)
{
	Conquista* g = &s_game;
	if (s_from >= 0 && s_to < 0) {
		if ((g->phase == CN_ATTACK && cn_can_attack(g, s_from, i)) || (g->phase == CN_FORTIFY && cn_can_move(g, s_from, i))) {
			s_to = i;
			s_move_n = g->troops[s_from] - 1;
			audio_play(SFX_SELECT);
			return;
		}
	}
	if (g->owner[i] == 0 && g->troops[i] >= 2) {
		s_from = i == s_from ? -1 : i;
		s_to = -1;
		audio_play(SFX_SELECT);
	} else {
		audio_play(SFX_ERROR);
	}
}

static void human_turn(const Input* in)
{
	Conquista* g = &s_game;
	if (in->repeat & KEY_LEFT)
		move_cursor(-1, 0);
	if (in->repeat & KEY_RIGHT)
		move_cursor(1, 0);
	if (in->repeat & KEY_UP)
		move_cursor(0, -1);
	if (in->repeat & KEY_DOWN)
		move_cursor(0, 1);
	int t = touched(in);
	if (t >= 0)
		s_cursor = t;

	switch (g->phase) {
	case CN_REINFORCE: {
		bool all = in->down & KEY_X;
		if ((t >= 0 || (in->down & KEY_A) || all) && g->owner[s_cursor] == 0) {
			cn_place(g, s_cursor, all ? g->to_place : 1);
			audio_play(SFX_CHIP);
			if (g->phase == CN_ATTACK)
				set_message("¡Al ataque! Toca tu provincia y luego una vecina enemiga");
			else
				set_message("Te quedan %d tropas por colocar (" GLYPH_X ": todas aquí)", g->to_place);
		} else if (t >= 0 || (in->down & KEY_A)) {
			audio_play(SFX_ERROR);
		}
		break;
	}
	case CN_ATTACK:
		button_set(&s_btn_end, 4, 2, 128, 18, GLYPH_Y " Fin ataques", C2D_Color32(200, 140, 30, 255), KEY_Y);
		if (s_to >= 0) {
			button_set(&s_btn_a, 240, 172, 76, 20, GLYPH_A " Atacar", C2D_Color32(200, 40, 45, 255), KEY_A);
			button_set(&s_btn_x, 240, 195, 76, 20, GLYPH_X " A muerte", C2D_Color32(150, 30, 35, 255), KEY_X);
			button_set(&s_btn_b, 240, 218, 76, 20, GLYPH_B " Parar", C2D_Color32(120, 120, 120, 255), KEY_B);
			bool round = button_update(&s_btn_a, in), all = !round && button_update(&s_btn_x, in);
			if (round || all) {
				int from = s_from, to = s_to;
				if (all)
					cn_attack_all(g, from, to);
				else
					cn_attack_round(g, from, to);
				after_battle(from, to);
				if (g->conquered || g->troops[from] < 2 || g->phase == CN_OVER) {
					s_from = g->conquered && g->troops[to] >= 2 ? to : -1;
					s_to = -1;
				}
			} else if (button_update(&s_btn_b, in)) {
				s_to = -1;
			}
			break;
		}
		if (button_update(&s_btn_end, in)) {
			cn_end_attacks(g);
			s_from = s_to = -1;
			set_message("Puedes mover tropas una vez entre dos provincias tuyas");
			break;
		}
		if (t >= 0 || (in->down & KEY_A))
			select_province(s_cursor);
		if (in->down & KEY_B)
			s_from = -1;
		break;
	case CN_FORTIFY:
		button_set(&s_btn_end, 4, 2, 128, 18, GLYPH_Y " Fin del turno", C2D_Color32(200, 140, 30, 255), KEY_Y);
		if (s_to >= 0) {
			static char label[24];
			snprintf(label, sizeof label, GLYPH_A " Mover %d", s_move_n);
			button_set(&s_btn_a, 240, 195, 76, 20, label, C2D_Color32(40, 140, 60, 255), KEY_A);
			button_set(&s_btn_b, 240, 218, 76, 20, GLYPH_B " Parar", C2D_Color32(120, 120, 120, 255), KEY_B);
			int max = g->troops[s_from] - 1;
			if (in->repeat & KEY_R)
				s_move_n = s_move_n < max ? s_move_n + 1 : max;
			if (in->repeat & KEY_L)
				s_move_n = s_move_n > 1 ? s_move_n - 1 : 1;
			if (button_update(&s_btn_a, in)) {
				cn_move(g, s_from, s_to, s_move_n);
				s_from = s_to = -1;
				audio_play(SFX_CHIP);
			} else if (button_update(&s_btn_b, in)) {
				s_to = -1;
			}
			break;
		}
		if (button_update(&s_btn_end, in)) {
			cn_end_turn(g);
			s_from = s_to = -1;
			break;
		}
		if (t >= 0 || (in->down & KEY_A))
			select_province(s_cursor);
		if (in->down & KEY_B)
			s_from = -1;
		break;
	default:
		break;
	}
}

static void cpu_turn(float dt)
{
	Conquista* g = &s_game;
	s_cpu_timer += dt;
	if (s_cpu_timer < 0.6f)
		return;
	s_cpu_timer = 0.0f;
	int p = g->current;
	int owners[CN_PROVINCES];
	memcpy(owners, g->owner, sizeof owners);
	CnPhase phase = g->phase;
	cn_ai_step(g);
	if (phase == CN_REINFORCE) {
		set_message("%s recibe refuerzos", name(p));
	} else if (phase == CN_ATTACK) {
		for (int i = 0; i < CN_PROVINCES; i++)
			if (owners[i] != g->owner[i]) {
				set_message("%s conquista %s%s", name(p), CN_MAP[i].name, owners[i] == 0 ? ": ¡era tuya!" : "");
				audio_play(owners[i] == 0 ? SFX_LOSE : SFX_CHIP);
				mood_set(&s_mood[p], EXPR_HAPPY, 1.2f);
				if (owners[i] == 0)
					mood_set(&s_mood[0], EXPR_SAD, 1.5f);
				s_dice_time = 0.0f;
			}
	}
	if (g->current == 0 && g->phase == CN_REINFORCE) {
		s_cursor = s_cursor >= 0 && g->owner[s_cursor] == 0 ? s_cursor : 0;
		for (int i = 0; i < CN_PROVINCES && g->owner[s_cursor] != 0; i++)
			if (g->owner[i] == 0)
				s_cursor = i;
		set_message("Tu turno: coloca tus %d tropas de refuerzo", g->to_place);
		audio_play(SFX_CONFIRM);
	}
}

static SceneId setup_update(const Input* in)
{
	static char label[24];
	snprintf(label, sizeof label, GLYPH_X " %d jugadores", s_players);
	button_set(&s_btn_players, 60, 160, 200, 30, label, C2D_Color32(60, 90, 160, 255), KEY_X);
	button_set(&s_btn_play, 60, 200, 200, 30, GLYPH_A " Jugar", C2D_Color32(40, 140, 60, 255), KEY_A);
	if (button_update(&s_btn_players, in)) {
		s_players = s_players == 4 ? 2 : s_players + 1;
		preview();
	}
	else if (button_update(&s_btn_play, in))
		start();
	return SCENE_CONQUISTA;
}

static SceneId cn_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	for (int p = 0; p < CN_PLAYERS; p++)
		mood_update(&s_mood[p], dt);
	s_dice_time += dt;
	if (s_mode == MODE_SETUP)
		return setup_update(in);
	Conquista* g = &s_game;
	if (g->phase == CN_OVER) {
		s_end_time += dt;
		if (!s_recorded) {
			s_recorded = true;
			s_end_time = 0.0f;
			bool won = g->winner == 0;
			profile_record_game(GAME_CONQUISTA, won);
			audio_play(won ? SFX_WIN : SFX_LOSE);
		}
		button_set(&s_btn_play, 240, 210, 76, 26, GLYPH_A " Otra", C2D_Color32(40, 140, 60, 255), KEY_A);
		s_btn_play.enabled = s_end_time > 1.0f;
		if (button_update(&s_btn_play, in))
			preview();
		return SCENE_CONQUISTA;
	}
	if (g->current == 0)
		human_turn(in);
	else
		cpu_turn(dt);
	return SCENE_CONQUISTA;
}

static const char* phase_name(void)
{
	switch (s_game.phase) {
	case CN_REINFORCE:
		return "Refuerzos";
	case CN_ATTACK:
		return "Ataque";
	case CN_FORTIFY:
		return "Movimiento";
	default:
		return "Fin";
	}
}

static void cn_draw_top(void)
{
	const Conquista* g = &s_game;
	gfx_rect(0, 0, TOP_W, SCREEN_H, DEPTH_TABLE, C2D_Color32(60, 110, 170, 255));
	if (s_mode == MODE_SETUP) {
		draw_map(&TOP_MAP, DEPTH_TABLE, false);
		gfx_panel(100, 70, 200, 70, DEPTH_BANNER, C2D_Color32(110, 20, 25, 235), COLOR_GOLD);
		gfx_text("Conquista", 200, 76, DEPTH_BANNER, 0.9f, COLOR_GOLD, C2D_AlignCenter);
		gfx_text("España, provincia a provincia", 200, 110, DEPTH_BANNER, 0.45f, COLOR_CREAM, C2D_AlignCenter);
		return;
	}
	draw_map(&TOP_MAP, DEPTH_TABLE, false);
	/* Jugadores, en el Mediterráneo (dos por fila) */
	for (int p = 0; p < g->players; p++) {
		float x = 240 + (p % 2) * 80, y = 186 + (p / 2) * 27;
		bool turn = g->current == p && g->phase != CN_OVER;
		gfx_panel(x, y, 78, 26, DEPTH_UI, C2D_Color32(20, 30, 40, 220), turn ? COLOR_GOLD : gfx_rgb(COLOR[p], 255));
		mii_draw(avatar(p), x + 12, y + 13, 8, DEPTH_UI, mood_expr(&s_mood[p]), false);
		if (g->out[p])
			gfx_text("fuera", x + 24, y + 5, DEPTH_UI, 0.42f, COLOR_RED, C2D_AlignLeft);
		else
			gfx_textf(x + 24, y + 5, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignLeft, "%d prov.", cn_provinces_of(g, p));
	}
	/* Provincia del cursor */
	int i = s_to >= 0 ? s_to : s_cursor;
	gfx_panel(240, 134, 158, 50, DEPTH_UI, C2D_Color32(20, 30, 40, 225), gfx_rgb(COLOR[g->owner[i]], 255));
	gfx_text(CN_MAP[i].name, 246, 136, DEPTH_UI, 0.45f, COLOR_GOLD, C2D_AlignLeft);
	gfx_text(CN_REGION[CN_MAP[i].region].name, 246, 151, DEPTH_UI, 0.4f, COLOR_CREAM, C2D_AlignLeft);
	gfx_textf(246, 166, DEPTH_UI, 0.4f, COLOR_CREAM, C2D_AlignLeft, "%s · %d tropas", name(g->owner[i]), g->troops[i]);
	gfx_textf(396, 4, DEPTH_UI, 0.42f, COLOR_WHITE, C2D_AlignRight, "Ronda %d de %d", g->round + 1, CN_MAX_ROUNDS);
	/* Dados de la última batalla */
	if (s_dice_time < 1.5f && g->die_a) {
		for (int k = 0; k < 2; k++) {
			float x = 342 + k * 26, y = 100;
			gfx_rrect(x, y, 22, 22, 5, DEPTH_BANNER, k == 0 ? C2D_Color32(250, 240, 240, 255) : C2D_Color32(240, 240, 250, 255));
			gfx_textf_plain(x + 11, y + 3, DEPTH_BANNER, 0.55f, k == 0 ? COLOR_RED : C2D_Color32(40, 60, 160, 255),
				C2D_AlignCenter, "%d", k == 0 ? g->die_a : g->die_d);
		}
	}
	if (g->phase == CN_OVER && banner_draw(s_end_time, 200, 120, 300, 70)) {
		if (g->winner == 0)
			gfx_text("¡España es tuya!", 200, 100, DEPTH_BANNER, 0.7f, COLOR_GOLD, C2D_AlignCenter);
		else
			gfx_textf(200, 100, DEPTH_BANNER, 0.62f, COLOR_CREAM, C2D_AlignCenter, "Gana %s", name(g->winner));
		gfx_textf(200, 128, DEPTH_BANNER, 0.45f, COLOR_CREAM, C2D_AlignCenter, "con %d provincias",
			cn_provinces_of(g, g->winner));
	}
}

static void cn_draw_bottom(void)
{
	const Conquista* g = &s_game;
	gfx_rect(0, 0, BOTTOM_W, SCREEN_H, 0, C2D_Color32(60, 110, 170, 255));
	if (s_mode == MODE_SETUP) {
		gfx_background(BG_TABLE_BOTTOM);
		gfx_text_wrap("Conquista las 50 provincias de España. En cada turno recibes refuerzos (más si tienes "
		              "comunidades autónomas enteras), atacas a tus vecinos con los dados y mueves tropas. "
		              "Gana quien conquiste toda España o, tras 40 rondas, quien tenga más provincias.",
			16, 14, 0, 0.45f, COLOR_CREAM, 288);
		button_draw(&s_btn_players, false);
		button_draw(&s_btn_play, true);
		button_draw(&s_btn_menu, false);
		return;
	}
	draw_map(&BOTTOM_MAP, 0, true);
	gfx_rect(0, 0, BOTTOM_W, BAR_H, 0, C2D_Color32(20, 30, 40, 230));
	button_draw(&s_btn_menu, false);
	if (g->current == 0 && g->phase != CN_OVER) {
		if (g->phase == CN_REINFORCE)
			gfx_textf(6, 3, 0, 0.45f, COLOR_GOLD, C2D_AlignLeft, "Refuerzos: %d por colocar", g->to_place);
		else
			button_draw(&s_btn_end, true);
		gfx_text(phase_name(), 196, 3, 0, 0.45f, COLOR_CREAM, C2D_AlignCenter);
	} else if (g->phase != CN_OVER) {
		gfx_textf(6, 3, 0, 0.45f, gfx_rgb(COLOR[g->current], 255), C2D_AlignLeft, "Juega %s (%s)", name(g->current),
			phase_name());
	}
	gfx_text(s_message, 6, 19, 0, 0.4f, COLOR_WHITE, C2D_AlignLeft);
	if (g->phase == CN_OVER) {
		button_draw(&s_btn_play, s_btn_play.enabled);
		return;
	}
	if (g->current == 0 && s_to >= 0) {
		gfx_panel(236, 156, 82, 84, 0, C2D_Color32(20, 30, 40, 230), COLOR_GOLD);
		gfx_textf(277, 157, 0, 0.42f, COLOR_CREAM, C2D_AlignCenter, "%d contra %d", g->troops[s_from], g->troops[s_to]);
		if (g->phase == CN_ATTACK) {
			button_draw(&s_btn_a, true);
			button_draw(&s_btn_x, false);
		} else {
			gfx_text("L/R: cuántas", 277, 174, 0, 0.4f, COLOR_CREAM, C2D_AlignCenter);
			button_draw(&s_btn_a, true);
		}
		button_draw(&s_btn_b, false);
	}
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_conquista = { cn_enter, cn_update, cn_draw_top, cn_draw_bottom, scene_leave };
