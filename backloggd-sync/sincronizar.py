#!/usr/bin/env python3
"""Sincronizador de sesiones de juego: Steam, Epic Games, Nintendo Switch y PS5 → Backloggd.

Uso rápido:
    python3 sincronizar.py                 # sincroniza (lo normal, programarlo cada hora)
    python3 sincronizar.py --simular       # enseña lo que haría sin escribir nada
    python3 sincronizar.py estado          # último resultado, pendientes, registro
    python3 sincronizar.py pendientes      # juegos que no se pudieron asociar solos
    python3 sincronizar.py asociar CLAVE SLUG   # p. ej. asociar steam:379720 doom--1
    python3 sincronizar.py ignorar CLAVE   # no enviar nunca ese juego
    python3 sincronizar.py buscar TEXTO    # buscar el slug de un juego en Backloggd
    python3 sincronizar.py login-epic      # conectar Epic Games (una vez)
    python3 sincronizar.py login-nintendo  # conectar Nintendo (una vez)
    python3 sincronizar.py login-psn       # comprobar PlayStation y guardar el token

La configuración va en variables de entorno o en un archivo `.env` junto a este script
(ver `config.ejemplo.env`).
"""

from __future__ import annotations

import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from bsync import fuentes  # noqa: E402
from bsync.backloggd import Backloggd, ErrorAutenticacion, ErrorBackloggd  # noqa: E402
from bsync.fuentes import ErrorFuente  # noqa: E402
from bsync.nucleo import Sincronizador, cargar_estado, guardar_estado  # noqa: E402
from bsync.red import Http  # noqa: E402

CARPETA = os.path.dirname(os.path.abspath(__file__))


def cargar_env(ruta: str) -> None:
    """Lee un .env sencillo (CLAVE=valor). Las variables ya definidas tienen prioridad."""
    if not os.path.exists(ruta):
        return
    with open(ruta, encoding="utf-8") as f:
        for linea in f:
            linea = linea.strip()
            if not linea or linea.startswith("#") or "=" not in linea:
                continue
            k, v = linea.split("=", 1)
            v = v.strip()
            if len(v) >= 2 and v[0] == v[-1] and v[0] in "\"'":
                v = v[1:-1]
            os.environ.setdefault(k.strip(), v)


def env(nombre: str, defecto: str | None = None) -> str | None:
    v = os.environ.get(nombre, "").strip()
    return v or defecto


def cliente_backloggd(http: Http, estado: dict) -> Backloggd:
    # La cookie guardada (rotada en la última ejecución) va antes que la de la configuración.
    cookie = estado["tokens"].get("backloggd_cookie") or env("BACKLOGGD_COOKIE")
    return Backloggd(http, cookie=cookie, usuario=env("BACKLOGGD_USUARIO"),
                     contrasena=env("BACKLOGGD_CONTRASENA"))


def leer_fuentes(http: Http, estado: dict, sync: Sincronizador, zona: str) -> list[str]:
    """Lee cada plataforma configurada. Un fallo en una no detiene a las demás."""
    leidas = []
    tokens = estado["tokens"]

    if env("STEAM_API_KEY") and env("STEAM_ID"):
        try:
            sync.registrar_totales("steam", fuentes.leer_steam(http, env("STEAM_API_KEY"), env("STEAM_ID")))
            leidas.append("Steam")
        except (ErrorFuente, OSError, ValueError) as e:
            sync.r.errores.append(f"Steam: {e}")

    refresh_epic = tokens.get("epic_refresh") or env("EPIC_REFRESH_TOKEN")
    if refresh_epic:
        try:
            token = fuentes.epic_token(http, refresh_token=refresh_epic)
            tokens["epic_refresh"] = token["refresh_token"]
            totales = fuentes.leer_epic(http, token, estado.get("nombres_epic"))
            estado["nombres_epic"].update({t.clave.split(":", 1)[1]: t.nombre for t in totales})
            sync.registrar_totales("epic", totales)
            leidas.append("Epic Games")
        except (ErrorFuente, OSError, ValueError, KeyError) as e:
            sync.r.errores.append(f"Epic Games: {e}")

    if tokens.get("psn_refresh") or env("PSN_NPSSO"):
        try:
            token = fuentes.psn_token(http, refresh_token=tokens.get("psn_refresh"), npsso=env("PSN_NPSSO"))
            tokens["psn_refresh"] = token.get("refresh_token") or tokens.get("psn_refresh")
            sync.registrar_totales("psn", fuentes.leer_psn(http, token))
            leidas.append("PlayStation")
        except (ErrorFuente, OSError, ValueError, KeyError) as e:
            sync.r.errores.append(f"PlayStation: {e}")

    token_nin = tokens.get("nintendo_session") or env("NINTENDO_SESSION_TOKEN")
    if token_nin:
        try:
            sync.registrar_dias(fuentes.leer_nintendo(http, token_nin, zona, env("NINTENDO_JUGADOR")))
            tokens["nintendo_session"] = token_nin
            leidas.append("Nintendo")
        except (ErrorFuente, OSError, ValueError, KeyError) as e:
            sync.r.errores.append(f"Nintendo: {e}")
    return leidas


def aplicar_asociaciones_fijas(bl: Backloggd, estado: dict, errores: list) -> None:
    """`asociaciones.json` (junto al script) fija juegos a mano: {"clave": "slug" | "ignorar"}.
    Sirve sobre todo con GitHub Actions, donde el estado no se puede editar."""
    ruta = os.path.join(CARPETA, "asociaciones.json")
    if not os.path.exists(ruta):
        return
    with open(ruta, encoding="utf-8") as f:
        fijas = json.load(f)
    for clave, valor in fijas.items():
        if clave.startswith("_"):
            continue
        actual = estado["asociaciones"].get(clave) or {}
        if valor == "ignorar":
            estado["asociaciones"][clave] = {"ignorar": True}
            estado["sin_asociar"].pop(clave, None)
            continue
        slug = str(valor).strip("/").split("/")[-1]
        if actual.get("slug") == slug:
            continue
        try:
            estado["asociaciones"][clave] = bl.id_desde_slug(slug)
        except ErrorBackloggd as e:
            errores.append(f"asociaciones.json, {clave}: {e}")
            continue
        estado["sin_asociar"].pop(clave, None)
        for d in estado["dias"].values():
            if d["clave"] == clave:
                d.pop("enviado", None)


def cmd_sincronizar(args, ruta_estado: str) -> int:
    http = Http()
    estado = cargar_estado(ruta_estado)
    zona = env("ZONA_HORARIA", "Europe/Madrid")
    sync = Sincronizador(estado, zona=zona, minimo=int(env("MINUTOS_MINIMOS", "5")))
    leidas = leer_fuentes(http, estado, sync, zona)
    if not leidas and not sync.r.errores:
        print("No hay ninguna plataforma configurada. Mira config.ejemplo.env.")
        return 2

    bl = cliente_backloggd(http, estado)
    codigo = 0
    try:
        aplicar_asociaciones_fijas(bl, estado, sync.r.errores)
        sync.enviar(bl, simular=args.simular, marcar_terminados=env("MARCAR_TERMINADOS", "1") != "0")
    except ErrorAutenticacion as e:
        sync.r.errores.append(f"Backloggd: {e}")
        codigo = 3
    except Exception as e:  # noqa: BLE001 - lo leído y los tokens rotados deben guardarse igual
        sync.r.errores.append(f"Backloggd: {type(e).__name__}: {e}")
    if bl.cookie_sesion:
        estado["tokens"]["backloggd_cookie"] = bl.cookie_sesion
    sync.limpiar()
    sync.anotar()

    r = sync.r
    print(f"Plataformas leídas: {', '.join(leidas) or 'ninguna'}")
    for titulo, lista in (("Jugado", r.ganado), ("Enviado a Backloggd", r.enviado),
                          ("Marcado como terminado", r.terminados), ("Sin asociar", r.sin_asociar),
                          ("Errores", r.errores)):
        if lista:
            print(f"{titulo}:")
            for x in lista:
                print(f"  - {x}")
    if r.sin_asociar:
        print("Juegos sin asociar (añádelos a asociaciones.json o usa «asociar CLAVE SLUG»):")
        for clave, p in estado["sin_asociar"].items():
            cands = ", ".join(c["slug"] for c in p.get("candidatos") or []) or "ningún candidato"
            print(f"  - {clave} «{p['nombre']}» → {cands}")
    if args.simular:
        # Los tokens de Epic cambian en cada uso: aunque sea una simulación, se guardan.
        guardado = cargar_estado(ruta_estado)
        guardado["tokens"] = estado["tokens"]
        guardar_estado(ruta_estado, guardado)
        print("(simulación: no se ha escrito nada en Backloggd ni se ha guardado el tiempo leído)")
        return codigo
    guardar_estado(ruta_estado, estado)
    return codigo or (1 if r.errores else 0)


def cmd_estado(args, ruta_estado: str) -> int:
    estado = cargar_estado(ruta_estado)
    print(f"Última ejecución: {estado.get('ultima_ejecucion') or 'nunca'}")
    print(f"Juegos asociados: {sum(1 for a in estado['asociaciones'].values() if not a.get('ignorar'))}, "
          f"ignorados: {sum(1 for a in estado['asociaciones'].values() if a.get('ignorar'))}, "
          f"pendientes: {len(estado['sin_asociar'])}")
    pendientes = [d for d in estado["dias"].values() if not d.get("enviado")]
    if pendientes:
        print("Tiempo aún no enviado:")
        for d in sorted(pendientes, key=lambda d: d["fecha"]):
            print(f"  - {d['fecha']} {d['nombre']} ({d['clave']}): {round(d['minutos'])} min")
    for e in estado.get("registro", [])[:10]:
        print(json.dumps(e, ensure_ascii=False))
    return 0


def cmd_pendientes(args, ruta_estado: str) -> int:
    estado = cargar_estado(ruta_estado)
    if not estado["sin_asociar"]:
        print("No hay juegos pendientes de asociar.")
        return 0
    for clave, p in estado["sin_asociar"].items():
        print(f"{clave}  «{p['nombre']}»")
        for c in p.get("candidatos") or []:
            print(f"    {c['slug']:<40} {c['titulo']} ({c.get('anio') or '?'})")
    print("\nAsocia con: python3 sincronizar.py asociar CLAVE SLUG   (o «ignorar CLAVE»)")
    return 0


def cmd_asociar(args, ruta_estado: str) -> int:
    estado = cargar_estado(ruta_estado)
    bl = cliente_backloggd(Http(), estado)
    juego = bl.id_desde_slug(args.slug.strip("/").split("/")[-1])
    estado["asociaciones"][args.clave] = juego
    estado["sin_asociar"].pop(args.clave, None)
    for d in estado["dias"].values():  # el tiempo ya acumulado se enviará en la próxima ejecución
        if d["clave"] == args.clave:
            d.pop("enviado", None)
    guardar_estado(ruta_estado, estado)
    print(f"{args.clave} → {juego['titulo']} (id {juego['id']})")
    return 0


def cmd_ignorar(args, ruta_estado: str) -> int:
    estado = cargar_estado(ruta_estado)
    estado["asociaciones"][args.clave] = {"ignorar": True}
    estado["sin_asociar"].pop(args.clave, None)
    guardar_estado(ruta_estado, estado)
    print(f"{args.clave} no se enviará a Backloggd.")
    return 0


def cmd_buscar(args, ruta_estado: str) -> int:
    bl = cliente_backloggd(Http(), cargar_estado(ruta_estado))
    for c in bl.buscar(" ".join(args.texto)):
        print(f"{c['slug']:<40} {c['titulo']} ({c.get('anio') or '?'})")
    return 0


def cmd_login_epic(args, ruta_estado: str) -> int:
    codigo = args.codigo
    if not codigo:
        print("1. Abre esta dirección e inicia sesión en Epic Games:\n")
        print("   " + fuentes.EPIC_URL_LOGIN + "\n")
        print('2. Verás un texto JSON. Copia el valor de "authorizationCode".')
        codigo = input("Código: ").strip().strip('"')
    estado = cargar_estado(ruta_estado)
    token = fuentes.epic_token(Http(), codigo=codigo)
    estado["tokens"]["epic_refresh"] = token["refresh_token"]
    guardar_estado(ruta_estado, estado)
    print(f"Epic Games conectado ({token.get('displayName') or token.get('account_id')}).")
    print("Si lo usas en GitHub Actions, guarda este valor en el secreto EPIC_REFRESH_TOKEN:")
    print(token["refresh_token"])
    return 0


def cmd_login_nintendo(args, ruta_estado: str) -> int:
    url, verificador = fuentes.nintendo_url_login()
    print("Requisito: tu Switch tiene que estar vinculada a la app «Control Parental de Nintendo")
    print("Switch» (puedes dejarla sin ninguna restricción).\n")
    print("1. Abre esta dirección en el navegador del ordenador e inicia sesión:\n")
    print("   " + url + "\n")
    print("2. En el botón rojo «Elegir a esta persona», haz clic derecho → «Copiar dirección del")
    print("   enlace». Empieza por npf54789befb391a838://auth#...")
    enlace = input("Enlace: ").strip()
    token = fuentes.nintendo_session_token(Http(), enlace, verificador)
    estado = cargar_estado(ruta_estado)
    estado["tokens"]["nintendo_session"] = token
    guardar_estado(ruta_estado, estado)
    print("Nintendo conectado. El session_token dura unos dos años.")
    print("Si lo usas en GitHub Actions, guarda este valor en el secreto NINTENDO_SESSION_TOKEN:")
    print(token)
    return 0


def cmd_login_psn(args, ruta_estado: str) -> int:
    npsso = args.npsso or env("PSN_NPSSO")
    if not npsso:
        print("1. Inicia sesión en https://www.playstation.com en el navegador.")
        print("2. Abre https://ca.account.sony.com/api/v1/ssocookie y copia el valor de «npsso».")
        npsso = input("NPSSO: ").strip().strip('"')
    estado = cargar_estado(ruta_estado)
    token = fuentes.psn_token(Http(), npsso=npsso)
    estado["tokens"]["psn_refresh"] = token["refresh_token"]
    guardar_estado(ruta_estado, estado)
    print("PlayStation conectado. El token dura unos dos meses; luego hace falta un NPSSO nuevo.")
    return 0


def main(argv=None) -> int:
    cargar_env(os.path.join(CARPETA, ".env"))
    p = argparse.ArgumentParser(description="Sincroniza sesiones de juego con Backloggd.")
    p.add_argument("--estado", default=env("ESTADO", os.path.join(CARPETA, "estado.json")),
                   help="archivo de estado (contiene tokens: no lo compartas)")
    p.add_argument("--simular", action="store_true", help="no escribir en Backloggd ni guardar el estado")
    sub = p.add_subparsers(dest="cmd")
    sub.add_parser("sincronizar")
    sub.add_parser("estado")
    sub.add_parser("pendientes")
    a = sub.add_parser("asociar")
    a.add_argument("clave")
    a.add_argument("slug", help="slug o URL de Backloggd (p. ej. hollow-knight)")
    a = sub.add_parser("ignorar")
    a.add_argument("clave")
    a = sub.add_parser("buscar")
    a.add_argument("texto", nargs="+")
    a = sub.add_parser("login-epic")
    a.add_argument("codigo", nargs="?")
    sub.add_parser("login-nintendo")
    a = sub.add_parser("login-psn")
    a.add_argument("npsso", nargs="?")
    args = p.parse_args(argv)

    ordenes = {None: cmd_sincronizar, "sincronizar": cmd_sincronizar, "estado": cmd_estado,
               "pendientes": cmd_pendientes, "asociar": cmd_asociar, "ignorar": cmd_ignorar,
               "buscar": cmd_buscar, "login-epic": cmd_login_epic, "login-nintendo": cmd_login_nintendo,
               "login-psn": cmd_login_psn}
    try:
        return ordenes[args.cmd](args, args.estado)
    except (ErrorFuente, ErrorAutenticacion, ErrorBackloggd) as e:
        print(f"Error: {e}", file=sys.stderr)
        return 3


if __name__ == "__main__":
    sys.exit(main())
