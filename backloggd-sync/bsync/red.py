"""Peticiones HTTP con la biblioteca estándar (sin dependencias)."""

from __future__ import annotations

import json
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass, field

TIMEOUT = 30


class ErrorHTTP(Exception):
    def __init__(self, status: int, url: str, cuerpo: str = ""):
        super().__init__(f"HTTP {status} en {url}: {cuerpo[:300]}")
        self.status = status
        self.url = url
        self.cuerpo = cuerpo


@dataclass
class Respuesta:
    status: int
    url: str
    cabeceras: list[tuple[str, str]] = field(default_factory=list)
    cuerpo: bytes = b""

    @property
    def ok(self) -> bool:
        return 200 <= self.status < 300

    @property
    def texto(self) -> str:
        return self.cuerpo.decode("utf-8", errors="replace")

    def json(self):
        return json.loads(self.cuerpo or b"null")

    def cabecera(self, nombre: str) -> str | None:
        nombre = nombre.lower()
        for k, v in self.cabeceras:
            if k.lower() == nombre:
                return v
        return None

    def todas(self, nombre: str) -> list[str]:
        nombre = nombre.lower()
        return [v for k, v in self.cabeceras if k.lower() == nombre]


class _SinRedirecciones(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):  # noqa: D401
        return None


_opener = urllib.request.build_opener(_SinRedirecciones())


def codificar_form(form: dict) -> bytes:
    """Un valor lista se envía como un campo por elemento (`ids[]=1&ids[]=2`)."""
    pares = []
    for k, v in form.items():
        if isinstance(v, (list, tuple)):
            pares.extend((k, "" if x is None else str(x)) for x in v)
        else:
            pares.append((k, "" if v is None else str(v)))
    return urllib.parse.urlencode(pares).encode()


class Http:
    """Cliente mínimo. Las fuentes lo reciben inyectado, así los tests lo sustituyen."""

    def peticion(
        self,
        metodo: str,
        url: str,
        *,
        cabeceras: dict | None = None,
        consulta: dict | None = None,
        form: dict | None = None,
        json_cuerpo=None,
        cuerpo: bytes | None = None,
    ) -> Respuesta:
        if consulta:
            url += ("&" if "?" in url else "?") + urllib.parse.urlencode(
                {k: v for k, v in consulta.items() if v is not None}
            )
        cab = dict(cabeceras or {})
        if form is not None:
            cuerpo = codificar_form(form)
            cab.setdefault("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8")
        elif json_cuerpo is not None:
            cuerpo = json.dumps(json_cuerpo).encode()
            cab.setdefault("Content-Type", "application/json")
        req = urllib.request.Request(url, data=cuerpo, headers=cab, method=metodo)
        try:
            with _opener.open(req, timeout=TIMEOUT) as r:
                return Respuesta(r.status, url, list(r.headers.items()), r.read())
        except urllib.error.HTTPError as e:  # 3xx (sin seguir), 4xx y 5xx
            return Respuesta(e.code, url, list(e.headers.items()) if e.headers else [], e.read() or b"")
