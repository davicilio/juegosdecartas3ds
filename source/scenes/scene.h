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
	SCENE_GUINOTE,
	SCENE_TUTE,
	SCENE_TUTE_CABRON,
	SCENE_TUTE_SUBASTADO,
	SCENE_UNO,
	SCENE_POKER,
	SCENE_BLACKJACK,
	SCENE_CORAZONES,
	SCENE_POCHA,
	SCENE_CONECTA4,
	SCENE_DAMAS,
	SCENE_DOMINO,
	SCENE_AS23,
	SCENE_PUMBA,
	SCENE_JULEPE,
	SCENE_TRUCO,
	SCENE_TRUC,
	SCENE_BUTIFARRA,
	SCENE_REVERSI,
	SCENE_OCA,
	SCENE_BURRO,
	SCENE_MIERDA,
	SCENE_TRESILLO,
	SCENE_CUATRILLO,
	SCENE_MONA,
	SCENE_CUCU,
	SCENE_TREINTAYUNA,
	SCENE_MENTIROSO,
	SCENE_CULO,
	SCENE_RUMMIKUB,
	SCENE_SOLITARIO,
	SCENE_PIRAMIDE,
	SCENE_PROFILE,
	SCENE_RIVALS,
	SCENE_CARDBACK,
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
extern const Scene scene_guinote;
extern const Scene scene_tute;
extern const Scene scene_tute_cabron;
extern const Scene scene_tute_subastado;
extern const Scene scene_uno;
extern const Scene scene_poker;
extern const Scene scene_blackjack;
extern const Scene scene_corazones;
extern const Scene scene_pocha;
extern const Scene scene_conecta4;
extern const Scene scene_damas;
extern const Scene scene_domino;
extern const Scene scene_as23;
extern const Scene scene_pumba;
extern const Scene scene_julepe;
extern const Scene scene_truco;
extern const Scene scene_truc;
extern const Scene scene_butifarra;
extern const Scene scene_reversi;
extern const Scene scene_oca;
extern const Scene scene_burro;
extern const Scene scene_mierda;
extern const Scene scene_tresillo;
extern const Scene scene_cuatrillo;
extern const Scene scene_mona;
extern const Scene scene_cucu;
extern const Scene scene_treintayuna;
extern const Scene scene_mentiroso;
extern const Scene scene_culo;
extern const Scene scene_rummikub;
extern const Scene scene_solitario;
extern const Scene scene_piramide;
extern const Scene scene_profile;
extern const Scene scene_rivals;
extern const Scene scene_cardback;

/* Carga el dorso de las cartas guardado en la SD. */
void cardback_editor_init(void);

#endif
