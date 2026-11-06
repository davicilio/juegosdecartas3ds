"""Lectura del tiempo de juego en cada plataforma.

Steam, Epic y PlayStation dan el tiempo TOTAL por juego: el núcleo compara con la
lectura anterior y la diferencia es lo jugado desde entonces. Nintendo (a través del
Control Parental) da directamente los minutos de cada día, que es aún mejor.
"""

from __future__ import annotations

import base64
import hashlib
import re
import secrets
import urllib.parse
from dataclasses import dataclass
from datetime import datetime, timezone

from .red import Http

# Ids de plataforma de IGDB, que es de donde saca Backloggd su catálogo.
PLATAFORMA_PC = 6
PLATAFORMA_PS4 = 48
PLATAFORMA_PS5 = 167
PLATAFORMA_SWITCH = 130
PLATAFORMA_SWITCH2 = 508


class ErrorFuente(Exception):
    """Fallo de una plataforma. No detiene a las demás."""


@dataclass
class Total:
    """Tiempo total acumulado de un juego en una plataforma."""

    clave: str          # p. ej. "steam:620", única por plataforma y juego
    nombre: str
    minutos: float
    ultima_vez: datetime | None = None  # cuándo se jugó por última vez, si se sabe
    plataforma: int | None = None


@dataclass
class Dia:
    """Minutos jugados a un juego en un día concreto (ya calculados por la plataforma)."""

    clave: str
    nombre: str
    fecha: str  # AAAA-MM-DD
    minutos: float
    plataforma: int | None = None


# ---------------------------------------------------------------------- Steam

def leer_steam(http: Http, api_key: str, steam_id: str) -> list[Total]:
    """Todos los juegos de la cuenta con su tiempo total. El perfil debe tener los
    «Detalles de los juegos» en público."""
    res = http.peticion("GET", "https://api.steampowered.com/IPlayerService/GetOwnedGames/v1/", consulta={
        "key": api_key, "steamid": steam_id, "include_appinfo": 1,
        "include_played_free_games": 1, "format": "json",
    })
    if not res.ok:
        raise ErrorFuente(f"Steam GetOwnedGames: HTTP {res.status}")
    juegos = ((res.json() or {}).get("response") or {}).get("games")
    if not isinstance(juegos, list):
        raise ErrorFuente("Steam no devolvió juegos: ¿están en público los «Detalles de los juegos» del perfil?")
    out = []
    for g in juegos:
        ultima = g.get("rtime_last_played") or 0
        out.append(Total(
            clave=f"steam:{g['appid']}",
            nombre=g.get("name") or f"App {g['appid']}",
            minutos=float(g.get("playtime_forever") or 0),
            ultima_vez=datetime.fromtimestamp(ultima, timezone.utc) if ultima else None,
            plataforma=PLATAFORMA_PC,
        ))
    return out


# ---------------------------------------------------------------------- Epic Games

EPIC_CLIENTE = "34a02cf8f4414e29b15921876da36f9a"
EPIC_SECRETO = "daafbccc737745039dffe53d94fc76cf"
EPIC_OAUTH = "https://account-public-service-prod03.ol.epicgames.com/account/api/oauth/token"
EPIC_LIBRERIA = "https://library-service.live.use1a.on.epicgames.com/library/api/public"
EPIC_CATALOGO = "https://catalog-public-service-prod06.ol.epicgames.com/catalog/api/shared/namespace"
EPIC_URL_LOGIN = ("https://www.epicgames.com/id/login?redirectUrl=" + urllib.parse.quote(
    f"https://www.epicgames.com/id/api/redirect?clientId={EPIC_CLIENTE}&responseType=code"))


def epic_token(http: Http, *, refresh_token: str | None = None, codigo: str | None = None) -> dict:
    """Devuelve el token de Epic. El refresh_token cambia en cada uso: hay que guardarlo."""
    if refresh_token:
        form = {"grant_type": "refresh_token", "refresh_token": refresh_token, "token_type": "eg1"}
    elif codigo:
        form = {"grant_type": "authorization_code", "code": codigo.strip(), "token_type": "eg1"}
    else:
        raise ErrorFuente("Epic: falta el código de autorización (ejecuta «login-epic»).")
    basico = base64.b64encode(f"{EPIC_CLIENTE}:{EPIC_SECRETO}".encode()).decode()
    res = http.peticion("POST", EPIC_OAUTH, cabeceras={"Authorization": f"Basic {basico}"}, form=form)
    datos = res.json() if res.cuerpo else {}
    if not res.ok or "access_token" not in (datos or {}):
        error = (datos or {}).get("errorCode") or f"HTTP {res.status}"
        raise ErrorFuente(f"Epic rechazó el inicio de sesión ({error}). Vuelve a ejecutar «login-epic».")
    return datos


def leer_epic(http: Http, token: dict, nombres_conocidos: dict | None = None) -> list[Total]:
    """Tiempo total por juego de la biblioteca de Epic."""
    cab = {"Authorization": f"bearer {token['access_token']}"}
    cuenta = token["account_id"]
    res = http.peticion("GET", f"{EPIC_LIBRERIA}/playtime/account/{cuenta}/all", cabeceras=cab)
    if not res.ok:
        raise ErrorFuente(f"Epic playtime: HTTP {res.status}")
    tiempos = {t["artifactId"]: t.get("totalTime") or 0 for t in (res.json() or []) if t.get("artifactId")}
    nombres = dict(nombres_conocidos or {})
    faltan = [a for a, seg in tiempos.items() if seg and a not in nombres]
    if faltan:
        nombres.update(_epic_nombres(http, cab, set(faltan)))
    return [
        Total(clave=f"epic:{app}", nombre=nombres.get(app, app), minutos=seg / 60, plataforma=PLATAFORMA_PC)
        for app, seg in tiempos.items()
    ]


def _epic_nombres(http: Http, cab: dict, apps: set[str]) -> dict:
    """Nombre de cada appName, a partir de la biblioteca y el catálogo."""
    registros, cursor = [], None
    while True:
        res = http.peticion("GET", f"{EPIC_LIBRERIA}/items", cabeceras=cab,
                            consulta={"includeMetadata": "true", "cursor": cursor})
        if not res.ok:
            return {}
        datos = res.json() or {}
        registros.extend(datos.get("records") or [])
        cursor = (datos.get("responseMetadata") or {}).get("nextCursor")
        if not cursor:
            break
    nombres = {}
    for r in registros:
        app = r.get("appName")
        if app not in apps or not r.get("namespace") or not r.get("catalogItemId"):
            continue
        res = http.peticion("GET", f"{EPIC_CATALOGO}/{r['namespace']}/bulk/items", cabeceras=cab, consulta={
            "id": r["catalogItemId"], "includeDLCDetails": "true", "includeMainGameDetails": "true",
            "country": "ES", "locale": "es-ES",
        })
        if res.ok:
            item = (res.json() or {}).get(r["catalogItemId"]) or {}
            if item.get("title"):
                nombres[app] = item["title"]
    return nombres


# ---------------------------------------------------------------------- PlayStation

PSN_CLIENTE = "09515159-7237-4370-9b40-3806e67c0891"
PSN_BASICO = "MDk1MTUxNTktNzIzNy00MzcwLTliNDAtMzgwNmU2N2MwODkxOnVjUGprYTV0bnRCMktxc1A="
PSN_REDIRECCION = "com.scee.psxandroid.scecompcall://redirect"
PSN_AMBITO = "psn:mobile.v2.core psn:clientapp"
PSN_AUTH = "https://ca.account.sony.com/api/authz/v3/oauth"
PSN_JUEGOS = "https://m.np.playstation.com/api/gamelist/v2/users/me/titles"


def psn_token(http: Http, *, refresh_token: str | None = None, npsso: str | None = None) -> dict:
    """Token de PSN: con el refresh_token guardado (dura ~2 meses) o, si no, con NPSSO."""
    cab = {"Authorization": f"Basic {PSN_BASICO}"}
    if refresh_token:
        res = http.peticion("POST", f"{PSN_AUTH}/token", cabeceras=cab, form={
            "refresh_token": refresh_token, "grant_type": "refresh_token",
            "scope": PSN_AMBITO, "token_format": "jwt"})
        if res.ok:
            return res.json()
        if not npsso:
            raise ErrorFuente("PSN: el refresh token caducó. Pon un NPSSO nuevo (PSN_NPSSO).")
    if not npsso:
        raise ErrorFuente("PSN: falta el NPSSO (PSN_NPSSO).")
    res = http.peticion("GET", f"{PSN_AUTH}/authorize", cabeceras={"Cookie": f"npsso={npsso.strip()}"}, consulta={
        "access_type": "offline", "client_id": PSN_CLIENTE, "redirect_uri": PSN_REDIRECCION,
        "response_type": "code", "scope": PSN_AMBITO})
    destino = res.cabecera("location") or ""
    params = urllib.parse.parse_qs(urllib.parse.urlparse(destino).query)
    if "code" not in params:
        raise ErrorFuente("PSN: el NPSSO caducó o no es correcto. Consigue uno nuevo.")
    res = http.peticion("POST", f"{PSN_AUTH}/token", cabeceras=cab, form={
        "code": params["code"][0], "redirect_uri": PSN_REDIRECCION, "grant_type": "authorization_code",
        "scope": PSN_AMBITO, "token_format": "jwt"})
    if not res.ok:
        raise ErrorFuente(f"PSN: no se pudo obtener el token (HTTP {res.status}).")
    return res.json()


_DURACION = re.compile(r"P(?:(\d+)D)?T?(?:(\d+)H)?(?:(\d+)M)?(?:(\d+(?:\.\d+)?)S)?")


def duracion_iso_a_minutos(texto: str | None) -> float:
    """'PT228H56M33S' -> minutos."""
    m = _DURACION.fullmatch(texto or "")
    if not m:
        return 0.0
    d, h, mi, s = (float(x) if x else 0.0 for x in m.groups())
    return d * 1440 + h * 60 + mi + s / 60


def _fecha_iso(texto: str | None) -> datetime | None:
    if not texto:
        return None
    try:
        return datetime.fromisoformat(texto.replace("Z", "+00:00"))
    except ValueError:
        return None


def leer_psn(http: Http, token: dict) -> list[Total]:
    """Juegos de PS4 y PS5 con su tiempo total."""
    cab = {"Authorization": f"Bearer {token['access_token']}"}
    out, offset = [], 0
    while True:
        res = http.peticion("GET", PSN_JUEGOS, cabeceras=cab, consulta={
            "categories": "ps4_game,ps5_native_game", "limit": 200, "offset": offset})
        if not res.ok:
            raise ErrorFuente(f"PSN gamelist: HTTP {res.status}")
        datos = res.json() or {}
        for t in datos.get("titles") or []:
            out.append(Total(
                clave=f"psn:{t.get('titleId')}",
                nombre=t.get("name") or t.get("titleId"),
                minutos=duracion_iso_a_minutos(t.get("playDuration")),
                ultima_vez=_fecha_iso(t.get("lastPlayedDateTime")),
                plataforma=PLATAFORMA_PS5 if t.get("category") == "ps5_native_game" else PLATAFORMA_PS4,
            ))
        offset = datos.get("nextOffset") or 0
        if not offset:
            return out


# ---------------------------------------------------------------------- Nintendo Switch

NINTENDO_CLIENTE = "54789befb391a838"  # app «Control Parental de Nintendo Switch»
NINTENDO_REDIRECCION = f"npf{NINTENDO_CLIENTE}://auth"
NINTENDO_AMBITOS = ["openid", "user", "moonUser:administration", "moonDevice:create",
                    "moonOwnedDevice:administration", "moonParentalControlSetting",
                    "moonParentalControlSetting:update", "moonParentalControlSettingState",
                    "moonPairingState", "moonSmartDevice:administration",
                    "moonDailySummary", "moonMonthlySummary"]
NINTENDO_CUENTAS = "https://accounts.nintendo.com/connect/1.0.0"
NINTENDO_MOON = "https://app.lp1.znma.srv.nintendo.net"
MOON_VERSION, MOON_BUILD = "2.6.0", "700"


def nintendo_url_login() -> tuple[str, str]:
    """(url, verificador). El usuario abre la url, inicia sesión y copia el enlace del
    botón «Elegir a esta persona» (npf54789befb391a838://auth#session_token_code=...)."""
    verificador = secrets.token_urlsafe(48)
    reto = base64.urlsafe_b64encode(hashlib.sha256(verificador.encode()).digest()).decode().rstrip("=")
    consulta = urllib.parse.urlencode({
        "client_id": NINTENDO_CLIENTE, "redirect_uri": NINTENDO_REDIRECCION,
        "response_type": "session_token_code", "scope": " ".join(NINTENDO_AMBITOS),
        "session_token_code_challenge": reto, "session_token_code_challenge_method": "S256",
        "state": secrets.token_urlsafe(24), "theme": "login_form",
    })
    return f"{NINTENDO_CUENTAS}/authorize?{consulta}", verificador


def nintendo_session_token(http: Http, enlace: str, verificador: str) -> str:
    """Cambia el enlace copiado por un session_token (dura unos dos años)."""
    frag = urllib.parse.parse_qs(urllib.parse.urlparse(enlace.strip()).fragment)
    codigo = (frag.get("session_token_code") or [None])[0]
    if not codigo:
        raise ErrorFuente("Ese enlace no contiene session_token_code.")
    res = http.peticion("POST", f"{NINTENDO_CUENTAS}/api/session_token", form={
        "client_id": NINTENDO_CLIENTE, "session_token_code": codigo,
        "session_token_code_verifier": verificador})
    if not res.ok:
        raise ErrorFuente(f"Nintendo rechazó el código (HTTP {res.status}).")
    return res.json()["session_token"]


def leer_nintendo(http: Http, session_token: str, zona: str, jugador: str | None = None,
                  idioma: str = "es-ES") -> list[Dia]:
    """Minutos por día y juego de cada consola vinculada al Control Parental.
    Si se indica `jugador` (apodo del usuario en la consola), solo cuenta el suyo."""
    res = http.peticion("POST", f"{NINTENDO_CUENTAS}/api/token", json_cuerpo={
        "client_id": NINTENDO_CLIENTE, "session_token": session_token,
        "grant_type": "urn:ietf:params:oauth:grant-type:jwt-bearer-session-token"})
    if not res.ok:
        raise ErrorFuente(f"Nintendo: el session_token no es válido (HTTP {res.status}). Ejecuta «login-nintendo».")
    tokens = res.json()
    cab = {
        "User-Agent": f"moon_ANDROID/{MOON_VERSION} (com.nintendo.znma; build:{MOON_BUILD}; ANDROID 34)",
        "X-Moon-App-Id": "com.nintendo.znma", "X-Moon-Os": "ANDROID", "X-Moon-Os-Version": "34",
        "X-Moon-Model": "Pixel 4 XL", "X-Moon-App-Display-Version": MOON_VERSION,
        "X-Moon-App-Internal-Version": MOON_BUILD, "X-Moon-TimeZone": zona,
        "X-Moon-Os-Language": idioma, "X-Moon-App-Language": idioma,
        # La app de Control Parental se autentica con el id_token, no con el access_token.
        "Authorization": f"Bearer {tokens.get('id_token') or tokens.get('access_token')}",
    }
    res = http.peticion("GET", f"{NINTENDO_MOON}/v2/actions/user/fetchOwnedDevices", cabeceras=cab)
    if not res.ok:
        raise ErrorFuente(f"Nintendo: no se pudieron leer las consolas (HTTP {res.status}).")
    consolas = (res.json() or {}).get("ownedDevices") or []
    if not consolas:
        raise ErrorFuente("Nintendo: no hay ninguna consola vinculada al Control Parental.")

    dias: dict[tuple[str, str], Dia] = {}
    for c in consolas:
        dev = c.get("deviceId")
        gen = ((c.get("device") or {}).get("platformGeneration") or "P00")
        plataforma = PLATAFORMA_SWITCH if gen == "P00" else PLATAFORMA_SWITCH2
        res = http.peticion("GET", f"{NINTENDO_MOON}/v2/actions/playSummary/fetchDailySummaries",
                            cabeceras=cab, consulta={"deviceId": dev})
        if not res.ok:
            raise ErrorFuente(f"Nintendo: resúmenes diarios de {dev}: HTTP {res.status}")
        for resumen in (res.json() or {}).get("dailySummaries") or []:
            fecha = resumen.get("date")
            if not fecha:
                continue
            titulos = {a.get("applicationId", "").upper(): a.get("title")
                       for a in resumen.get("playedApps") or [] if isinstance(a, dict)}
            for p in resumen.get("players") or resumen.get("devicePlayers") or []:
                perfil = p.get("profile") or p
                if jugador and (perfil.get("nickname") or "").lower() != jugador.lower():
                    continue
                for j in p.get("playedGames") or p.get("playedApps") or []:
                    meta = j.get("meta") or {}
                    app = (j.get("applicationId") or meta.get("applicationId") or "").upper()
                    minutos = j.get("playingTime") or 0
                    if not app or not minutos:
                        continue
                    nombre = meta.get("title") or titulos.get(app) or app
                    d = dias.setdefault((app, fecha), Dia(f"nintendo:{app}", nombre, fecha, 0, plataforma))
                    d.minutos += minutos
    return list(dias.values())

