/* Generado por tools/gen_characters.py: no editar a mano. */
#ifndef UI_CHARS_LAYOUT_H
#define UI_CHARS_LAYOUT_H

/* Radio de la cabeza en el atlas gfx/chars.t3s (píxeles). */
#define CHR_HEAD_RADIUS 40.0f

/* Índices del atlas, en el mismo orden que gfx/chars.t3s. */
enum {
	CHR_HEAD,
	CHR_NOSE,
	CHR_BLUSH,
	CHR_EYE_OPEN,
	CHR_EYE_LASHES,
	CHR_EYE_WIDE,
	CHR_EYE_CLOSED,
	CHR_EYE_HAPPY,
	CHR_BROW,
	CHR_MOUTH_SMILE,
	CHR_MOUTH_HAPPY,
	CHR_MOUTH_SAD,
	CHR_MOUTH_O,
	CHR_MOUTH_BITE,
	CHR_MOUTH_TONGUE,
	CHR_MOUTH_TWIST,
	CHR_GLASSES,
	CHR_BEARD,
	CHR_MUSTACHE,
	CHR_TORSO,
	CHR_ARMS,
	CHR_LEGS,
	CHR_SHINS,
	CHR_SKIRT,
	CHR_BLUSH_R,
	CHR_EYE_OPEN_R,
	CHR_EYE_LASHES_R,
	CHR_EYE_WIDE_R,
	CHR_EYE_CLOSED_R,
	CHR_EYE_HAPPY_R,
	CHR_BROW_R,
	CHR_HAIR_SHORT_FRONT,
	CHR_HAIR_SHORT_BACK,
	CHR_HAIR_LONG_FRONT,
	CHR_HAIR_LONG_BACK,
	CHR_HAIR_BUN_FRONT,
	CHR_HAIR_BUN_BACK,
	CHR_HAIR_SPIKY_FRONT,
	CHR_HAIR_SPIKY_BACK,
	CHR_HAIR_BALD_FRONT,
	CHR_HAIR_BALD_BACK,
	CHR_HAIR_BOB_FRONT,
	CHR_HAIR_BOB_BACK,
	CHR_HAIR_PONYTAIL_FRONT,
	CHR_HAIR_PONYTAIL_BACK,
	CHR_HAIR_CURLY_FRONT,
	CHR_HAIR_CURLY_BACK,
	CHR_COUNT
};

/* Centro de cada pieza respecto al centro de la cabeza (píxeles del atlas). */
static const float CHR_OFFSET[CHR_COUNT][2] = {
	{ 0.0f, 0.0f }, /* head */
	{ 0.0f, 8.0f }, /* nose */
	{ -22.5f, 12.0f }, /* blush */
	{ -13.5f, 0.5f }, /* eye_open */
	{ -16.0f, 0.5f }, /* eye_lashes */
	{ -15.0f, 2.5f }, /* eye_wide */
	{ -13.5f, 3.5f }, /* eye_closed */
	{ -14.5f, 2.0f }, /* eye_happy */
	{ -14.5f, -9.0f }, /* brow */
	{ 0.0f, 16.5f }, /* mouth_smile */
	{ 0.0f, 17.5f }, /* mouth_happy */
	{ 0.0f, 17.5f }, /* mouth_sad */
	{ 0.0f, 17.0f }, /* mouth_o */
	{ 0.0f, 18.0f }, /* mouth_bite */
	{ 0.5f, 19.0f }, /* mouth_tongue */
	{ 3.0f, 17.5f }, /* mouth_twist */
	{ 0.5f, 1.5f }, /* glasses */
	{ -0.5f, 22.5f }, /* beard */
	{ 0.0f, 11.5f }, /* mustache */
	{ 0.5f, 53.5f }, /* torso */
	{ 0.0f, 63.5f }, /* arms */
	{ 0.0f, 83.5f }, /* legs */
	{ 0.0f, 85.0f }, /* shins */
	{ 0.0f, 83.0f }, /* skirt */
	{ 22.5f, 12.0f }, /* blush_r */
	{ 13.5f, 0.5f }, /* eye_open_r */
	{ 16.0f, 0.5f }, /* eye_lashes_r */
	{ 15.0f, 2.5f }, /* eye_wide_r */
	{ 13.5f, 3.5f }, /* eye_closed_r */
	{ 14.5f, 2.0f }, /* eye_happy_r */
	{ 14.5f, -9.0f }, /* brow_r */
	{ 0.0f, -16.0f }, /* hair_short_front */
	{ 0.0f, -18.5f }, /* hair_short_back */
	{ 0.0f, -16.5f }, /* hair_long_front */
	{ 0.0f, 4.5f }, /* hair_long_back */
	{ 0.0f, -20.5f }, /* hair_bun_front */
	{ 0.5f, -22.0f }, /* hair_bun_back */
	{ 0.0f, -19.5f }, /* hair_spiky_front */
	{ 0.5f, -12.5f }, /* hair_spiky_back */
	{ 0.0f, -8.5f }, /* hair_bald_front */
	{ 0.0f, -8.0f }, /* hair_bald_back */
	{ 0.0f, -11.5f }, /* hair_bob_front */
	{ 0.5f, -3.0f }, /* hair_bob_back */
	{ 0.0f, -18.5f }, /* hair_ponytail_front */
	{ 7.0f, -5.0f }, /* hair_ponytail_back */
	{ 1.0f, -24.5f }, /* hair_curly_front */
	{ 0.5f, -13.0f }, /* hair_curly_back */
};

#endif
