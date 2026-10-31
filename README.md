# Baraja Española para Nintendo 3DS

**50 juegos** de cartas y de mesa para Nintendo 3DS, en español: siete y media, mus, chinchón,
tute, brisca, póker, blackjack, parchís, bingo, mahjong, un juego de comprar calles con tableros
personalizables y muchos más. Juegas contra personajes
basados en los Miis de tu consola, con fichas comunes para todos los juegos de apuestas (que se
compran con las monedas de juego que ganas andando), 3D estereoscópico y pantalla táctil.

| Menú | Mus | Póker | Bingo |
|:---:|:---:|:---:|:---:|
| ![Menú](docs/menu.png) | ![Mus](docs/mus.png) | ![Póker](docs/poker.png) | ![Bingo](docs/bingo.png) |

**[Descargar la última versión](https://github.com/davicilio/juegosdecartas3ds/releases/tag/ultima-version)**
(`JuegosDeCartas.cia` o `JuegosDeCartas.3dsx`)

## Instalar en la consola

Necesitas una 3DS con [homebrew](https://3ds.hacks.guide/) instalado (Luma3DS y
FBI).

### Como aplicación del menú HOME (.cia, recomendado)

1. Descarga `JuegosDeCartas.cia` de la release
   [**ultima-version**](https://github.com/davicilio/juegosdecartas3ds/releases/tag/ultima-version)
   (se actualiza sola con cada cambio).
2. Cópialo a la tarjeta SD, por ejemplo a `/cias/`.
3. En la consola abre **FBI → SD → cias → JuegosDeCartas.cia → Install CIA**.
4. El juego aparece en el menú HOME con su icono y su banner ("Baraja Española").

Para actualizarlo basta con instalar el `.cia` nuevo encima.

#### Con un QR

En **FBI → Remote Install → Scan QR Code**, escanea este código (solo funciona si el
repositorio es público):

<img src="docs/qr_cia.png" alt="QR del .cia" width="140">

### Desde el Homebrew Launcher (.3dsx)

1. Copia `JuegosDeCartas.3dsx` a la carpeta `/3ds/` de la tarjeta SD.
2. Ábrelo desde el **Homebrew Launcher**.

El sonido necesita el firmware del DSP (`/3ds/dspfirm.cdc`), que se obtiene con
[DSP1](https://github.com/zoogie/DSP1). Sin él el juego funciona igual, pero en
silencio.

También funciona en el emulador [Azahar](https://azahar-emu.org/).

## Juegos

- **Baraja española:** siete y media, cinquillo, brisca, escoba, mus, chinchón, guiñote, tute,
  tute cabrón, tute subastado, julepe, pocha, la mona, cucú, treinta y una, mentiroso, culo,
  mierda, burro, as 2 3, pumba, tresillo, cuatrillo, truc, truco argentino, butifarra,
  solitario tradicional y solitario en pirámide.
- **Baraja francesa:** póker Texas Hold'em, Omaha, póker de descarte, videopóker, blackjack,
  corazones y carta blanca.
- **De mesa:** parchís, la oca, bingo, dominó, damas, reversi, conecta 4, tres en raya, hundir
  la flota, toma 6, mahjong solitario, mahjong riichi, Uno, Rummikub y Negocio redondo (compraventa de calles con
  tableros que puedes crear tú).

## Más información

| | |
|---|---|
| 📖 [Reglas de los juegos](docs/JUEGOS.md) | Cómo se juega a cada uno |
| ⚙️ [Funciones](docs/FUNCIONES.md) | Fichas y monedas de juego, pausa, personajes, dorso, música, controles |
| 🖼️ [Capturas](docs/CAPTURAS.md) | Imágenes de todos los juegos |
| 🛠️ [Desarrollo](docs/DESARROLLO.md) | Compilar, tests y estructura del código |
