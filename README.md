# Baraja Española para Nintendo 3DS

Juegos de cartas con la baraja española de 40 cartas, hechos como homebrew
para Nintendo 3DS con [devkitPro](https://devkitpro.org) (libctru + citro2d).

| Menú | Siete y media | Resultado | Cinquillo |
|:---:|:---:|:---:|:---:|
| ![Menú](docs/menu.png) | ![Siete y media](docs/sym.png) | ![Resultado](docs/symres.png) | ![Cinquillo](docs/cq.png) |

## Juegos

| Juego | Estado |
|---|---|
| Siete y media | ✅ Disponible |
| Cinquillo | ✅ Disponible |
| Brisca | 🔜 Próximamente |
| Mus | 🔜 Próximamente |
| Chinchón | 🔜 Próximamente |

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

## Controles

| Botón | Acción |
|---|---|
| Cruceta / Circle Pad | Moverse por los menús y por tus cartas |
| A | Aceptar, pedir carta, jugar carta |
| B | Plantarse / volver |
| X | Pasar (cinquillo) |
| L / R | Bajar / subir la apuesta (siete y media) |
| START | Volver al menú (en el menú: salir) |
| Pantalla táctil | Todos los botones y las cartas se pueden tocar |

## Aspecto "de 3DS"

- Las dos pantallas: la mesa arriba y los controles táctiles abajo.
- **3D estereoscópico**: las cartas en vuelo, los marcadores y los carteles de
  resultado salen de la pantalla al subir el regulador 3D.
- Animaciones de reparto, giro de cartas, botones con brillo y fundidos entre
  pantallas.
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
  core/      Baraja española y generador aleatorio (C puro)
  games/     Reglas e IA de cada juego (C puro, con tests)
  ui/        Dibujo con citro2d, 3D, entrada, botones, cartas animadas, sonido
  scenes/    Pantallas: menú y una mesa por juego
  main.c     Bucle principal y transiciones entre escenas
gfx/         Imágenes fuente (se convierten con tex3ds al compilar)
romfs/sfx/   Sonidos en PCM de 16 bits
tests/       Tests de la lógica
tools/       Generadores de gráficos y sonidos
```

### Añadir un juego nuevo (brisca, mus, chinchón...)

1. Escribe las reglas en `source/games/<juego>.c/.h` sin usar nada de la 3DS y
   añade tests en `tests/test_main.c`.
2. Crea la mesa en `source/scenes/<juego>_scene.c` siguiendo el modelo de las
   existentes (funciones `enter`, `update`, `draw_top` y `draw_bottom`).
3. Añade la escena a `SceneId` en `source/scenes/scene.h`, a `scene_for()` en
   `source/main.c` y cambia su entrada en `source/scenes/menu.c`.
