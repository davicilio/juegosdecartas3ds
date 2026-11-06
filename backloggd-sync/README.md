# Sincronizador de sesiones de juego → Backloggd

Lee cuánto juegas en **Steam**, **Epic Games**, **PlayStation (PS5 y PS4)** y **Nintendo
Switch / Switch 2**, y lo apunta en [Backloggd](https://backloggd.com) como **sesiones de
juego** (el diario/journal de cada juego): una sesión por juego y día, con las horas y los
minutos jugados. Si vuelves a jugar el mismo día, actualiza esa sesión; no crea otra.

Está escrito en Python sin dependencias (solo la biblioteca estándar, Python 3.9 o más
nuevo). Se puede ejecutar en tu ordenador o, sin tener nada encendido, con GitHub Actions
cada hora.

> **Aviso.** Backloggd, Epic, PlayStation y Nintendo no tienen API pública para esto. El
> programa usa los mismos endpoints que sus webs y apps oficiales (los de Backloggd, como
> los documenta [backloggd-mcp](https://github.com/DenisionSoft/backloggd-mcp)). Pueden
> cambiar sin aviso y romper alguna plataforma. Úsalo solo con tus propias cuentas.

## Cómo funciona

| Plataforma | De dónde sale el tiempo | Fecha de la sesión |
|---|---|---|
| Steam | Tiempo total de cada juego (API web oficial) | Día de la última partida |
| PlayStation | Tiempo total de cada juego (lista de juegos de PSN) | Día de la última partida |
| Epic Games | Tiempo total de cada juego (servicio de biblioteca) | Día en que se detecta |
| Nintendo | Minutos de cada día y juego (app de Control Parental) | El día exacto |

- Steam, PlayStation y Epic dan el **total acumulado**: el programa lo compara con la
  lectura anterior y la diferencia es lo que has jugado desde entonces. Por eso la
  **primera ejecución solo guarda el punto de partida** y no importa tu historial.
- Nintendo da los **minutos de cada día**. También empieza a contar desde el día de la
  primera ejecución.
- Cada juego se busca en Backloggd por su título. Si hay una coincidencia exacta y única,
  se asocia solo. Si no (títulos repetidos como *DOOM*, nombres distintos), queda
  **pendiente** y se asocia a mano una vez; el tiempo espera hasta entonces.
- La sesión se escribe en la última partida (*playthrough*) del juego. Si el juego no
  estaba en tu biblioteca, se añade como **Jugando** y se crea la partida. La primera
  sesión se marca como inicio; si pones el juego como *Played / Completed / Retired*, la
  última sesión se marca como final en la siguiente ejecución.
- Las sesiones que hayas puesto a mano no se tocan. Si un juego aún no tiene plataforma
  en Backloggd, se pone la suya (PC, PS5, PS4, Switch o Switch 2).
- El mismo juego jugado el mismo día en dos plataformas da **una** sesión con la suma.
- No se envía nada hasta que un juego suma 5 minutos en el día (configurable).

Steam, PlayStation y Epic actualizan el tiempo cuando cierras el juego, y la consola de
Nintendo tiene que conectarse a internet para enviar su resumen; la sesión aparece en la
siguiente ejecución después de eso.

## Instalación

```sh
cd backloggd-sync
cp config.ejemplo.env .env      # y rellénalo
python3 sincronizar.py --simular   # prueba sin escribir nada
python3 sincronizar.py             # sincroniza
```

`.env` y `estado.json` contienen contraseñas y tokens: están en `.gitignore`, no los
subas ni los compartas.

### Backloggd (obligatorio)

1. Inicia sesión en backloggd.com en el navegador.
2. Abre las herramientas de desarrollo (F12) → **Aplicación** (Chrome) o **Almacenamiento**
   (Firefox) → **Cookies** → `https://backloggd.com`.
3. Copia el valor de `_backloggd_session` en `BACKLOGGD_COOKIE`.

La cookie se renueva sola y se guarda en `estado.json`. Si además pones
`BACKLOGGD_USUARIO` y `BACKLOGGD_CONTRASENA`, el programa vuelve a iniciar sesión solo
cuando caduque.

### Steam

1. Crea una clave en <https://steamcommunity.com/dev/apikey> → `STEAM_API_KEY`.
2. Tu SteamID64 (17 cifras, por ejemplo en <https://steamid.io>) → `STEAM_ID`.
3. En Steam: **Perfil → Editar perfil → Privacidad → Detalles de los juegos: Público**.

### Epic Games

```sh
python3 sincronizar.py login-epic
```

Te da un enlace; inicia sesión y copia el `authorizationCode` que aparece. El token se
guarda y se renueva en cada ejecución (si pasan unas semanas sin ejecutarlo, caduca y hay
que repetir este paso). Epic no dice cuándo jugaste por última vez: el tiempo se apunta
el día en que se detecta, así que conviene ejecutarlo a menudo.

### PlayStation (PS5 y PS4)

1. Inicia sesión en <https://www.playstation.com>.
2. En la misma pestaña abre <https://ca.account.sony.com/api/v1/ssocookie> y copia el
   valor de `npsso` en `PSN_NPSSO` (o ejecuta `python3 sincronizar.py login-psn`).

El token dura unos dos meses; después hay que poner un NPSSO nuevo.

### Nintendo Switch / Switch 2

Nintendo solo da el tiempo de juego por día a través del **Control Parental**:

1. Instala la app **Control Parental de Nintendo Switch** en el móvil y vincula tu consola
   (no hace falta poner ninguna restricción).
2. Ejecuta:

   ```sh
   python3 sincronizar.py login-nintendo
   ```

   Abre el enlace en el ordenador, inicia sesión y, en el botón rojo **«Elegir a esta
   persona»**, haz clic derecho → **Copiar dirección del enlace** y pégalo.
3. Si en la consola juega más gente, pon tu apodo en `NINTENDO_JUGADOR` para contar solo
   tus partidas.

El token dura unos dos años.

## Juegos que no se asocian solos

```sh
python3 sincronizar.py pendientes                 # lista y candidatos
python3 sincronizar.py buscar doom                # buscar en Backloggd
python3 sincronizar.py asociar steam:379720 doom--1   # clave + slug (o URL) de Backloggd
python3 sincronizar.py ignorar epic:Fortnite      # no enviar nunca ese juego
python3 sincronizar.py estado                     # resumen y registro de ejecuciones
```

El slug es la última parte de la dirección del juego: `backloggd.com/games/doom--1/`.
El tiempo que estaba esperando se envía en la siguiente ejecución.

También se pueden fijar en `asociaciones.json` (junto al script), que es la forma de
hacerlo cuando se usa GitHub Actions. Se aplica en cada ejecución:

```json
{
  "steam:379720": "doom--1",
  "epic:Fortnite": "ignorar"
}
```

## Ejecutarlo automáticamente

### En tu ordenador

- **Linux / macOS** (`crontab -e`), cada hora:

  ```
  17 * * * * cd /ruta/a/backloggd-sync && python3 sincronizar.py >> sync.log 2>&1
  ```

- **Windows** (en una consola):

  ```
  schtasks /create /sc hourly /tn "Backloggd sync" /tr "py C:\ruta\a\backloggd-sync\sincronizar.py"
  ```

### Con GitHub Actions (sin el ordenador encendido)

El workflow `.github/workflows/backloggd-sync.yml` lo ejecuta cada hora. El estado se
guarda **cifrado** en la caché de Actions entre ejecuciones.

1. Haz primero en tu ordenador `login-epic` y `login-nintendo` (si los usas): imprimen el
   token que hay que copiar.
2. En el repositorio: **Settings → Secrets and variables → Actions**.
   - **Secrets**: `CLAVE_ESTADO` (una contraseña larga inventada, para cifrar el estado),
     `BACKLOGGD_COOKIE` (y si quieres `BACKLOGGD_USUARIO`, `BACKLOGGD_CONTRASENA`),
     `STEAM_API_KEY`, `STEAM_ID`, `EPIC_REFRESH_TOKEN`, `PSN_NPSSO`,
     `NINTENDO_SESSION_TOKEN`. Solo los de las plataformas que uses.
   - **Variables**: `BACKLOGGD_SYNC` = `1` para encenderlo. Opcionales: `ZONA_HORARIA`,
     `MINUTOS_MINIMOS`, `NINTENDO_JUGADOR`.
3. **Actions → Sincronizar Backloggd → Run workflow** para la primera ejecución.

Los juegos pendientes de asociar aparecen en el registro de cada ejecución (con sus
candidatos). Para asociarlos, edita `backloggd-sync/asociaciones.json` desde GitHub
(ver más abajo).

GitHub desactiva los workflows programados de un repositorio público tras 60 días sin
actividad; si pasa, vuelve a activarlo desde la pestaña Actions.

## Tests

```sh
python3 -m unittest discover -s tests -v
```

Usan servidores falsos (no necesitan red ni cuentas). Comprueban, entre otras cosas, que
guardar en Backloggd no borra las sesiones existentes, que la misma sesión del día se
actualiza en vez de duplicarse y que la primera lectura no importa el historial.
