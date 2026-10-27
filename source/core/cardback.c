#include "core/cardback.h"

#include <stdio.h>
#include <string.h>

static const uint32_t COLORS[CB_COLORS] = {
	0x8E1B22, 0xC8282D, 0xE8761E, 0xF2C230, 0x3E9A3A, 0x1E5A2E, 0x2B5BB8,
	0x182C5C, 0x3CAADE, 0x6E2C9A, 0xE86A9A, 0x5A3418, 0x202024, 0xF4F0E4,
};
static const char* const COLOR_NAMES[CB_COLORS] = {
	"Granate", "Rojo", "Naranja", "Amarillo", "Verde", "Verde oscuro", "Azul",
	"Azul marino", "Celeste", "Morado", "Rosa", "Marrón", "Negro", "Blanco",
};
static const char* const PATTERN_NAMES[CB_PATTERNS] = {
	"Liso", "Rombos", "Rayas", "Lunares", "Cuadros", "Ondas", "Estrellas",
};
static const uint32_t FRAME_RGB[CB_FRAMES] = { 0, 0xE8B21E, 0xC8CCD4, 0xFFFFFF, 0x202024 };
static const char* const FRAME_NAMES[CB_FRAMES] = { "Sin marco", "Dorado", "Plateado", "Blanco", "Negro" };
static const char* const EMBLEM_NAMES[CB_EMBLEMS] = { "Ninguno", "Los palos", "Oro", "Copa", "Corona", "Estrella" };
static const char* const POS_NAMES[CB_TEXT_POSITIONS] = { "Arriba", "En el centro", "Abajo" };
static const char* const LABELS[CB_OPTIONS] = {
	"Fondo", "Dibujo", "Color del dibujo", "Marco", "Emblema", "Color del texto", "Texto en",
};

void cardback_default(CardBack* b)
{
	memset(b, 0, sizeof *b);
	b->magic = CARDBACK_MAGIC;
	b->version = CARDBACK_VERSION;
	b->value[CB_BACKGROUND] = 0; /* granate */
	b->value[CB_PATTERN] = 1;    /* rombos */
	b->value[CB_PATTERN_COLOR] = 0;
	b->value[CB_FRAME] = 1;      /* dorado */
	b->value[CB_EMBLEM] = 1;     /* los palos */
	b->value[CB_TEXT_COLOR] = 3; /* amarillo */
	b->value[CB_TEXT_POS] = CB_TEXT_BOTTOM;
}

int cardback_option_count(CardBackOption o)
{
	switch (o) {
	case CB_BACKGROUND:
		return CB_COLORS;
	case CB_PATTERN:
		return CB_PATTERNS;
	case CB_PATTERN_COLOR:
		return CB_COLORS + 1;
	case CB_FRAME:
		return CB_FRAMES;
	case CB_EMBLEM:
		return CB_EMBLEMS;
	case CB_TEXT_COLOR:
		return CB_COLORS;
	case CB_TEXT_POS:
		return CB_TEXT_POSITIONS;
	default:
		return 1;
	}
}

const char* cardback_option_label(CardBackOption o)
{
	return o < CB_OPTIONS ? LABELS[o] : "?";
}

const char* cardback_value_name(CardBackOption o, int v)
{
	if (o >= CB_OPTIONS || v < 0 || v >= cardback_option_count(o))
		return "?";
	switch (o) {
	case CB_BACKGROUND:
	case CB_TEXT_COLOR:
		return COLOR_NAMES[v];
	case CB_PATTERN:
		return PATTERN_NAMES[v];
	case CB_PATTERN_COLOR:
		return v == 0 ? "Automático" : COLOR_NAMES[v - 1];
	case CB_FRAME:
		return FRAME_NAMES[v];
	case CB_EMBLEM:
		return EMBLEM_NAMES[v];
	case CB_TEXT_POS:
		return POS_NAMES[v];
	default:
		return "?";
	}
}

void cardback_step(CardBack* b, CardBackOption o, int dir)
{
	if (o >= CB_OPTIONS)
		return;
	int n = cardback_option_count(o);
	b->value[o] = (uint8_t)(((int)b->value[o] + (dir < 0 ? n - 1 : 1)) % n);
}

/* Longitud en bytes de los primeros max caracteres UTF-8 de s, cabiendo en size - 1 bytes. */
static size_t utf8_prefix(const char* s, int max, size_t size)
{
	size_t i = 0;
	for (int chars = 0; s[i] && chars < max; chars++) {
		size_t len = 1;
		unsigned char c = (unsigned char)s[i];
		if (c >= 0xF0)
			len = 4;
		else if (c >= 0xE0)
			len = 3;
		else if (c >= 0xC0)
			len = 2;
		size_t k = 1;
		while (k < len && ((unsigned char)s[i + k] & 0xC0) == 0x80)
			k++;
		if (k < len || i + len > size - 1)
			break; /* carácter cortado o no cabe */
		i += len;
	}
	return i;
}

void cardback_set_text(CardBack* b, const char* text)
{
	if (!text)
		text = "";
	size_t n = utf8_prefix(text, CARDBACK_TEXT_CHARS, sizeof b->text);
	memcpy(b->text, text, n);
	b->text[n] = '\0';
}

void cardback_sanitize(CardBack* b)
{
	b->magic = CARDBACK_MAGIC;
	b->version = CARDBACK_VERSION;
	for (int o = 0; o < CB_OPTIONS; o++)
		b->value[o] %= (uint8_t)cardback_option_count((CardBackOption)o);
	b->text[CARDBACK_TEXT_MAX - 1] = '\0';
	char copy[CARDBACK_TEXT_MAX];
	memcpy(copy, b->text, sizeof copy);
	cardback_set_text(b, copy);
}

bool cardback_load(CardBack* b, const char* path)
{
	CardBack tmp;
	FILE* f = fopen(path, "rb");
	if (!f)
		return false;
	size_t n = fread(&tmp, 1, sizeof tmp, f);
	fclose(f);
	if (n != sizeof tmp || tmp.magic != CARDBACK_MAGIC || tmp.version != CARDBACK_VERSION)
		return false;
	cardback_sanitize(&tmp);
	*b = tmp;
	return true;
}

bool cardback_save(const CardBack* b, const char* path)
{
	FILE* f = fopen(path, "wb");
	if (!f)
		return false;
	bool ok = fwrite(b, 1, sizeof *b, f) == sizeof *b;
	return fclose(f) == 0 && ok;
}

uint32_t cardback_color_rgb(int color)
{
	return COLORS[(unsigned)color % CB_COLORS];
}

uint32_t cardback_background_rgb(const CardBack* b)
{
	return cardback_color_rgb(b->value[CB_BACKGROUND]);
}

/* Automático: el fondo un poco más claro (o más oscuro si ya es muy claro). */
uint32_t cardback_pattern_rgb(const CardBack* b)
{
	if (b->value[CB_PATTERN_COLOR] != 0)
		return cardback_color_rgb(b->value[CB_PATTERN_COLOR] - 1);
	uint32_t bg = cardback_background_rgb(b), out = 0;
	int luma = (int)(((bg >> 16) & 0xFF) * 3 + ((bg >> 8) & 0xFF) * 6 + (bg & 0xFF)) / 10;
	for (int sh = 0; sh <= 16; sh += 8) {
		int v = (bg >> sh) & 0xFF;
		v = luma > 190 ? v * 82 / 100 : v + (255 - v) * 22 / 100;
		out |= (uint32_t)v << sh;
	}
	return out;
}

uint32_t cardback_frame_rgb(const CardBack* b)
{
	return FRAME_RGB[b->value[CB_FRAME] % CB_FRAMES];
}

uint32_t cardback_text_rgb(const CardBack* b)
{
	return cardback_color_rgb(b->value[CB_TEXT_COLOR]);
}
