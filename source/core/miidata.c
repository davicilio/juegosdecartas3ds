#include "core/miidata.h"

#include <stdio.h>
#include <string.h>

void mii_parts_default(MiiParts* m, bool female)
{
	memset(m, 0, sizeof *m);
	m->gender = female;
	m->height = m->build = 64;
	m->hair_type = female ? 12 : 33;
	m->hair_color = 1;
	m->eye_type = female ? 4 : 2;
	m->eye_scale = 4;
	m->eye_yscale = 3;
	m->eye_rotation = 4;
	m->eye_xspacing = 2;
	m->eye_y = 12;
	m->brow_type = female ? 0 : 6;
	m->brow_color = 1;
	m->brow_scale = 4;
	m->brow_yscale = 3;
	m->brow_rotation = 6;
	m->brow_xspacing = 2;
	m->brow_y = 10;
	m->nose_type = 1;
	m->nose_scale = 4;
	m->nose_y = 9;
	m->mouth_type = 23;
	m->mouth_scale = 4;
	m->mouth_yscale = 3;
	m->mouth_y = 13;
	m->mustache_scale = 4;
	m->mustache_y = 10;
	m->glasses_scale = 4;
	m->glasses_y = 10;
	m->mole_scale = 4;
	m->mole_x = 2;
	m->mole_y = 20;
}

static uint8_t clamp(uint8_t v, uint8_t max)
{
	return v > max ? max : v;
}

void mii_parts_sanitize(MiiParts* m)
{
	m->gender = clamp(m->gender, 1);
	m->favorite_color = clamp(m->favorite_color, 11);
	m->height = clamp(m->height, 127);
	m->build = clamp(m->build, 127);
	m->face_type = clamp(m->face_type, 11);
	m->face_color = clamp(m->face_color, 5);
	m->face_wrinkles = clamp(m->face_wrinkles, 11);
	m->face_makeup = clamp(m->face_makeup, 11);
	m->hair_type = clamp(m->hair_type, 131);
	m->hair_color = clamp(m->hair_color, 7);
	m->hair_flip = clamp(m->hair_flip, 1);
	m->eye_type = clamp(m->eye_type, 59);
	m->eye_color = clamp(m->eye_color, 5);
	m->eye_scale = clamp(m->eye_scale, 7);
	m->eye_yscale = clamp(m->eye_yscale, 6);
	m->eye_rotation = clamp(m->eye_rotation, 7);
	m->eye_xspacing = clamp(m->eye_xspacing, 12);
	m->eye_y = clamp(m->eye_y, 18);
	m->brow_type = clamp(m->brow_type, 24);
	m->brow_color = clamp(m->brow_color, 7);
	m->brow_scale = clamp(m->brow_scale, 8);
	m->brow_yscale = clamp(m->brow_yscale, 6);
	m->brow_rotation = clamp(m->brow_rotation, 11);
	m->brow_xspacing = clamp(m->brow_xspacing, 12);
	m->brow_y = clamp(m->brow_y, 18);
	m->nose_type = clamp(m->nose_type, 17);
	m->nose_scale = clamp(m->nose_scale, 8);
	m->nose_y = clamp(m->nose_y, 18);
	m->mouth_type = clamp(m->mouth_type, 35);
	m->mouth_color = clamp(m->mouth_color, 4);
	m->mouth_scale = clamp(m->mouth_scale, 8);
	m->mouth_yscale = clamp(m->mouth_yscale, 6);
	m->mouth_y = clamp(m->mouth_y, 18);
	m->mustache_type = clamp(m->mustache_type, 5);
	m->beard_type = clamp(m->beard_type, 5);
	m->facial_hair_color = clamp(m->facial_hair_color, 7);
	m->mustache_scale = clamp(m->mustache_scale, 8);
	m->mustache_y = clamp(m->mustache_y, 16);
	m->glasses_type = clamp(m->glasses_type, 8);
	m->glasses_color = clamp(m->glasses_color, 5);
	m->glasses_scale = clamp(m->glasses_scale, 7);
	m->glasses_y = clamp(m->glasses_y, 20);
	m->mole_enabled = clamp(m->mole_enabled, 1);
	m->mole_scale = clamp(m->mole_scale, 8);
	m->mole_x = clamp(m->mole_x, 16);
	m->mole_y = clamp(m->mole_y, 30);
}

uint32_t mii_parts_hash(const MiiParts* m)
{
	/* FNV-1a sobre todas las piezas (el struct solo tiene bytes) */
	const uint8_t* data = (const uint8_t*)m;
	uint32_t h = 2166136261u;
	for (size_t i = 0; i < sizeof *m; i++) {
		h ^= data[i];
		h *= 16777619u;
	}
	return h;
}
