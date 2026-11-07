# Funciones

[← Volver al README](../README.md)

- [Actualizar el juego](#actualizar-el-juego)
- [Fichas y monedas de juego](#fichas-y-monedas-de-juego)
- [Pausa](#pausa)
- [Los personajes](#los-personajes-basados-en-los-miis-de-tu-consola)
- [Editor del dorso de las cartas](#editor-del-dorso-de-las-cartas)
- [Música del menú](#música-del-menú)
- [Controles](#controles)
- [Aspecto "de 3DS"](#aspecto-de-3ds)

## Actualizar el juego

Al arrancar, el juego mira por internet (sin parar nada) si hay una versión nueva publicada en la
release [ultima-version](https://github.com/davicilio/juegosdecartas3ds/releases/tag/ultima-version).
Si la hay, en el menú aparece un aviso y un punto rojo en **Configuración → Versión**. Ahí se ve
tu versión y la publicada, y con **Instalar** se descarga e instala sola:

- Si lo tienes instalado como aplicación (**.cia**), se instala encima con el servicio de
  instalación de la consola, como hace FBI.
- Si lo abres desde el Homebrew Launcher (**.3dsx**), se sustituye el fichero.

Luego basta con salir y volver a abrir el juego. El perfil, las fichas, los tableros y los récords
se conservan. Las descargas van cifradas y se comprueba el certificado de GitHub. Para que funcione
el repositorio tiene que ser público y la consola, tener internet.

## Fichas y monedas de juego

Todos los juegos con apuestas (siete y media, cinquillo, julepe, póker, Omaha, póker de descarte,
bingo y todo el casino) usan **las mismas fichas**, que se guardan en tu perfil. Empiezas
con 2000. Al sentarte a una mesa coges fichas del monedero (100 en la siete y media, 1000 en el
póker...) y al acabar la partida vuelven las que te queden, con lo que hayas ganado. En los juegos
del **Casino** (ruleta, tragaperras, punto y banca, dados, keno y rueda de la fortuna) cada
apuesta sale directamente del monedero y los premios vuelven a él; si sales a media jugada, la
apuesta se anula y se devuelve.

Si te quedas sin fichas, en **Configuración → Fichas** las cambias por las **monedas de juego**
de la consola, las que se ganan andando con la 3DS en el bolsillo: cada moneda vale 100 fichas.

## Pausa

En cualquier juego, el botón **Menú** (o START) pausa la partida:

- **Reanudar** sigue donde estabas.
- **Reiniciar** da la partida por acabada y empieza otra. En los juegos que tienen pantalla de antes
  de jugar (jugadores, dificultad...), vuelve a ella para elegir de nuevo.
- **Salir** da la partida por acabada, sin guardarla, y vuelve al menú.

Al reiniciar o salir, las fichas que tenías en la mesa vuelven al monedero (también las apostadas
en una mano sin terminar, que se anula).

Lo que se elige antes de empezar (por ejemplo, los jugadores de la oca o la dificultad del conecta
4, el reversi, las damas y el tres en raya) ya no se puede cambiar a mitad de partida: para eso
está Reiniciar.

## Los personajes: basados en los Miis de tu consola

Cada jugador es un **personaje dibujado al estilo de Animal Crossing** (cabeza
grande y redonda, ojos sencillos y coloretes) que toma su aspecto de un **Mii de
tu consola**: el nombre, el sexo, el color de piel, el color de pelo, un peinado
parecido, el color favorito (la camiseta), las gafas y la barba o el bigote.
No hace falta conexión a internet: todo se dibuja en la consola.

- **Tu personaje**: la primera vez eliges tu Mii con el **selector de Miis del
  sistema** (la pantalla de Nintendo que usan los juegos para elegir un Mii).
  Puedes elegir uno de tus Miis o uno de los **Miis invitados**.
- **Tus rivales**: al arrancar, el juego lee la lista de Miis de la consola (la base
  de datos del editor de Miis, `CFL_DB.dat`, en los datos compartidos del sistema)
  y los reparte entre los puestos de la CPU: la banca de la siete y media, los tres
  de la mesa de cinquillo y mus, y los rivales de brisca, escoba y chinchón. Cada
  rival se llama como su Mii. Tu propio Mii nunca hace de rival.
- En **Mi perfil → Tus rivales** ves quién ocupa cada puesto y puedes cambiar
  cualquiera con el selector de Miis, también por Miis invitados. Con **Miis de la
  consola** vuelves al reparto automático.
- Si la consola no tiene Miis (o no se puede leer la lista), se usan unos
  personajes de reserva hasta que elijas los tuyos o Miis invitados con el selector.

Las piezas de los personajes (cabezas, peinados, ojos, bocas, ropa...) las genera
`tools/gen_characters.py` en gris, y el juego las tiñe con los colores de cada Mii.

El perfil guarda tus estadísticas en `/3ds/JuegosDeCartas/perfil.dat` y los
rivales que elijas a mano en `/3ds/JuegosDeCartas/rivales.dat`.

Los personajes parpadean, se alegran cuando ganan, se entristecen cuando pierden y
se sorprenden con una escoba, un chinchón o un órdago.

## Editor del dorso de las cartas

En **Configuración → Dorso de las cartas** (en el menú) diseñas la parte de atrás de las cartas,
con una vista previa grande en la pantalla de arriba:

- **Fondo**: 14 colores.
- **Dibujo**: liso, rombos, rayas, lunares, cuadros, ondas o estrellas, y su color
  (automático: un tono más claro que el fondo).
- **Marco**: dorado, plateado, blanco, negro o sin marco.
- **Emblema** del centro: los cuatro palos, un oro, una copa, una corona, una
  estrella o ninguno.
- **Texto**: lo escribes con el teclado de la consola (hasta 16 caracteres, por
  ejemplo *Caja Madrid* o el nombre de tu peña), con su color y su posición
  (arriba, en el centro o abajo).

Con la cruceta eliges la opción y la cambias con ◀ ▶ (o tocando las flechas); el
texto se escribe con Ⓐ. **Guardar** lo deja en `/3ds/JuegosDeCartas/dorso.dat` y
se usa en todos los juegos; **Por defecto** recupera el dorso granate de siempre.
El juego compone el dorso en una textura con las capas que genera
`tools/gen_backs.py`.

## Música del menú
En el menú suena en bucle el pasodoble **Suspiros de España** (Antonio Álvarez Alonso, 1902);
al entrar en un juego se para y vuelve al salir.

Puedes poner otra: copia un fichero `musica.wav` en la carpeta `/3ds/JuegosDeCartas/` de la
tarjeta SD y sonará esa en su lugar. Tiene que ser un WAV PCM de 16 bits, mono o estéreo, de
8000 a 48000 Hz; con Audacity, por ejemplo: *Archivo → Exportar → Exportar como WAV* con "PCM
de 16 bits con signo".

### Música de los juegos de cartas
Los juegos de cartas pueden tener su propia música en bucle: copia un fichero `juegos.wav`
(con el mismo formato) en `/3ds/JuegosDeCartas/`. Suena en todos los juegos de cartas (no en
los de tablero, fichas o bolas), sigue sonando durante la pausa y se para al volver al menú. No
va dentro del juego: la pones tú.

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
| START | Pausa la partida: reanudar o salir (en el menú: salir de la aplicación) |
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
