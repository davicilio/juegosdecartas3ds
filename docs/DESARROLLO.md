# Desarrollo

[← Volver al README](../README.md)

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

Para crear también el `.cia` hacen falta
[makerom](https://github.com/3DSGuy/Project_CTR) y
[bannertool](https://github.com/carstene1ns/3ds-bannertool):

```sh
MAKEROM=/ruta/makerom BANNERTOOL=/ruta/bannertool tools/make_cia.sh
```

El banner y la melodía del menú HOME están en `cia/` (se generan con los scripts
de `tools/`) y la configuración del `.cia` en `cia/app.rsf`.

El QR de instalación se regenera con `python3 tools/gen_qr.py URL docs/qr_cia.png`.

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
python3 tools/gen_assets.py      # cartas, fondos, fichas, logotipo e icono
python3 tools/gen_characters.py  # piezas de los personajes
python3 tools/gen_backs.py       # capas del dorso de las cartas
python3 tools/gen_board.py       # cartas del Uno y fichas del Rummikub
python3 tools/gen_french.py      # baraja francesa (póker)
python3 tools/gen_sounds.py      # efectos de sonido
```

Los ficheros generados están incluidos en el repositorio, así que solo hace falta
ejecutarlos si se cambia su aspecto.

## Estructura del código

```
source/
  core/      Baraja española, aleatorio, Miis, perfil y dorso de las cartas (C puro)
  games/     Reglas e IA de cada juego (C puro, con tests)
  ui/        Dibujo con citro2d, 3D, personajes, entrada, botones, cartas animadas, sonido
  scenes/    Pantallas: menú, perfil, rivales, editor del dorso y una mesa por juego
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
