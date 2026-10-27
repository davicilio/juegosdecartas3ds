#ifndef CORE_MIIDATA_H
#define CORE_MIIDATA_H

#include <stdbool.h>
#include <stdint.h>

/* Datos de un Mii de verdad: todas las piezas del editor de Miis de Nintendo
 * (el mismo formato de 3DS y Wii U). Los personajes del juego toman de
 * ellos su aspecto: piel, pelo, color favorito, gafas, barba... */

typedef struct {
	uint8_t gender; /* 0 hombre, 1 mujer */
	uint8_t favorite_color;
	uint8_t height, build;
	uint8_t face_type, face_color, face_wrinkles, face_makeup;
	uint8_t hair_type, hair_color, hair_flip;
	uint8_t eye_type, eye_color, eye_scale, eye_yscale, eye_rotation, eye_xspacing, eye_y;
	uint8_t brow_type, brow_color, brow_scale, brow_yscale, brow_rotation, brow_xspacing, brow_y;
	uint8_t nose_type, nose_scale, nose_y;
	uint8_t mouth_type, mouth_color, mouth_scale, mouth_yscale, mouth_y;
	uint8_t mustache_type, beard_type, facial_hair_color, mustache_scale, mustache_y;
	uint8_t glasses_type, glasses_color, glasses_scale, glasses_y;
	uint8_t mole_enabled, mole_scale, mole_x, mole_y;
} MiiParts;

/* Mii nuevo del editor, con las piezas por defecto. */
void mii_parts_default(MiiParts* m, bool female);
/* Recorta cada pieza a su rango válido. */
void mii_parts_sanitize(MiiParts* m);
/* Huella para distinguir Miis. */
uint32_t mii_parts_hash(const MiiParts* m);

#endif
