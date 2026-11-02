#ifndef UI_UPDATER_H
#define UI_UPDATER_H

#include <stdbool.h>

/* Actualizador: mira en la release "ultima-version" de GitHub si hay una
 * compilación más nueva y la instala (el .cia con el servicio de instalación
 * de la consola, o el .3dsx sustituyendo el fichero). Todo va en un hilo
 * aparte para que la pantalla siga respondiendo. */

typedef enum {
	UPD_IDLE,
	UPD_CHECKING,
	UPD_UP_TO_DATE,
	UPD_AVAILABLE,
	UPD_DOWNLOADING,
	UPD_DONE,       /* instalada: hay que reiniciar */
	UPD_ERROR
} UpdState;

/* Número de esta compilación (0 = compilada a mano, sin número) */
int updater_build(void);
/* Ruta del .3dsx en marcha (argv[0]), si se ha lanzado desde el Homebrew Launcher */
void updater_set_path(const char* argv0);
bool updater_is_cia(void);

/* Empieza a comprobar (en segundo plano) si hay versión nueva */
void updater_check(void);
/* Descarga e instala la versión nueva */
void updater_install(void);
UpdState updater_state(void);
int updater_remote_build(void);
/* Progreso de la descarga: 0..1 (negativo si no se sabe el tamaño) */
float updater_progress(void);
const char* updater_error(void);
/* Espera al hilo y libera la red (al salir del juego) */
void updater_exit(void);

#endif
