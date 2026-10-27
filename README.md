# Baraja Española para Nintendo 3DS

Juegos de cartas con la baraja española de 40 cartas, hechos como homebrew
para Nintendo 3DS con [devkitPro](https://devkitpro.org) (libctru + citro2d).

| Menú | Siete y media | Resultado | Cinquillo |
|:---:|:---:|:---:|:---:|
| ![Menú](docs/menu.png) | ![Siete y media](docs/sym.png) | ![Resultado](docs/symres.png) | ![Cinquillo](docs/cq.png) |

| Brisca | Escoba | Fin de ronda de la escoba |
|:---:|:---:|:---:|
| ![Brisca](docs/brisca.png) | ![Escoba](docs/escoba.png) | ![Marcador](docs/escoba_fin.png) |

| Chinchón | Cierre del chinchón | Mus | Recuento del mus |
|:---:|:---:|:---:|:---:|
| ![Chinchón](docs/chinchon.png) | ![Cierre](docs/chinchon_fin.png) | ![Mus](docs/mus.png) | ![Recuento](docs/mus_fin.png) |

| Tu perfil Mii | Señas del mus |
|:---:|:---:|
| ![Perfil](docs/perfil.png) | ![Señas](docs/mus_senas.png) |

## Tu Mii y los rivales

La primera vez que abres el juego creas tu **perfil**: puedes usar un **Mii de la
consola** (se abre el selector de Miis del sistema) o diseñar uno aquí mismo
(nombre, sexo, piel, peinado, color de pelo, color favorito, gafas, barba y forma
de la cara). Tu perfil guarda tus estadísticas de cada juego y se puede cambiar
cuando quieras desde **Mi perfil** en el menú. Se guarda en
`/3ds/JuegosDeCartas/perfil.dat` en la tarjeta SD.

Todos los rivales de la CPU son Miis con su propia cara: Don Ramón (la banca de la
siete y media), Paco, Lola y Manolo (cinquillo y mus), Carmen (brisca), Pepe
(escoba) y Lucía (chinchón). Parpadean, se alegran cuando ganan, ponen mala cara
cuando pierden y se sorprenden con una escoba, un chinchón o un órdago.

Los Miis se dibujan con formas sencillas al estilo Mii: de un Mii de la consola se
copian el nombre, el sexo, los colores (piel, pelo y color favorito), las gafas y
la barba o el bigote; los peinados se agrupan en los ocho estilos del juego.

## Juegos

| Juego | Estado |
|---|---|
| Siete y media | ✅ Disponible |
| Cinquillo | ✅ Disponible |
| Brisca | ✅ Disponible |
| Escoba | ✅ Disponible |
| Mus | ✅ Disponible |
| Chinchón | ✅ Disponible |

### Siete y media
Te enfrentas a la banca. Las cartas del 1 al 7 valen su número y las figuras
(sota, caballo y rey) medio punto. Pide cartas hasta acercarte a 7½ sin pasarte.
Los empates son para la banca y la **siete y media real** (un 7 y una figura con
solo dos cartas) cobra el doble. Empiezas con 100 fichas y apuestas entre 5 y 50
en cada mano.

La banca no hace trampas: tu primera carta está boca abajo y solo ve las demás.

### Cinquillo
Partida a cuatro jugadores (tú contra Paco, Lola y Manolo). Se reparten las 40
cartas; sale quien tenga el cinco de oros. En cada turno hay que colocar una carta
si se puede: un cinco abre un palo nuevo y en los palos abiertos se continúa la
escalera hacia abajo (4, 3, 2, 1) o hacia arriba (6, 7, sota, caballo, rey). Si no
puedes jugar, pasas y pagas una ficha al bote. Quien se queda antes sin cartas
gana la ronda y se lleva el bote. La partida dura 10 rondas o hasta que alguien
se arruine.

### Brisca
Partida a dos contra Carmen. Cada uno tiene tres cartas y la muestra (la carta
que queda boca arriba bajo el mazo) marca el palo de triunfo. No es obligatorio
asistir: gana la baza el triunfo más alto o, si no hay, la carta más alta del palo
que salió. El orden es as, tres, rey, caballo, sota, 7, 6, 5, 4 y 2, y los puntos
son as 11, tres 10, rey 4, caballo 3 y sota 2. Tras cada baza roba primero quien
la gana, y sale en la siguiente. Gana quien pase de 60 de los 120 puntos.

Quien tenga el 7 de triunfo puede cambiarlo por la muestra cuando esta es un as,
un tres o una figura; si la muestra es más baja, se cambia con el 2. El cambio se
hace al salir, mientras quede mazo.

### Escoba
Partida a dos contra Pepe. Las cartas valen su número, salvo sota 8, caballo 9 y
rey 10. Hay cuatro cartas en la mesa y tres en cada mano. Si tu carta suma 15 con
una o varias de la mesa, te las llevas (si puede sumar 15, tiene que llevárselas;
si hay varias combinaciones, eliges cuál). Si no suma 15, la carta se queda en la
mesa. Dejar la mesa vacía es una **escoba**. Cuando se acaban las manos se
reparten otras tres hasta agotar el mazo, y lo que queda en la mesa es del último
que recogió. Si al repartir la mesa suma 15 (o 30), el que reparte hace escoba.

Cada ronda da un punto por escoba, por tener más cartas, más oros, más sietes y
por el siete de oros. Gana la partida quien llegue antes a 21.

### Chinchón
Partida a dos contra Lucía, con siete cartas cada uno. En tu turno robas una carta
(del mazo o la de arriba de los descartes) y descartas otra. Se combinan **tríos o
cuartetos** del mismo número y **escaleras** de tres o más cartas seguidas del
mismo palo (1-7, sota, caballo, rey). Tu mano se ordena sola: las combinaciones
van primero, separadas y subrayadas en dorado, y debajo se ve cuánto suman las
sueltas.

Puedes **cerrar** al descartar si te queda todo combinado o una sola carta suelta
de 3 o menos. Entonces cada uno suma sus cartas sueltas (las figuras valen 10),
pero antes el que no cierra puede **ligar** sus sueltas a las combinaciones del
que cierra. Cerrar con todo combinado resta 10 puntos, y cerrar con siete cartas
seguidas del mismo palo es **chinchón**: partida ganada. Pierde quien pase de 100.
Si se acaba el mazo, se barajan los descartes.

### Mus
Por parejas: tú y Lola contra Paco y Manolo, a ocho reyes (los treses cuentan como
reyes y los doses como ases). Cada uno tiene cuatro cartas. Primero se pide **mus**:
si los cuatro lo quieren, cada uno cambia las cartas que elija; en cuanto alguien
dice **no hay mus** se juegan los cuatro lances:

- **Grande**: gana la jugada más alta (rey, caballo, sota, 7... as).
- **Chica**: gana la más baja.
- **Pares**: pareja (1 piedra), medias (trío, 2) y duples (dos parejas, 3). Solo se
  envida si las dos parejas tienen pares.
- **Juego**: 31 o más contando las figuras a 10 (31 vale 3 y el resto 2; el orden es
  31, 32, 40, 37, 36, 35, 34, 33). Si nadie tiene juego se juega al **Punto**.

En cada lance se puede pasar, envidar, subir, querer, no querer o echar un
**órdago**, que decide la partida. Los empates los gana quien está más cerca de la
mano. Lo que no se quiere da el "deje" al que envidó; lo querido y lo que queda en
paso se cuenta al descubrir las cartas, junto con los tantos de pares y juego.
Gana la pareja que llegue a 40 piedras. En la pantalla de abajo siempre ves tu
grande, tus pares y tu juego para no tener que contar.

#### Señas
Como en el mus de verdad, los compañeros se hacen **señas** con la cara, y las
verás en los Miis:

| Seña | Significa |
|---|---|
| Morderse el labio | Dos reyes o más |
| Sacar la punta de la lengua | Dos ases o más |
| Torcer la boca | Medias |
| Levantar las cejas | Duples |
| Guiñar un ojo | 31 de juego |
| Cerrar los ojos | Ciego: nada |

Lola te hace señas y las tiene en cuenta para jugar; pulsa **SELECT** (o toca
**Señas**) para ver la última que te ha hecho y para hacerle tú una a ella. Paco y
Manolo también se hacen señas entre ellos: si estás atento a sus caras, ¡puedes
pillarlas!

## Controles

| Botón | Acción |
|---|---|
| Cruceta / Circle Pad | Moverse por los menús y por tus cartas |
| A | Aceptar, pedir carta, jugar carta |
| B | Plantarse / volver |
| X | Pasar (cinquillo); robar del mazo (chinchón); envidar o marcar cartas para el mus |
| Y | Cambiar el 7 (o el 2) por la muestra (brisca); coger el descarte o cerrar (chinchón); órdago (mus) |
| L / R | Bajar / subir la apuesta (siete y media); elegir qué cartas te llevas (escoba); cuánto envidas (mus) |
| SELECT | Señas con tu compañera (mus) |
| START | Volver al menú (en el menú: salir) |
| Pantalla táctil | Todos los botones y las cartas se pueden tocar (en la escoba y el chinchón, el primer toque elige la carta y el segundo la juega o la descarta) |

## Aspecto "de 3DS"

- Las dos pantallas: la mesa arriba y los controles táctiles abajo.
- **3D estereoscópico**: las cartas en vuelo, los marcadores y los carteles de
  resultado salen de la pantalla al subir el regulador 3D.
- Animaciones de reparto, giro de cartas, bazas que vuelan al montón del
  ganador, carteles de "¡Escoba!", "¡Menos diez!", "¡Chinchón!" y "¡Órdago!", bocadillos con lo
  que dice cada jugador, botones con brillo y fundidos entre pantallas.
- Glifos de botones de la fuente del sistema (Ⓐ, Ⓑ, Ⓧ...) y efectos de sonido.
- Cartas dibujadas a mano por código, con los cortes tradicionales del marco que
  indican el palo (oros sin cortes, copas uno, espadas dos y bastos tres).

## Instalar en la consola

Necesitas una 3DS con [homebrew](https://3ds.hacks.guide/) instalado.

1. Descarga `JuegosDeCartas.3dsx` (de la pestaña *Actions* del repositorio o
   compilándolo tú mismo).
2. Cópialo en la carpeta `/3ds/` de la tarjeta SD.
3. Ábrelo desde el **Homebrew Launcher**.

El sonido necesita el firmware del DSP (`/3ds/dspfirm.cdc`), que se obtiene con
[DSP1](https://github.com/zoogie/DSP1). Sin él el juego funciona igual, pero en
silencio.

También funciona en el emulador [Azahar](https://azahar-emu.org/).

## Compilar

Con devkitPro instalado (paquetes `3ds-dev`):

```sh
make
```

O sin instalar nada, con Docker:

```sh
docker run --rm -v "$PWD":/src -w /src devkitpro/devkitarm make
```

Se genera `JuegosDeCartas.3dsx` en la raíz del proyecto.

### Tests

La lógica de los juegos no depende de la consola y se prueba con el compilador
normal del sistema (incluye simulaciones de cientos de partidas):

```sh
make -C tests
```

### Gráficos y sonidos

Todos los PNG de `gfx/`, el icono y los sonidos de `romfs/sfx/` se generan con
scripts de Python (necesitan [Pillow](https://pypi.org/project/pillow/)):

```sh
python3 tools/gen_assets.py   # cartas, fondos, fichas, logotipo e icono
python3 tools/gen_sounds.py   # efectos de sonido
```

Los ficheros generados están incluidos en el repositorio, así que solo hace falta
ejecutarlos si se cambia su aspecto.

## Estructura del código

```
source/
  core/      Baraja española, aleatorio, Miis y perfil del jugador (C puro)
  games/     Reglas e IA de cada juego (C puro, con tests)
  ui/        Dibujo con citro2d, 3D, Miis, entrada, botones, cartas animadas, sonido
  scenes/    Pantallas: menú, perfil y una mesa por juego
  main.c     Bucle principal y transiciones entre escenas
gfx/         Imágenes fuente (se convierten con tex3ds al compilar)
romfs/sfx/   Sonidos en PCM de 16 bits
tests/       Tests de la lógica
tools/       Generadores de gráficos y sonidos
```

### Añadir un juego nuevo

1. Escribe las reglas en `source/games/<juego>.c/.h` sin usar nada de la 3DS y
   añade tests en `tests/test_main.c`.
2. Crea la mesa en `source/scenes/<juego>_scene.c` siguiendo el modelo de las
   existentes (funciones `enter`, `update`, `draw_top` y `draw_bottom`).
3. Añade la escena a `SceneId` en `source/scenes/scene.h`, a `scene_for()` en
   `source/main.c` y cambia su entrada en `source/scenes/menu.c`.
