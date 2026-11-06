"""Pruebas de herramientas.py: buscador, glosario, mapa mental, nota estimada, calendario, tiempo de
estudio, logros y exportación. Sin Ollama: la IA se simula.

Ejecutar:  python profesor/pruebas/test_herramientas.py
"""
import os
import json
import subprocess
import sys
import unittest
from datetime import date, timedelta
from pathlib import Path

os.environ["UNIVERSALIS_SIN_RESERVA"] = "1"  # sin trabajo de IA en segundo plano
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import herramientas  # noqa: E402
import ia  # noqa: E402
import motor as motor_mod  # noqa: E402
from test_profesor import RAIZ, Temario  # noqa: E402


class PruebasHerramientas(unittest.TestCase):
    def setUp(self):
        self.t = Temario()
        self.eventos = []
        self.motor = motor_mod.Motor(self.t.datos, self.eventos.append)
        original = ia.modelos
        ia.modelos = lambda timeout=2.0: None
        try:
            self.motor.hola('IA', 'Inteligencia Artificial', str(self.t.temario),
                            [{'nombre': 'Parcial', 'fecha': (date.today() + timedelta(days=5)).isoformat(), 'peso': 30}])
        finally:
            ia.modelos = original
        self.motor.comprobar_ia = lambda preferido='': self.motor.ia_estado

    def tearDown(self):
        self.motor.detener_generacion()
        self.motor.banco.cerrar()
        self.t.cerrar()

    def responder(self, acertar, n=5):
        token = self.motor.empezar({'modo': 'rapido', 'n': n})['sesion']['token']
        s = self.motor.banco.sesion(token)
        for i, e in enumerate(s['preguntas']):
            p = self.motor.banco.pregunta(e['id'])
            buena = e['orden'].index(p['respuesta'])
            self.motor.responder(token, i, buena if acertar else (buena + 1) % len(e['orden']))
        return self.motor.terminar(token)

    # ------------------------------------------------- tests desde el lector

    def test_test_de_unas_paginas(self):
        ruta = sorted({f['ruta'] for f in self.motor.fragmentos})[0]
        ubicacion = next(f['ubicacion'] for f in self.motor.fragmentos if f['ruta'] == ruta)
        r = self.motor.empezar({'modo': 'mixto', 'n': 5, 'temas': [ruta], 'paginas': [ubicacion]})
        s = self.motor.banco.sesion(r['sesion']['token'])
        self.assertEqual(s['config']['modo'], 'rapido')  # sin esperar a la IA
        self.assertTrue(s['preguntas'])
        for e in s['preguntas']:
            p = self.motor.banco.pregunta(e['id'])
            self.assertEqual((p['ruta'], p['ubicacion']), (ruta, ubicacion))
        # No cambia los ajustes guardados del alumno
        self.assertEqual(self.motor.banco.ajustes('IA').get('paginas'), None)

    def test_test_de_un_texto_seleccionado(self):
        ruta = sorted({f['ruta'] for f in self.motor.fragmentos})[0]
        texto = ' '.join(f['texto'] for f in self.motor.fragmentos if f['ruta'] == ruta)[:1200]
        r = self.motor.empezar({'n': 5, 'temas': [ruta], 'texto': texto, 'ubicacion': 'página 1'})
        s = self.motor.banco.sesion(r['sesion']['token'])
        self.assertTrue(0 < len(s['preguntas']) <= 5)
        for e in s['preguntas']:
            p = self.motor.banco.pregunta(e['id'])
            self.assertEqual(p['ruta'], ruta)
            self.assertIn(p['cita'][:30], texto)  # todas salen del trozo elegido
        with self.assertRaises(motor_mod.ErrorProfesor):
            self.motor.empezar({'n': 5, 'temas': [ruta], 'texto': 'Hola.'})

    # ------------------------------------------------------------- buscador

    def test_glosario_y_buscador(self):
        g = self.motor.glosario()['glosario']
        terminos = [x['termino'] for x in g]
        self.assertIn('Curva ROC', terminos)
        self.assertEqual(terminos, sorted(terminos, key=lambda t: t.lower()))
        self.assertTrue(all(x['definicion'].endswith('.') and x['tema'] for x in g))
        r = self.motor.buscar('validación cruzada')
        self.assertTrue(r['resultados'])
        self.assertIn('**', r['resultados'][0]['extracto'])
        self.assertIn('Aprendizaje', r['resultados'][0]['tema'])
        self.assertEqual(r['conceptos'][0]['termino'], 'Validación cruzada')
        self.assertEqual(self.motor.buscar('x')['resultados'], [])
        self.assertEqual(self.motor.buscar('zzzz qqqq')['resultados'], [])

    # ---------------------------------------------------------------- mapa

    def test_mapa_sin_ia_y_con_ia(self):
        ruta = self.motor.resumenes()['temas'][0]['ruta']
        m = self.motor.mapa(ruta)
        self.assertEqual(m['origen'], 'auto')
        self.assertGreaterEqual(len(m['ramas']), 3)
        self.assertTrue(all(r['titulo'] and r['ideas'] for r in m['ramas']))
        self.motor.ia_estado.update(disponible=True, modelo='falso:latest')
        original = ia.chat
        respuesta = {'centro': 'Aprendizaje automático', 'ramas': [
            {'titulo': 'Supervisado', 'ideas': ['Usa ejemplos etiquetados', 'Predice etiquetas nuevas']},
            {'titulo': 'No supervisado', 'ideas': ['Sin etiquetas']},
            {'titulo': 'Sobreajuste con un título larguísimo que hay que recortar', 'ideas': ['Memoriza']}]}
        ia.chat = lambda *a, **k: json.dumps(respuesta)
        try:
            m = self.motor.mapa(ruta, id_peticion=3)
        finally:
            ia.chat = original
        self.assertEqual(m['origen'], 'ia')
        self.assertEqual(m['centro'], 'Aprendizaje automático')
        self.assertTrue(m['ramas'][2]['titulo'].endswith('…'))
        # Guardado: no se vuelve a pedir.
        ia.chat = lambda *a, **k: self.fail('no debería llamar a la IA')
        try:
            self.assertEqual(self.motor.mapa(ruta)['origen'], 'ia')
        finally:
            ia.chat = original

    # -------------------------------------------------- examen y calendario

    def test_prediccion_y_calendario(self):
        self.assertIsNone(self.motor.prediccion())
        self.responder(acertar=True)
        p = self.motor.prediccion()
        self.assertGreater(p['nota'], 3)
        self.assertTrue(p['consejo'])
        bien = p['nota']
        self.responder(acertar=False, n=10)
        self.assertLess(self.motor.prediccion()['nota'], bien)
        c = self.motor.calendario()
        self.assertEqual(c['examen']['nombre'], 'Parcial')
        dias = c['dias']
        self.assertEqual(dias[0]['etiqueta'], 'Hoy')
        self.assertEqual(dias[-1]['tipo'], 'examen')
        self.assertEqual(dias[-2]['tipo'], 'simulacro')
        repartidos = [t for d in dias if d['tipo'] == 'temas' for t in d['tareas']]
        self.assertEqual(len(dias), 6)
        self.assertGreaterEqual(len(repartidos), 3)  # todos los temas aparecen
        self.motor.poner_examenes([])
        self.assertIsNone(self.motor.calendario())
        self.motor.poner_examenes([{'nombre': 'Hoy', 'fecha': date.today().isoformat(), 'peso': 10}])
        self.assertEqual(self.motor.calendario()['dias'][0]['tipo'], 'examen')

    # --------------------------------------------------------------- logros

    def test_logros_nivel_y_tiempo(self):
        l = self.motor.logros()
        self.assertEqual(l['nivel'], 1)
        self.assertEqual(l['conseguidos'], 0)
        self.assertEqual(len(l['logros']), len(herramientas.LOGROS))
        self.responder(acertar=True)
        self.motor.registrar_estudio(25)
        l = self.motor.logros()
        conseguidos = {x['id'] for x in l['logros'] if x['conseguido']}
        self.assertIn('primer_test', conseguidos)
        self.assertEqual(l['minutos'], 25)
        self.assertEqual(l['minutos_hoy'], 25)
        # Temario: se lista, se lee página a página y se marca como leído
        temario = self.motor.temario()
        self.assertGreater(temario['total'], 0)
        self.assertEqual(temario['leidos'], 0)
        ruta = next(t['ruta'] for t in temario['temas'] if t['legibles'])
        doc = self.motor.leer(ruta)
        self.assertTrue(doc['paginas'])
        self.assertGreater(doc['palabras'], 0)
        self.assertFalse(doc['leido'])
        self.motor.marcar_leido(ruta)
        self.assertTrue(self.motor.leer(ruta)['leido'])
        self.assertEqual(self.motor.temario()['leidos'], 1)
        self.motor.marcar_leido(ruta, False)
        self.assertEqual(self.motor.leer(ruta)['posicion'], 0)
        self.motor.posicion_lectura(ruta, 0.4)
        self.assertEqual(self.motor.leer(ruta)['posicion'], 0.4)
        t = self.motor.temario()
        self.assertEqual(t['ultimo'], ruta)
        self.assertEqual(next(d for d in t['temas'] if d['ruta'] == ruta)['posicion'], 0.4)
        self.assertEqual(self.motor.tam_lectura(40)['tam'], 34)  # tope de la letra de lectura
        self.assertEqual(self.motor.temario()['tam'], 34)
        self.assertEqual(self.motor.temario()['leidos'], 0)
        # Grafica de los ultimos 14 dias: hoy es el ultimo y tiene los 25 minutos
        self.assertEqual(len(l['minutos_dias']), 14)
        self.assertEqual(l['minutos_dias'][-1]['minutos'], 25)
        self.assertEqual(sum(d['minutos'] for d in l['minutos_dias']), 25)
        self.assertGreater(l['xp'], 0)
        self.assertLessEqual(l['xp_nivel'], l['xp'])
        self.assertGreater(l['xp_siguiente'], l['xp'])
        maraton = next(x for x in l['logros'] if x['id'] == 'maraton')
        self.assertAlmostEqual(maraton['progreso'], 25 / 300, places=2)
        inicio = self.motor.inicio()
        self.assertEqual(inicio['nivel']['minutos_hoy'], 25)
        self.assertIsNotNone(inicio['prediccion'])
        self.assertIsNotNone(inicio['calendario'])

    # ------------------------------------------------------------ exportar

    def test_exportar_dossier_y_anki(self):
        self.responder(acertar=False)
        ruta = self.motor.resumenes()['temas'][0]['ruta']
        self.motor.resumen(ruta, usar_ia=False)
        r = self.motor.exportar('dossier')
        html = Path(r['archivo']).read_text(encoding='utf-8')
        self.assertIn('<h1>Inteligencia Artificial</h1>', html)
        self.assertIn('Glosario', html)
        self.assertIn('Mis fallos', html)
        self.assertIn('Calendario hasta el examen', html)
        self.assertIn('<li>', html)
        a = self.motor.exportar('anki')
        lineas = Path(a['archivo']).read_text(encoding='utf-8').splitlines()
        self.assertEqual(lineas[0], '#separator:tab')
        self.assertEqual(a['cantidad'], len(lineas) - 3)
        self.assertEqual(len(lineas[3].split('\t')), 3)
        with self.assertRaises(motor_mod.ErrorProfesor):
            self.motor.exportar('pdf')

    def test_markdown_html(self):
        h = herramientas._markdown_html('## Ideas\n- **Uno**: a\n- dos\nTexto <x>')
        self.assertEqual(h, '<h4>Ideas</h4>\n<ul>\n<li><b>Uno</b>: a</li>\n<li>dos</li>\n</ul>\n<p>Texto &lt;x&gt;</p>')


class PruebasProtocoloHerramientas(unittest.TestCase):
    def test_ordenes(self):
        t = Temario()
        proceso = subprocess.Popen([sys.executable, str(RAIZ / 'servidor.py'), str(t.datos)],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   encoding='utf-8', env={**__import__('os').environ,
                                                          'UNIVERSALIS_OLLAMA': 'http://127.0.0.1:9'})
        try:
            def pedir(**peticion):
                proceso.stdin.write(json.dumps(peticion, ensure_ascii=False) + '\n')
                proceso.stdin.flush()
                while True:
                    linea = json.loads(proceso.stdout.readline())
                    if linea.get('id') == peticion['id']:
                        return linea

            self.assertEqual(json.loads(proceso.stdout.readline())['evento'], 'listo')
            r = pedir(id=1, cmd='hola', codigo='IA', nombre='IA', temario=str(t.temario))
            self.assertTrue(r['ok'], r)
            self.assertIn('nivel', r['inicio'])
            ruta = r['archivos'][0]['ruta']
            for i, orden in enumerate([{'cmd': 'glosario'}, {'cmd': 'buscar', 'texto': 'sobreajuste'},
                                       {'cmd': 'mapa', 'ruta': ruta}, {'cmd': 'calendario'}, {'cmd': 'logros'},
                                       {'cmd': 'registrar_estudio', 'minutos': 25},
                                       {'cmd': 'exportar', 'tipo': 'dossier'}, {'cmd': 'exportar', 'tipo': 'anki'}]):
                r = pedir(id=400 + i, **orden)
                self.assertTrue(r['ok'], (orden, r))
            proceso.stdin.write('{"cmd": "salir"}\n')
            proceso.stdin.flush()
            self.assertEqual(proceso.wait(timeout=10), 0)
        finally:
            if proceso.poll() is None:
                proceso.kill()
            proceso.stdout.close()
            proceso.stderr.close()
            proceso.stdin.close()
            t.cerrar()


if __name__ == '__main__':
    unittest.main()
