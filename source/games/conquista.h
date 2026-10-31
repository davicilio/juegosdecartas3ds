#ifndef GAMES_CONQUISTA_H
#define GAMES_CONQUISTA_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

/* Conquista: juego de estrategia por las 50 provincias de España, de 2 a 4
 * jugadores (0 es el humano).
 *
 * - Al empezar se reparten las provincias al azar, con una tropa en cada
 *   una, y el resto de tropas iniciales se colocan solas.
 * - Turno: 1) Refuerzos: un tercio de tus provincias (como poco 3) más un
 *   extra por cada comunidad autónoma entera que tengas. 2) Ataques desde
 *   una provincia con al menos dos tropas a una vecina enemiga, tantas
 *   veces como quieras. 3) Un movimiento de tropas entre dos provincias
 *   tuyas vecinas.
 * - Batalla: cada ronda, atacante y defensor tiran un dado; gana el más
 *   alto y el empate es para el defensor. El atacante suma 1 si lleva más
 *   tropas que el defensor. Quien pierde la ronda pierde una tropa. Al
 *   conquistar, pasa a la provincia la mitad de las tropas atacantes.
 * - Gana quien conquista toda España o, tras CN_MAX_ROUNDS vueltas, quien
 *   tenga más provincias. Si te quedas sin provincias, pierdes. */

#define CN_PROVINCES  50
#define CN_REGIONS    17
#define CN_PLAYERS    4
#define CN_MAX_ADJ    8
#define CN_MAX_ROUNDS 40

typedef struct {
	const char* name;
	uint8_t region;
	int8_t adj[CN_MAX_ADJ]; /* -1 al final */
	float lon, lat;         /* posición en el mapa (Canarias va en un recuadro) */
} CnProvince;

typedef struct {
	const char* name;
	int bonus;
} CnRegion;

extern const CnProvince CN_MAP[CN_PROVINCES];
extern const CnRegion CN_REGION[CN_REGIONS];

bool cn_adjacent(int a, int b);

typedef enum { CN_REINFORCE, CN_ATTACK, CN_FORTIFY, CN_OVER } CnPhase;

typedef struct {
	int players;
	int owner[CN_PROVINCES];
	int troops[CN_PROVINCES];
	int current;
	int round;
	CnPhase phase;
	int to_place;        /* refuerzos que quedan por colocar */
	bool out[CN_PLAYERS];
	int winner;
	/* Última ronda de batalla, para la interfaz */
	int die_a, die_d;
	bool attacker_won_round;
	bool conquered;      /* la última batalla acabó en conquista */
	Rng rng;
} Conquista;

void cn_init(Conquista* g, int players, uint32_t seed);
int cn_provinces_of(const Conquista* g, int p);
int cn_reinforcements(const Conquista* g, int p);
bool cn_region_owned(const Conquista* g, int region, int p);

/* Refuerzos: pone una tropa en la provincia (que tiene que ser suya) */
bool cn_place(Conquista* g, int prov, int n);
bool cn_can_attack(const Conquista* g, int from, int to);
/* Una ronda de batalla. Devuelve true si se ha jugado. */
bool cn_attack_round(Conquista* g, int from, int to);
/* Ataca hasta conquistar o quedarse con una tropa */
bool cn_attack_all(Conquista* g, int from, int to);
void cn_end_attacks(Conquista* g);
bool cn_can_move(const Conquista* g, int from, int to);
/* Mueve n tropas en la fase de movimiento (y termina el turno) */
bool cn_move(Conquista* g, int from, int to, int n);
void cn_end_turn(Conquista* g);

/* IA: hace su fase actual paso a paso. Devuelve false cuando ha terminado
 * el turno (o la partida). */
bool cn_ai_step(Conquista* g);

#endif
