"""Tests sin red: Backloggd y las plataformas se sustituyen por servidores falsos."""

import json
import os
import sys
import unittest
import urllib.parse
from datetime import datetime, timezone

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from bsync import fuentes  # noqa: E402
from bsync.backloggd import Backloggd, campos_sesiones, normalizar_sesiones  # noqa: E402
from bsync.fuentes import Dia, Total  # noqa: E402
from bsync.nucleo import Sincronizador, estado_vacio, normalizar_titulo  # noqa: E402
from bsync.red import Respuesta  # noqa: E402

HOY = datetime(2026, 11, 6, 18, 0, tzinfo=timezone.utc)


class BackloggdFalso:
    """Imita los endpoints de backloggd.com que usa el cliente."""

    JUEGOS = [
        {"id": 100, "slug": "hollow-knight", "title": "Hollow Knight", "year": "2017"},
        {"id": 200, "slug": "doom", "title": "DOOM", "year": "1993"},
        {"id": 201, "slug": "doom--1", "title": "DOOM", "year": "2016"},
        {"id": 300, "slug": "mario-kart-8-deluxe", "title": "Mario Kart 8 Deluxe", "year": "2017"},
    ]

    def __init__(self):
        self.logs = {}           # game_id -> {"game_log": {...}, "playthroughs": [...]}
        self.siguiente_id = 1000
        self.escrituras = []

    def peticion(self, metodo, url, *, cabeceras=None, consulta=None, form=None, json_cuerpo=None, cuerpo=None):
        ruta = urllib.parse.urlparse(url).path
        if ruta == "/":
            return self._r('<meta name="csrf-token" content="tok">')
        if ruta == "/settings/":
            return self._r('<a href="/users/sign_out">x</a> user_id="42"')
        if ruta == "/autocomplete.json":
            q = normalizar_titulo(consulta["query"])
            sug = [{"value": j["title"], "data": j} for j in self.JUEGOS if q in normalizar_titulo(j["title"])]
            return self._r(json.dumps({"suggestions": sug}))
        if ruta.startswith("/games/"):
            slug = ruta.split("/")[2]
            j = next((j for j in self.JUEGOS if j["slug"] == slug), None)
            return self._r(f'<title>{j["title"]} | Backloggd</title> game_id="{j["id"]}"') if j else self._r("", 404)
        if ruta.startswith("/log/edit/"):
            gid = int(ruta.split("/")[-1])
            return self._r(json.dumps(self.logs.get(gid, {"game_log": None, "playthroughs": []})))
        if ruta.startswith("/api/user/42/log/") and metodo == "POST":
            assert cabeceras.get("X-CSRF-Token") == "tok"
            self.escrituras.append(form)
            self._guardar(int(ruta.split("/")[-1]), form)
            return self._r("{}")
        return self._r("", 404)

    def _r(self, texto, status=200):
        return Respuesta(status, "", [], texto.encode())

    def _guardar(self, gid, form):
        datos = self.logs.setdefault(gid, {"game_log": None, "playthroughs": []})
        datos["game_log"] = {"id": 1, "is_play": form["log[is_play]"] == "true",
                             "is_playing": form["log[is_playing]"] == "true", "status": form["log[status]"]}
        pid = int(form["playthroughs[0][id]"])
        if pid == -1:
            self.siguiente_id += 1
            datos["playthroughs"].append({"id": self.siguiente_id, "title": "Log", "play_dates": []})
            return
        pt = next(p for p in datos["playthroughs"] if p["id"] == pid)
        if "playthroughs[0][played_platform]" in form:
            pt["played_platform"] = form["playthroughs[0][played_platform]"]
        if form.get("playthroughs[0][sync_sessions]") != "true":
            return
        # Como la web real: el conjunto enviado SUSTITUYE al anterior.
        nuevas, i = [], 0
        while f"dates[{pid}][{i}][id]" in form:
            p = f"dates[{pid}][{i}]"
            if form[f"{p}[edited]"] == "true":
                sid = int(form[f"{p}[id]"])
                if sid == -1:
                    self.siguiente_id += 1
                    sid = self.siguiente_id
                nuevas.append({"id": sid, "range_start_date": form[f"{p}[range_start_date]"],
                               "range_end_date": form[f"{p}[range_end_date]"],
                               "hours": form[f"{p}[hours]"], "minutes": form[f"{p}[minutes]"],
                               "note": form[f"{p}[note]"], "start_date": form[f"{p}[start_date]"] or None,
                               "finish_date": form[f"{p}[finish_date]"] or None})
            i += 1
        pt["play_dates"] = nuevas

    def sesiones(self, gid):
        return normalizar_sesiones(self.logs[gid]["playthroughs"][-1]["play_dates"])


def ejecutar(estado, *, totales=None, dias=None, falso=None, ahora=HOY, simular=False):
    falso = falso or BackloggdFalso()
    s = Sincronizador(estado, zona="Europe/Madrid", minimo=5, ahora=ahora)
    for fuente, lista in (totales or {}).items():
        s.registrar_totales(fuente, lista)
    if dias:
        s.registrar_dias(dias)
    s.enviar(Backloggd(falso, cookie="c", espera=0), simular=simular)
    s.limpiar()
    s.anotar()
    return s.r, falso


def hk(minutos, ultima=None):
    return Total("steam:367520", "Hollow Knight", minutos, ultima, fuentes.PLATAFORMA_PC)


class TestSincronizacion(unittest.TestCase):
    def test_primera_lectura_no_importa_historial(self):
        estado = estado_vacio()
        r, falso = ejecutar(estado, totales={"steam": [hk(600)]})
        self.assertEqual(falso.escrituras, [])
        self.assertEqual(estado["fotos"]["steam"]["steam:367520"], 600)

    def test_crea_y_actualiza_la_sesion_del_dia(self):
        estado = estado_vacio()
        falso = BackloggdFalso()
        ejecutar(estado, totales={"steam": [hk(600)]}, falso=falso)
        r, _ = ejecutar(estado, totales={"steam": [hk(630)]}, falso=falso)
        self.assertEqual(r.enviado, ["Hollow Knight 2026-11-06 → 30 min"])
        s = falso.sesiones(100)
        self.assertEqual(len(s), 1)
        self.assertEqual((s[0]["inicio"], s[0]["fin"], s[0]["horas"], s[0]["minutos"]),
                         ("2026-11-06", "2026-11-06", 0, 30))
        self.assertEqual(s[0]["marca_inicio"], "2026-11-06")  # primera sesión = empezado
        self.assertTrue(falso.logs[100]["game_log"]["is_playing"])
        self.assertEqual(falso.logs[100]["playthroughs"][-1]["played_platform"], 6)

        # Más tiempo el mismo día: misma sesión, no una nueva.
        r, _ = ejecutar(estado, totales={"steam": [hk(725)]}, falso=falso)
        s = falso.sesiones(100)
        self.assertEqual(len(s), 1)
        self.assertEqual((s[0]["horas"], s[0]["minutos"]), (2, 5))
        self.assertEqual(s[0]["marca_inicio"], "2026-11-06")

        # Sin cambios: no escribe.
        n = len(falso.escrituras)
        ejecutar(estado, totales={"steam": [hk(725)]}, falso=falso)
        self.assertEqual(len(falso.escrituras), n)

    def test_conserva_sesiones_puestas_a_mano(self):
        estado = estado_vacio()
        falso = BackloggdFalso()
        falso.logs[100] = {"game_log": {"id": 1, "is_play": True, "status": "played"}, "playthroughs": [
            {"id": 5, "title": "Log", "played_platform": 130, "play_dates": [
                {"id": 7, "range_start_date": "2026-10-01", "range_end_date": "2026-10-02",
                 "hours": 1, "minutes": 0, "note": "a mano", "start_date": "2026-10-01"}]}]}
        ejecutar(estado, totales={"steam": [hk(0)]}, falso=falso)
        ejecutar(estado, totales={"steam": [hk(45)]}, falso=falso)
        s = falso.sesiones(100)
        self.assertEqual([x["id"] for x in s][0], 7)
        self.assertEqual(s[0]["nota"], "a mano")
        self.assertEqual(s[0]["marca_inicio"], "2026-10-01")
        self.assertEqual(len(s), 2)
        self.assertEqual(s[1]["marca_inicio"], "")  # ya estaba empezado
        self.assertEqual(falso.logs[100]["playthroughs"][-1]["played_platform"], 130)  # no se toca

    def test_menos_del_minimo_espera_a_acumular(self):
        estado = estado_vacio()
        falso = BackloggdFalso()
        ejecutar(estado, totales={"steam": [hk(0)]}, falso=falso)
        ejecutar(estado, totales={"steam": [hk(3)]}, falso=falso)
        self.assertNotIn(100, falso.logs)
        ejecutar(estado, totales={"steam": [hk(6)]}, falso=falso)
        self.assertEqual(falso.sesiones(100)[0]["minutos"], 6)

    def test_fecha_segun_ultima_partida(self):
        estado = estado_vacio()
        falso = BackloggdFalso()
        ejecutar(estado, totales={"steam": [hk(0)]}, falso=falso)
        ayer_noche = datetime(2026, 11, 5, 22, 30, tzinfo=timezone.utc)  # 23:30 en Madrid
        ejecutar(estado, totales={"steam": [hk(40, ayer_noche)]}, falso=falso)
        self.assertEqual(falso.sesiones(100)[0]["inicio"], "2026-11-05")

    def test_mismo_juego_en_dos_plataformas_suma_en_una_sesion(self):
        estado = estado_vacio()
        falso = BackloggdFalso()
        psn = lambda m: Total("psn:CUSA1", "Hollow Knight", m, None, fuentes.PLATAFORMA_PS4)  # noqa: E731
        ejecutar(estado, totales={"steam": [hk(0)], "psn": [psn(0)]}, falso=falso)
        ejecutar(estado, totales={"steam": [hk(20)], "psn": [psn(15)]}, falso=falso)
        s = falso.sesiones(100)
        self.assertEqual(len(s), 1)
        self.assertEqual(s[0]["minutos"], 35)

    def test_nintendo_por_dias(self):
        estado = estado_vacio()
        falso = BackloggdFalso()
        mk = lambda f, m: Dia("nintendo:0100152000022000", "Mario Kart 8 Deluxe", f, m, 130)  # noqa: E731
        # Primera vez: solo cuenta desde hoy (no importa el historial de días anteriores).
        ejecutar(estado, dias=[mk("2026-11-04", 50), mk("2026-11-06", 25)], falso=falso)
        s = falso.sesiones(300)
        self.assertEqual([(x["inicio"], x["minutos"]) for x in s], [("2026-11-06", 25)])
        # Al día siguiente: el día anterior se completa y aparece el nuevo.
        manana = datetime(2026, 11, 7, 12, 0, tzinfo=timezone.utc)
        ejecutar(estado, dias=[mk("2026-11-06", 70), mk("2026-11-07", 10)], falso=falso, ahora=manana)
        s = falso.sesiones(300)
        self.assertEqual([(x["inicio"], x["horas"], x["minutos"]) for x in s],
                         [("2026-11-06", 1, 10), ("2026-11-07", 0, 10)])

    def test_titulo_ambiguo_queda_pendiente_y_se_asocia_a_mano(self):
        estado = estado_vacio()
        falso = BackloggdFalso()
        doom = lambda m: Total("steam:379720", "DOOM", m)  # noqa: E731
        ejecutar(estado, totales={"steam": [doom(0)]}, falso=falso)
        r, _ = ejecutar(estado, totales={"steam": [doom(60)]}, falso=falso)
        self.assertEqual(r.sin_asociar, ["DOOM"])
        self.assertEqual(len(estado["sin_asociar"]["steam:379720"]["candidatos"]), 2)
        self.assertEqual(falso.escrituras, [])

        estado["asociaciones"]["steam:379720"] = Backloggd(falso, cookie="c").id_desde_slug("doom--1")
        r, _ = ejecutar(estado, totales={"steam": [doom(60)]}, falso=falso)
        self.assertEqual(falso.sesiones(201)[0]["horas"], 1)

    def test_simular_no_escribe(self):
        estado = estado_vacio()
        falso = BackloggdFalso()
        ejecutar(estado, totales={"steam": [hk(0)]}, falso=falso)
        r, _ = ejecutar(estado, totales={"steam": [hk(30)]}, falso=falso, simular=True)
        self.assertEqual(falso.escrituras, [])
        self.assertTrue(r.enviado[0].startswith("[simulado]"))

    def test_marca_terminado(self):
        estado = estado_vacio()
        falso = BackloggdFalso()
        ejecutar(estado, totales={"steam": [hk(0)]}, falso=falso)
        ejecutar(estado, totales={"steam": [hk(30)]}, falso=falso)
        falso.logs[100]["game_log"].update(is_play=True, status="completed")
        r, _ = ejecutar(estado, totales={"steam": [hk(30)]}, falso=falso)
        self.assertEqual(falso.sesiones(100)[0]["marca_fin"], "2026-11-06")
        self.assertEqual(r.terminados, ["Hollow Knight (sesión del 2026-11-06)"])


class TestUtilidades(unittest.TestCase):
    def test_duracion_psn(self):
        self.assertAlmostEqual(fuentes.duracion_iso_a_minutos("PT228H56M33S"), 228 * 60 + 56 + 33 / 60)
        self.assertEqual(fuentes.duracion_iso_a_minutos("PT18H"), 1080)
        self.assertEqual(fuentes.duracion_iso_a_minutos("P1DT2H"), 1560)
        self.assertEqual(fuentes.duracion_iso_a_minutos(None), 0)

    def test_normalizar_titulo(self):
        self.assertEqual(normalizar_titulo("Marvel's Spider-Man 2™"), "marvel s spider man 2")
        self.assertEqual(normalizar_titulo("Pokémon Escarlata"), "pokemon escarlata")
        self.assertEqual(normalizar_titulo("Hogwarts Legacy PS4 & PS5"), "hogwarts legacy")
        self.assertEqual(normalizar_titulo("Tetris Effect (PS5)"), "tetris effect")

    def test_sesiones_fin_exclusivo(self):
        f = campos_sesiones(9, [{"id": -1, "inicio": "2026-12-31", "horas": 1, "minutos": 5}])
        self.assertEqual(f["dates[9][0][range_end_date]"], "2027-01-01")
        self.assertEqual(f["dates[9][0][edited]"], "true")
        s = normalizar_sesiones([{"id": 1, "range_start_date": "2026-12-31", "range_end_date": "2027-01-01"}])
        self.assertEqual(s[0]["fin"], "2026-12-31")


class HttpGuion:
    """Devuelve respuestas fijas según el comienzo de la URL."""

    def __init__(self, rutas):
        self.rutas = rutas
        self.llamadas = []

    def peticion(self, metodo, url, **kw):
        self.llamadas.append((metodo, url, kw))
        for prefijo, datos in self.rutas.items():
            if url.startswith(prefijo):
                if isinstance(datos, Respuesta):
                    return datos
                return Respuesta(200, url, [], json.dumps(datos).encode())
        return Respuesta(404, url, [], b"")


class TestFuentes(unittest.TestCase):
    def test_steam(self):
        http = HttpGuion({"https://api.steampowered.com": {"response": {"games": [
            {"appid": 620, "name": "Portal 2", "playtime_forever": 125, "rtime_last_played": 1762400000}]}}})
        t = fuentes.leer_steam(http, "k", "1")
        self.assertEqual((t[0].clave, t[0].minutos, t[0].plataforma), ("steam:620", 125, 6))
        self.assertIsNotNone(t[0].ultima_vez)

    def test_steam_perfil_privado(self):
        http = HttpGuion({"https://api.steampowered.com": {"response": {}}})
        with self.assertRaises(fuentes.ErrorFuente):
            fuentes.leer_steam(http, "k", "1")

    def test_psn(self):
        http = HttpGuion({fuentes.PSN_JUEGOS: {"titles": [
            {"titleId": "PPSA01", "name": "Astro Bot", "category": "ps5_native_game",
             "playDuration": "PT2H30M", "lastPlayedDateTime": "2026-11-05T21:00:00Z"}], "nextOffset": 0}})
        t = fuentes.leer_psn(http, {"access_token": "a"})
        self.assertEqual((t[0].clave, t[0].minutos, t[0].plataforma), ("psn:PPSA01", 150, 167))

    def test_psn_npsso(self):
        http = HttpGuion({
            f"{fuentes.PSN_AUTH}/authorize": Respuesta(302, "", [("Location", "com.scee.psxandroid.scecompcall://redirect/?code=v3.ABC&cid=x")]),
            f"{fuentes.PSN_AUTH}/token": {"access_token": "a", "refresh_token": "r"},
        })
        self.assertEqual(fuentes.psn_token(http, npsso="n")["refresh_token"], "r")
        self.assertEqual(http.llamadas[1][2]["form"]["code"], "v3.ABC")

    def test_epic(self):
        http = HttpGuion({
            f"{fuentes.EPIC_LIBRERIA}/playtime": [{"artifactId": "Sugar", "totalTime": 5400}],
            f"{fuentes.EPIC_LIBRERIA}/items": {"records": [
                {"appName": "Sugar", "namespace": "ns", "catalogItemId": "cat"}], "responseMetadata": {}},
            fuentes.EPIC_CATALOGO: {"cat": {"title": "Rocket League"}},
        })
        t = fuentes.leer_epic(http, {"access_token": "a", "account_id": "acc"})
        self.assertEqual((t[0].clave, t[0].nombre, t[0].minutos), ("epic:Sugar", "Rocket League", 90))

    def test_nintendo(self):
        http = HttpGuion({
            f"{fuentes.NINTENDO_CUENTAS}/api/token": {"id_token": "i", "access_token": "a"},
            f"{fuentes.NINTENDO_MOON}/v2/actions/user/fetchOwnedDevices": {"ownedDevices": [
                {"deviceId": "D1", "device": {"platformGeneration": "P00"}}]},
            f"{fuentes.NINTENDO_MOON}/v2/actions/playSummary/fetchDailySummaries": {"dailySummaries": [
                {"date": "2026-11-06", "players": [
                    {"profile": {"nickname": "Davicilio"}, "playedGames": [
                        {"meta": {"applicationId": "0100152000022000", "title": "Mario Kart 8 Deluxe"},
                         "playingTime": 45}]},
                    {"profile": {"nickname": "Otro"}, "playedGames": [
                        {"meta": {"applicationId": "0100152000022000", "title": "Mario Kart 8 Deluxe"},
                         "playingTime": 30}]}]}]},
        })
        d = fuentes.leer_nintendo(http, "s", "Europe/Madrid", jugador="davicilio")
        self.assertEqual([(x.clave, x.fecha, x.minutos, x.plataforma) for x in d],
                         [("nintendo:0100152000022000", "2026-11-06", 45, 130)])
        cab = http.llamadas[1][2]["cabeceras"]
        self.assertEqual(cab["Authorization"], "Bearer i")

    def test_nintendo_url_login(self):
        url, verificador = fuentes.nintendo_url_login()
        self.assertIn("session_token_code_challenge=", url)
        self.assertGreater(len(verificador), 40)


if __name__ == "__main__":
    unittest.main()


class TestLineaDeOrdenes(unittest.TestCase):
    """La orden completa, con Steam y Backloggd falsos, guardando el estado en disco."""

    def test_dos_ejecuciones(self):
        import tempfile
        from unittest import mock

        import sincronizar

        falso = BackloggdFalso()
        minutos = {"v": 100}

        class HttpFalso:
            def peticion(self, metodo, url, **kw):
                if url.startswith("https://api.steampowered.com"):
                    datos = {"response": {"games": [
                        {"appid": 367520, "name": "Hollow Knight", "playtime_forever": minutos["v"]}]}}
                    return Respuesta(200, url, [], json.dumps(datos).encode())
                return falso.peticion(metodo, url, **kw)

        with tempfile.TemporaryDirectory() as carpeta, \
                mock.patch.object(sincronizar, "Http", HttpFalso), \
                mock.patch("bsync.backloggd.ESPERA_ESCRITURA", 0), \
                mock.patch.dict(os.environ, {"STEAM_API_KEY": "k", "STEAM_ID": "1", "BACKLOGGD_COOKIE": "c"},
                                clear=True), \
                mock.patch("sys.stdout"):
            ruta = os.path.join(carpeta, "estado.json")
            self.assertEqual(sincronizar.main(["--estado", ruta]), 0)
            minutos["v"] = 140
            self.assertEqual(sincronizar.main(["--estado", ruta]), 0)
            with open(ruta, encoding="utf-8") as f:
                estado = json.load(f)
        self.assertEqual(estado["tokens"]["backloggd_cookie"], "c")
        self.assertEqual(estado["asociaciones"]["steam:367520"]["id"], 100)
        s = falso.sesiones(100)
        self.assertEqual(len(s), 1)
        self.assertEqual(s[0]["minutos"], 40)
