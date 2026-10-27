#ifndef UI_MIIFACE_H
#define UI_MIIFACE_H

#include <citro2d.h>
#include <stdbool.h>

#include "core/miidata.h"

/* Caras oficiales de los Miis. Las dibuja el renderizador de Mii Studio de
 * Nintendo, que hace falta descargar una vez por Mii y expresión; después
 * quedan guardadas en la SD (sdmc:/3ds/JuegosDeCartas/miis) y ya no se
 * necesita conexión. Las descargas van en un hilo aparte (curl con mbedTLS),
 * comprobando el certificado del servidor con los certificados raíz de
 * Mozilla que lleva el juego. */

void miiface_init(void);
void miiface_exit(void);
/* Llamar una vez por fotograma (limita las cargas de imágenes por fotograma). */
void miiface_frame(void);
/* Imagen de la cara con la expresión de Mii Studio indicada ("normal",
 * "smile"...), o NULL si todavía no está. Si download es true y no está en
 * la SD, se pide al servidor. */
const C2D_Image* miiface_get(const MiiParts* mii, const char* expression, bool download);
/* Vuelve a intentar las caras que fallaron (por ejemplo, sin conexión). */
void miiface_retry(void);
/* Caras pendientes de descargar, para mostrar el progreso. */
int miiface_pending(void);
/* Motivo del último fallo de descarga ("" si no ha fallado). */
const char* miiface_last_error(void);
/* false si no hay red o han fallado varias descargas seguidas. */
bool miiface_online(void);

#endif
