#ifndef CORE_MIIDATA_H
#define CORE_MIIDATA_H

#include <stdbool.h>
#include <stdint.h>

/* Datos de un Mii de verdad: todas las piezas del editor de Miis de Nintendo
 * (el mismo formato de 3DS y Wii U). Con ellos se pide al renderizador
 * oficial de Mii Studio la imagen de la cara del Mii. */

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

#define MII_STUDIO_SIZE 46
/* Código hexadecimal para la URL de Mii Studio: 47 bytes -> 94 caracteres */
#define MII_STUDIO_HEX  (2 * (MII_STUDIO_SIZE + 1) + 1)

/* Mii nuevo del editor, con las piezas por defecto. */
void mii_parts_default(MiiParts* m, bool female);
/* Recorta cada pieza a su rango válido. */
void mii_parts_sanitize(MiiParts* m);
/* Datos de Mii Studio sin ofuscar, en el orden que espera el servicio. */
void mii_studio_data(const MiiParts* m, uint8_t out[MII_STUDIO_SIZE]);
/* Código para la URL: un byte inicial y cada byte ofuscado con el anterior. */
void mii_studio_code(const MiiParts* m, uint8_t seed, char out[MII_STUDIO_HEX]);
/* Deshace la ofuscación de un código (para comprobarlo). */
bool mii_studio_decode(const char* hex, uint8_t out[MII_STUDIO_SIZE]);
/* Huella para nombrar la caché de imágenes. */
uint32_t mii_parts_hash(const MiiParts* m);

#endif
