#ifndef CORE_AVATAR_H
#define CORE_AVATAR_H

#include <stdbool.h>
#include <stdint.h>

#include "core/miidata.h"

/* Un personaje: su Mii de verdad (mii, que se dibuja con el renderizador
 * oficial de Nintendo) y una versión simplificada (el resto de campos) que se
 * dibuja por código cuando la imagen oficial no está disponible, por ejemplo
 * sin conexión a internet. Los colores usan las paletas del editor de Miis. */

#define AVATAR_NAME_MAX 32 /* UTF-8, hasta 10 caracteres como los Miis */

#define AVATAR_SKINS     6
#define AVATAR_HAIR_COLORS 8
#define AVATAR_FAVORITES 12

typedef enum {
	HAIR_SHORT,
	HAIR_LONG,
	HAIR_BUN,
	HAIR_SPIKY,
	HAIR_BALD,
	HAIR_BOB,
	HAIR_PONYTAIL,
	HAIR_CURLY,
	HAIR_STYLES
} HairStyle;

typedef enum {
	FACIAL_NONE,
	FACIAL_BEARD,
	FACIAL_MUSTACHE,
	FACIAL_STYLES
} FacialHair;

typedef struct {
	char name[AVATAR_NAME_MAX];
	uint8_t female;
	uint8_t skin;       /* 0..AVATAR_SKINS-1 */
	uint8_t hair_style; /* HairStyle */
	uint8_t hair_color; /* 0..AVATAR_HAIR_COLORS-1 */
	uint8_t favorite;   /* color favorito: camiseta */
	uint8_t glasses;    /* 0 sin gafas */
	uint8_t facial;     /* FacialHair */
	uint8_t long_face;
	uint8_t official; /* mii contiene un Mii de verdad */
	MiiParts mii;
} Avatar;

/* Personajes de la CPU */
typedef enum {
	CPU_PACO,
	CPU_LOLA,
	CPU_MANOLO,
	CPU_CARMEN,
	CPU_PEPE,
	CPU_LUCIA,
	CPU_RAMON, /* la banca de la siete y media */
	CPU_COUNT
} CpuId;

const Avatar* cpu_avatar(CpuId id);
void avatar_default(Avatar* a);
/* Recorta el valor de cada campo a su rango válido. */
void avatar_sanitize(Avatar* a);
/* Ajusta el aspecto simplificado a partir de las piezas del Mii de verdad. */
void avatar_from_mii(Avatar* a, const MiiParts* mii, const char* name);

/* Colores en formato 0xRRGGBB */
uint32_t avatar_skin_rgb(int skin);
uint32_t avatar_hair_rgb(int color);
uint32_t avatar_favorite_rgb(int favorite);
const char* avatar_favorite_name(int favorite);
const char* avatar_hair_style_name(int style);

#endif
