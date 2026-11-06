"""Cliente de Backloggd.

Backloggd no tiene API pública: estos son los mismos endpoints que usa la propia web,
tal y como los documentan github.com/DenisionSoft/backloggd-mcp y
github.com/jethequeen/steam-backloggd-sync. Pueden cambiar sin aviso.

Puntos delicados (aprendidos de esos proyectos):
- Guardar un log REEMPLAZA todas las sesiones de la partida (playthrough): hay que
  reenviar las que ya existen o se borran.
- El servidor solo guarda las sesiones marcadas `edited=true`.
- Las sesiones son rangos [inicio, fin) con el fin exclusivo.
- Hay que reenviar el estado de las estanterías (jugando, backlog...) para no moverlo.
"""

from __future__ import annotations

import re
import time
from datetime import date, timedelta

from .red import Http, Respuesta

BASE_URL = "https://backloggd.com"
USER_AGENT = "backloggd-sync/1.0 (sincronizador personal de tiempo de juego)"
ESPERA_ESCRITURA = 2.5  # segundos entre escrituras, para no cargar la web
ESTADOS_TERMINADOS = {"played", "completed", "retired"}


class ErrorAutenticacion(Exception):
    pass


class ErrorBackloggd(Exception):
    pass


def desplazar_fecha(fecha: str, dias: int) -> str:
    return (date.fromisoformat(fecha) + timedelta(days=dias)).isoformat()


def _num(v):
    if v is None or v == "":
        return None
    try:
        return int(str(v))
    except ValueError:
        return None


def normalizar_sesiones(crudas) -> list[dict]:
    """`play_dates` de Backloggd a una forma cómoda (con el fin ya inclusivo)."""
    if not isinstance(crudas, list):
        return []
    out = []
    for s in crudas:
        if not isinstance(s, dict):
            continue
        ini = s.get("range_start_date")
        fin = s.get("range_end_date")
        if fin and ini and fin != ini:
            fin = desplazar_fecha(fin, -1)
        out.append(
            {
                "id": s.get("id"),
                "inicio": ini,
                "fin": fin or ini,
                "horas": _num(s.get("hours")),
                "minutos": _num(s.get("minutes")),
                "nota": (s.get("note") or "").strip() or None,
                "estado": s.get("status"),
                # Se reenvían tal cual para no borrar las marcas de Empezado/Terminado.
                "marca_inicio": s.get("start_date") or "",
                "marca_fin": s.get("finish_date") or "",
            }
        )
    return out


def campos_sesiones(playthrough_id, sesiones: list[dict]) -> dict:
    """Los campos `dates[playthrough][i][...]`; el guardado sustituye todo el conjunto."""
    form = {}
    for i, s in enumerate(sesiones):
        p = f"dates[{playthrough_id}][{i}]"
        form[f"{p}[id]"] = s["id"]
        form[f"{p}[range_start_date]"] = s["inicio"]
        form[f"{p}[range_end_date]"] = desplazar_fecha(s.get("fin") or s["inicio"], 1)
        form[f"{p}[edited]"] = "true"
        form[f"{p}[note]"] = s.get("nota") or ""
        form[f"{p}[hours]"] = "" if s.get("horas") is None else s["horas"]
        form[f"{p}[minutes]"] = "" if s.get("minutos") is None else s["minutos"]
        form[f"{p}[status]"] = s.get("estado") or ""
        form[f"{p}[start_date]"] = s.get("marca_inicio") or ""
        form[f"{p}[finish_date]"] = s.get("marca_fin") or ""
    return form


def campos_playthrough(pt: dict, sesiones: list[dict]) -> dict:
    form = {
        "playthroughs[0][id]": pt["id"],
        "playthroughs[0][title]": pt.get("title") or "Log",
        "playthroughs[0][rating]": pt.get("rating") or 0,
        "playthroughs[0][review]": pt.get("review") or "",
        "playthroughs[0][review_spoilers]": str(bool(pt.get("review_spoilers"))).lower(),
        "playthroughs[0][is_master]": str(bool(pt.get("is_master"))).lower(),
        "playthroughs[0][is_replay]": str(bool(pt.get("is_replay"))).lower(),
        "playthroughs[0][sync_sessions]": "true",
    }
    form.update(campos_sesiones(pt["id"], sesiones))
    return form


def plataforma_actual(pt: dict):
    actual = pt.get("played_platform")
    if actual in (None, ""):
        actual = pt.get("platform")
    return None if actual in (None, "") else actual


class Backloggd:
    def __init__(self, http: Http, *, cookie: str | None = None, usuario: str | None = None,
                 contrasena: str | None = None, espera: float | None = None, dormir=time.sleep):
        self.http = http
        self.cookies: dict[str, str] = {"ne_cookies_consent": "true"}
        if cookie:
            self.cookies["_backloggd_session"] = cookie.strip().removeprefix("_backloggd_session=")
        self.usuario = usuario
        self.contrasena = contrasena
        self.espera = ESPERA_ESCRITURA if espera is None else espera
        self.dormir = dormir
        self.csrf: str | None = None
        self.user_id: str | None = None
        self._ultima_escritura = 0.0

    @property
    def cookie_sesion(self) -> str | None:
        """Rails rota la cookie: hay que guardarla tras cada ejecución."""
        return self.cookies.get("_backloggd_session")

    # ------------------------------------------------------------------ HTTP

    def _peticion(self, ruta: str, *, metodo="GET", form=None, consulta=None, escritura=False,
                  csrf=None, aceptar=None) -> Respuesta:
        if csrf is None:
            csrf = metodo != "GET"
        if escritura:
            falta = self._ultima_escritura + self.espera - time.monotonic()
            if falta > 0:
                self.dormir(falta)
        cab = {
            "User-Agent": USER_AGENT,
            "Accept": aceptar or "text/html,application/json,*/*;q=0.8",
            "X-Requested-With": "XMLHttpRequest",
            "Referer": BASE_URL + "/",
            "Origin": BASE_URL,
            "Cookie": "; ".join(f"{k}={v}" for k, v in self.cookies.items()),
        }
        if csrf:
            cab["X-CSRF-Token"] = self._token_csrf()
        res = self.http.peticion(metodo, BASE_URL + ruta, cabeceras=cab, form=form, consulta=consulta)
        for linea in res.todas("set-cookie"):
            par = linea.split(";", 1)[0]
            if "=" in par:
                k, v = par.split("=", 1)
                self.cookies[k.strip()] = v.strip()
        if escritura:
            self._ultima_escritura = time.monotonic()
        return res

    def _token_csrf(self) -> str:
        if not self.csrf:
            res = self._peticion("/", csrf=False)
            m = re.search(r'<meta name="csrf-token" content="([^"]+)"', res.texto)
            if not m:
                raise ErrorBackloggd("No se encontró el token CSRF en backloggd.com")
            self.csrf = m.group(1)
        return self.csrf

    # ------------------------------------------------------------------ sesión

    def autenticar(self) -> str:
        """Comprueba la cookie y averigua el id numérico del usuario. Si caducó y hay
        usuario y contraseña, vuelve a iniciar sesión."""
        if self.user_id:
            return self.user_id
        if self.cookie_sesion:
            self.user_id = self._identidad()
            if self.user_id:
                return self.user_id
        if not (self.usuario and self.contrasena):
            raise ErrorAutenticacion(
                "La sesión de Backloggd caducó o no es válida. Copia de nuevo la cookie "
                "_backloggd_session o configura BACKLOGGD_USUARIO y BACKLOGGD_CONTRASENA."
            )
        self._iniciar_sesion()
        self.user_id = self._identidad()
        if not self.user_id:
            raise ErrorAutenticacion("Backloggd no aceptó el inicio de sesión.")
        return self.user_id

    def _identidad(self) -> str | None:
        res = self._peticion("/settings/", csrf=False)
        if res.status != 200 or "/users/sign_out" not in res.texto:
            return None
        m = re.search(r"user_id=[\"']?(\d+)", res.texto)
        return m.group(1) if m else None

    def _iniciar_sesion(self) -> None:
        self.cookies.pop("_backloggd_session", None)
        self.csrf = None
        html = self._peticion("/users/sign_in", csrf=False).texto
        m_form = re.search(r'<form[^>]*action="/users/sign_in"[\s\S]*?</form>', html)
        m = re.search(r'name="authenticity_token"[^>]*value="([^"]+)"', m_form.group(0) if m_form else html)
        if not m:
            raise ErrorAutenticacion("No se pudo leer el formulario de inicio de sesión de Backloggd.")
        res = self._peticion("/users/sign_in", metodo="POST", csrf=False, escritura=True, form={
            "authenticity_token": m.group(1),
            "user[login]": self.usuario,
            "user[password]": self.contrasena,
            "user[remember_me]": "1",
        })
        if res.status == 422:
            raise ErrorAutenticacion("Backloggd rechazó el usuario o la contraseña.")
        self.csrf = None

    # ------------------------------------------------------------------ lectura

    def buscar(self, texto: str) -> list[dict]:
        """[{id, slug, titulo, anio}] con el autocompletado de la web."""
        res = self._peticion("/autocomplete.json", consulta={"query": texto}, aceptar="application/json")
        if not res.ok:
            raise ErrorBackloggd(f"Autocompletado de Backloggd: HTTP {res.status}")
        try:
            datos = res.json() or {}
        except ValueError:
            datos = {}
        out = []
        for s in datos.get("suggestions") or []:
            d = s.get("data") or {}
            try:
                gid = int(d.get("id"))
            except (TypeError, ValueError):
                continue
            if not d.get("slug"):
                continue
            anio = d.get("year")
            out.append({
                "id": gid,
                "slug": d["slug"],
                "titulo": d.get("title") or s.get("value") or d["slug"],
                "anio": int(anio) if str(anio or "").isdigit() else None,
            })
        return out

    def id_desde_slug(self, slug: str) -> dict:
        """Lee la ficha /games/<slug>/ para sacar el id del juego."""
        res = self._peticion(f"/games/{slug}/")
        if not res.ok:
            raise ErrorBackloggd(f"No existe /games/{slug}/ en Backloggd (HTTP {res.status})")
        m = re.search(r"game_id=[\"']?(\d+)", res.texto)
        if not m:
            raise ErrorBackloggd(f"No se encontró el id del juego en /games/{slug}/")
        t = re.search(r"<title>([^<|]+)", res.texto)
        return {"id": int(m.group(1)), "slug": slug, "titulo": (t.group(1).strip() if t else slug)}

    def log_juego(self, game_id: int) -> tuple[dict | None, list[dict]]:
        """Estado completo del usuario para un juego: (log, playthroughs)."""
        res = self._peticion(f"/log/edit/{game_id}", aceptar="application/json")
        if res.status in (401, 403):
            raise ErrorAutenticacion(f"Backloggd rechazó la lectura (HTTP {res.status}).")
        if not res.ok:
            raise ErrorBackloggd(f"/log/edit/{game_id}: HTTP {res.status}")
        datos = res.json() or {}
        crudos = datos.get("playthroughs") or []
        pts = list(crudos.values()) if isinstance(crudos, dict) else list(crudos)
        return datos.get("game_log"), [p for p in pts if isinstance(p, dict)]

    # ------------------------------------------------------------------ escritura

    def _guardar_log(self, game_id: int, log: dict | None, campos: dict, *, marcar_jugando=False) -> None:
        user_id = self.autenticar()
        log = log or {}
        form = {
            "game_id": game_id,
            **campos,
            "log[game_liked]": str(bool(log.get("game_liked"))).lower(),
            "log[is_play]": str(bool(log.get("is_play"))).lower(),
            "log[is_playing]": str(marcar_jugando or bool(log.get("is_playing"))).lower(),
            "log[is_backlog]": str(bool(log.get("is_backlog"))).lower(),
            "log[is_wishlist]": str(bool(log.get("is_wishlist"))).lower(),
            "log[status]": log.get("status") or "played",
            "log[id]": log.get("id") or "",
            "log[time_source]": "1",
            "modal_type": "full",
        }
        res = self._peticion(f"/api/user/{user_id}/log/{game_id}", metodo="POST", escritura=True, form=form)
        if res.status == 422:  # token CSRF caducado: se renueva una vez
            self.csrf = None
            res = self._peticion(f"/api/user/{user_id}/log/{game_id}", metodo="POST", escritura=True, form=form)
        if res.status in (401, 403):
            raise ErrorAutenticacion(f"Backloggd rechazó la escritura (HTTP {res.status}).")
        if not res.ok:
            raise ErrorBackloggd(f"Guardar el log de {game_id}: HTTP {res.status}")

    def fijar_sesion_dia(self, game_id: int, fecha: str, minutos: int, *, plataforma=None,
                         session_id=None, ya_enviada=False) -> dict:
        """Deja la sesión de este programa en `fecha` con `minutos`, creando el log, la
        partida y la sesión si hace falta. Repetirlo con los mismos datos no cambia nada.

        Devuelve {"cambiada": bool, "session_id": id|None}.
        """
        self.autenticar()
        log, pts = self.log_juego(game_id)
        ya_jugado = bool((log or {}).get("is_play"))

        if not pts:
            self._guardar_log(game_id, log, {
                "playthroughs[0][id]": -1,
                "playthroughs[0][title]": "Log",
                "playthroughs[0][sync_sessions]": "false",
            }, marcar_jugando=True)
            log, pts = self.log_juego(game_id)
            if not pts:
                raise ErrorBackloggd(f"No se pudo crear una partida para el juego {game_id}")

        pt = pts[-1]
        sesiones = normalizar_sesiones(pt.get("play_dates"))
        mismo_dia = [s for s in sesiones if s["inicio"] == fecha]
        sin_nota = [s for s in mismo_dia if not s["nota"]]
        mia = None
        if session_id is not None:
            mia = next((s for s in sesiones if str(s["id"]) == str(session_id)), None)
        if mia is None and ya_enviada and len(sin_nota) == 1:
            # Se escribió antes pero se perdió su id: es la única del día sin nota.
            mia = sin_nota[0]

        deseado = {"horas": minutos // 60, "minutos": minutos % 60}
        actual = plataforma_actual(pt)
        poner_plataforma = plataforma is not None and actual is None
        if mia and mia["horas"] == deseado["horas"] and mia["minutos"] == deseado["minutos"] and not poner_plataforma:
            return {"cambiada": False, "session_id": mia["id"]}

        if mia:
            nuevas = [({**s, **deseado} if s is mia else s) for s in sesiones]
        else:
            # La primera sesión de la partida la marca como empezada.
            marca = fecha if not sesiones and not ya_jugado else ""
            nuevas = sesiones + [{"id": -1, "inicio": fecha, "fin": fecha, "nota": None, "estado": None,
                                  "marca_inicio": marca, "marca_fin": "", **deseado}]

        campos = campos_playthrough(pt, nuevas)
        mantener = {} if actual is None else {
            "playthroughs[0][platform]": actual, "playthroughs[0][played_platform]": actual}
        if poner_plataforma:
            try:
                self._guardar_log(game_id, log, {**campos, "playthroughs[0][platform]": plataforma,
                                                 "playthroughs[0][played_platform]": plataforma})
            except ErrorBackloggd:
                # Una plataforma que la web no acepta nunca debe frenar el tiempo de juego.
                self._guardar_log(game_id, log, campos)
        else:
            self._guardar_log(game_id, log, {**campos, **mantener})

        nuevo_id = mia["id"] if mia else self._id_sesion_nueva(game_id, fecha, sesiones)
        return {"cambiada": True, "session_id": nuevo_id}

    def _id_sesion_nueva(self, game_id: int, fecha: str, antes: list[dict]):
        try:
            conocidas = {str(s["id"]) for s in antes}
            _, pts = self.log_juego(game_id)
            nuevas = [s for s in normalizar_sesiones(pts[-1].get("play_dates") if pts else None)
                      if s["inicio"] == fecha and str(s["id"]) not in conocidas]
            return nuevas[0]["id"] if len(nuevas) == 1 else None
        except (ErrorBackloggd, ValueError):
            return None

    def marcar_terminado(self, game_id: int):
        """Si el juego está como Played/Completed/Retired en Backloggd, marca su última
        sesión como final de la partida. Devuelve la fecha marcada, True si no hay nada
        que marcar o False si el juego no está terminado."""
        log, pts = self.log_juego(game_id)
        if not (log or {}).get("is_play") or (log or {}).get("status") not in ESTADOS_TERMINADOS:
            return False
        if not pts:
            return True
        pt = pts[-1]
        sesiones = normalizar_sesiones(pt.get("play_dates"))
        if not sesiones or any(s["marca_fin"] for s in sesiones):
            return True
        ultima = max(sesiones, key=lambda s: s["fin"] or s["inicio"] or "")
        fecha = ultima["fin"] or ultima["inicio"]
        nuevas = [({**s, "marca_fin": fecha} if s is ultima else s) for s in sesiones]
        actual = plataforma_actual(pt)
        mantener = {} if actual is None else {
            "playthroughs[0][platform]": actual, "playthroughs[0][played_platform]": actual}
        self._guardar_log(game_id, log, {**campos_playthrough(pt, nuevas), **mantener})
        return fecha
