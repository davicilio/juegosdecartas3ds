"""Lógica de sincronización: de lecturas de las plataformas a sesiones diarias en Backloggd.

Cada juego de cada plataforma tiene una clave («steam:620», «psn:PPSA01234_00»,
«epic:Fortnite», «nintendo:0100152000022000»). El tiempo jugado se acumula por clave y
día en `estado["dias"]`; después se agrupa por juego de Backloggd y día, y se escribe
como UNA sesión por juego y día. Volver a ejecutarlo solo actualiza esa sesión.
"""

from __future__ import annotations

import json
import os
import re
import tempfile
import unicodedata
from dataclasses import dataclass, field
from datetime import date, datetime, timedelta, timezone
from zoneinfo import ZoneInfo

from .backloggd import Backloggd, ErrorAutenticacion, ErrorBackloggd
from .fuentes import Dia, Total

DIAS_GUARDADOS = 14          # días ya enviados que se recuerdan (para no duplicar)
DIAS_CORTOS = 3              # un día que no llega al mínimo se descarta pasado este tiempo
DIAS_ENVIADOS = 45           # cuánto se recuerda la sesión escrita de cada juego y día
DIAS_NINTENDO = 7            # Nintendo da ~31 días de historial; solo se miran los últimos
TAM_REGISTRO = 40


def estado_vacio() -> dict:
    return {"version": 1, "fotos": {}, "dias": {}, "enviados": {}, "asociaciones": {},
            "sin_asociar": {}, "terminados": {}, "tokens": {}, "nombres_epic": {},
            "inicio_nintendo": None, "ultima_ejecucion": None, "registro": []}


def cargar_estado(ruta: str) -> dict:
    estado = estado_vacio()
    if os.path.exists(ruta):
        with open(ruta, encoding="utf-8") as f:
            estado.update(json.load(f))
    return estado


def guardar_estado(ruta: str, estado: dict) -> None:
    """Escritura atómica, con permisos solo para el dueño (contiene tokens)."""
    carpeta = os.path.dirname(os.path.abspath(ruta))
    os.makedirs(carpeta, exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=carpeta, prefix=".estado-")
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        json.dump(estado, f, ensure_ascii=False, indent=1, sort_keys=True)
    os.chmod(tmp, 0o600)
    os.replace(tmp, ruta)


def normalizar_titulo(t: str) -> str:
    t = re.sub(r"[™®©]", "", str(t or ""))
    t = unicodedata.normalize("NFKD", t)
    t = "".join(c for c in t if not unicodedata.combining(c)).lower()
    t = re.sub(r"\s*[(\[][^)\]]*[)\]]", " ", t)          # «(PS4)», «[Demo]»...
    t = re.sub(r"\b(ps4|ps5)( (and|&) (ps4|ps5))?\s*$", "", t.strip())
    return re.sub(r"[^a-z0-9]+", " ", t).strip()


@dataclass
class Resultado:
    ganado: list[str] = field(default_factory=list)
    enviado: list[str] = field(default_factory=list)
    sin_asociar: list[str] = field(default_factory=list)
    terminados: list[str] = field(default_factory=list)
    errores: list[str] = field(default_factory=list)

    def vacio(self) -> bool:
        return not (self.ganado or self.enviado or self.terminados or self.errores)


class Sincronizador:
    def __init__(self, estado: dict, *, zona: str = "Europe/Madrid", minimo: int = 5,
                 ahora: datetime | None = None):
        self.estado = estado
        self.zona = ZoneInfo(zona)
        self.minimo = minimo
        self.ahora = ahora or datetime.now(timezone.utc)
        self.hoy = self.ahora.astimezone(self.zona).date().isoformat()
        self.r = Resultado()

    def fecha_local(self, momento: datetime | None) -> str:
        if momento is None:
            return self.hoy
        fecha = momento.astimezone(self.zona).date().isoformat()
        return min(fecha, self.hoy)

    # -------------------------------------------------------------- acumular

    def _sumar(self, clave: str, nombre: str, fecha: str, minutos: float, plataforma) -> None:
        k = f"{clave}|{fecha}"
        d = self.estado["dias"].setdefault(k, {"clave": clave, "nombre": nombre, "fecha": fecha, "minutos": 0.0})
        d["minutos"] += minutos
        d["nombre"] = nombre
        if plataforma:
            d["plataforma"] = plataforma

    def registrar_totales(self, fuente: str, totales: list[Total]) -> None:
        """Compara con la lectura anterior. La primera vez solo guarda el punto de partida
        (no importa el historial: solo cuenta lo que se juegue desde ahora)."""
        anterior = self.estado["fotos"].get(fuente)
        foto = {t.clave: round(t.minutos, 2) for t in totales}
        if anterior is not None:
            for t in totales:
                delta = t.minutos - anterior.get(t.clave, 0.0)
                if delta >= 0.5:
                    self._sumar(t.clave, t.nombre, self.fecha_local(t.ultima_vez), delta, t.plataforma)
                    self.r.ganado.append(f"{t.nombre} +{round(delta)} min")
            # Un juego que deja de aparecer (p. ej. oculto) conserva su último total.
            for clave, minutos in anterior.items():
                foto.setdefault(clave, minutos)
        self.estado["fotos"][fuente] = foto

    def registrar_dias(self, dias: list[Dia]) -> None:
        """Totales diarios ya calculados (Nintendo). Se toma el mayor visto para cada día."""
        if not self.estado.get("inicio_nintendo"):
            self.estado["inicio_nintendo"] = self.hoy
        desde = max(self.estado["inicio_nintendo"],
                    (date.fromisoformat(self.hoy) - timedelta(days=DIAS_NINTENDO)).isoformat())
        for d in dias:
            if d.fecha < desde or d.fecha > self.hoy:
                continue
            k = f"{d.clave}|{d.fecha}"
            previo = self.estado["dias"].get(k, {}).get("minutos", 0.0)
            if d.minutos > previo:
                self._sumar(d.clave, d.nombre, d.fecha, d.minutos - previo, d.plataforma)
                self.r.ganado.append(f"{d.nombre} +{round(d.minutos - previo)} min ({d.fecha})")

    # -------------------------------------------------------------- asociar

    def asociar(self, bl: Backloggd, clave: str, nombre: str):
        """Juego de Backloggd para una clave, o None. Solo asocia sola una coincidencia
        exacta y única del título; lo demás queda pendiente para «asociar» a mano."""
        a = self.estado["asociaciones"].get(clave)
        if a:
            return None if a.get("ignorar") else a
        pend = self.estado["sin_asociar"].get(clave)
        if pend and pend.get("buscado") == self.hoy:
            return None
        objetivo = normalizar_titulo(nombre)
        candidatos = bl.buscar(nombre)
        exactos = [c for c in candidatos if normalizar_titulo(c["titulo"]) == objetivo]
        if len(exactos) == 1:
            a = {"id": exactos[0]["id"], "slug": exactos[0]["slug"], "titulo": exactos[0]["titulo"]}
            self.estado["asociaciones"][clave] = a
            self.estado["sin_asociar"].pop(clave, None)
            return a
        self.estado["sin_asociar"][clave] = {"nombre": nombre, "buscado": self.hoy,
                                             "candidatos": (exactos or candidatos)[:5]}
        return None

    # -------------------------------------------------------------- enviar

    def enviar(self, bl: Backloggd, *, simular: bool = False, marcar_terminados: bool = True) -> None:
        grupos: dict[tuple[int, str], dict] = {}
        for k, d in list(self.estado["dias"].items()):
            if d.get("enviado") and d.get("enviado_min") == round(d["minutos"]):
                continue
            try:
                juego = self.asociar(bl, d["clave"], d["nombre"])
            except ErrorAutenticacion:
                raise
            except ErrorBackloggd as e:
                self.r.errores.append(f"Buscar «{d['nombre']}»: {e}")
                continue
            if not juego:
                if d["nombre"] not in self.r.sin_asociar:
                    self.r.sin_asociar.append(d["nombre"])
                continue
            g = grupos.setdefault((juego["id"], d["fecha"]), {"juego": juego, "minutos": 0.0,
                                                             "plataforma": None, "claves": []})
            g["claves"].append(k)
            g["plataforma"] = g["plataforma"] or d.get("plataforma")

        # El total del grupo incluye también días de otras plataformas ya enviados.
        for (gid, fecha), g in grupos.items():
            g["minutos"] = sum(d["minutos"] for k, d in self.estado["dias"].items()
                               if d["fecha"] == fecha and self._id_de(d["clave"]) == gid)

        for (gid, fecha), g in sorted(grupos.items(), key=lambda x: x[0][1]):
            minutos = round(g["minutos"])
            clave_env = f"{gid}|{fecha}"
            previo = self.estado["enviados"].get(clave_env)
            if minutos < self.minimo:
                continue
            if previo and previo.get("minutos") == minutos:
                self._marcar(g["claves"])
                continue
            titulo = g["juego"].get("titulo") or g["juego"].get("slug")
            if simular:
                self.r.enviado.append(f"[simulado] {titulo} {fecha} → {minutos} min")
                continue
            try:
                res = bl.fijar_sesion_dia(gid, fecha, minutos, plataforma=g["plataforma"],
                                          session_id=(previo or {}).get("session_id"),
                                          ya_enviada=bool(previo))
            except ErrorAutenticacion:
                raise
            except ErrorBackloggd as e:
                self.r.errores.append(f"{titulo} {fecha}: {e}")
                continue
            self.estado["enviados"][clave_env] = {"minutos": minutos, "session_id": res.get("session_id"),
                                                  "titulo": titulo}
            self._marcar(g["claves"])
            if res.get("cambiada"):
                self.r.enviado.append(f"{titulo} {fecha} → {minutos} min")

        if marcar_terminados and not simular:
            self._terminados(bl)

    def _id_de(self, clave: str):
        a = self.estado["asociaciones"].get(clave) or {}
        return None if a.get("ignorar") else a.get("id")

    def _marcar(self, claves: list[str]) -> None:
        for k in claves:
            d = self.estado["dias"][k]
            d["enviado"] = True
            d["enviado_min"] = round(d["minutos"])

    def _terminados(self, bl: Backloggd) -> None:
        """Juegos con sesiones recientes que se han marcado como terminados en Backloggd:
        su última sesión se marca como el final de la partida."""
        limite = (date.fromisoformat(self.hoy) - timedelta(days=DIAS_GUARDADOS)).isoformat()
        recientes = {}
        for k, v in self.estado["enviados"].items():
            gid, fecha = k.split("|")
            if fecha >= limite and gid not in self.estado["terminados"]:
                recientes[gid] = v.get("titulo") or gid
        for gid, titulo in recientes.items():
            try:
                res = bl.marcar_terminado(int(gid))
            except ErrorBackloggd as e:
                self.r.errores.append(f"Marcar terminado {titulo}: {e}")
                continue
            if res:
                self.estado["terminados"][gid] = res if isinstance(res, str) else self.hoy
                if isinstance(res, str):
                    self.r.terminados.append(f"{titulo} (sesión del {res})")

    # -------------------------------------------------------------- limpiar

    def limpiar(self) -> None:
        hoy = date.fromisoformat(self.hoy)
        for k, d in list(self.estado["dias"].items()):
            edad = (hoy - date.fromisoformat(d["fecha"])).days
            if (d.get("enviado") and edad > DIAS_GUARDADOS) or (not d.get("enviado") and edad > DIAS_CORTOS
                                                                 and self._id_de(d["clave"]) is not None
                                                                 and d["minutos"] < self.minimo):
                del self.estado["dias"][k]
            elif edad > DIAS_ENVIADOS:  # pendientes de asociar durante demasiado tiempo
                del self.estado["dias"][k]
        for k in list(self.estado["enviados"]):
            if (hoy - date.fromisoformat(k.split("|")[1])).days > DIAS_ENVIADOS:
                del self.estado["enviados"][k]

    def anotar(self) -> None:
        self.estado["ultima_ejecucion"] = self.ahora.isoformat(timespec="seconds")
        if self.r.vacio():
            return
        entrada = {"cuando": self.estado["ultima_ejecucion"]}
        for campo in ("ganado", "enviado", "terminados", "errores"):
            if getattr(self.r, campo):
                entrada[campo] = getattr(self.r, campo)
        self.estado["registro"] = ([entrada] + self.estado.get("registro", []))[:TAM_REGISTRO]
