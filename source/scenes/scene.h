#ifndef SCENES_SCENE_H
#define SCENES_SCENE_H

#include "ui/input.h"

typedef enum {
	SCENE_MENU,
	SCENE_SIETEYMEDIA,
	SCENE_CINQUILLO,
	SCENE_BRISCA,
	SCENE_ESCOBA,
	SCENE_CHINCHON,
	SCENE_MUS,
	SCENE_COUNT,
	SCENE_QUIT
} SceneId;

typedef struct {
	void (*enter)(void);
	/* Devuelve la escena a la que hay que cambiar (o la propia). */
	SceneId (*update)(const Input* in, float dt);
	void (*draw_top)(void);
	void (*draw_bottom)(void);
} Scene;

extern const Scene scene_menu;
extern const Scene scene_sieteymedia;
extern const Scene scene_cinquillo;
extern const Scene scene_brisca;
extern const Scene scene_escoba;
extern const Scene scene_chinchon;
extern const Scene scene_mus;

#endif
