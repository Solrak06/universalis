"""Pruebas de las herramientas de estudio (estudio.py): plan del día, resúmenes, preguntas de desarrollo,
análisis, trucos y protocolo. Sin Ollama: la IA se simula.

Ejecutar:  python profesor/pruebas/test_estudio.py
"""
import os
import json
import subprocess
import sys
import threading
import time
import unittest
from datetime import date, timedelta
from pathlib import Path

os.environ["UNIVERSALIS_SIN_RESERVA"] = "1"  # sin trabajo de IA en segundo plano
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import ia  # noqa: E402
import motor as motor_mod  # noqa: E402
from test_profesor import IAFalsa, RAIZ, Temario  # noqa: E402


class IAEstudio(IAFalsa):
    """IA falsa con las llamadas de estudio. `lenta` hace que cada llamada tarde hasta que se cancela."""

    def __init__(self, lenta=False):
        super().__init__()
        self.lenta = lenta
        self.pedidas = []

    def __enter__(self):
        super().__enter__()
        self.otras = (ia.resumir, ia.pregunta_abierta, ia.corregir_abierta, ia.analizar, ia.truco)
        ia.resumir, ia.pregunta_abierta, ia.corregir_abierta, ia.analizar, ia.truco = (
            self.resumir, self.pregunta_abierta, self.corregir_abierta, self.analizar, self.truco)
        return self

    def __exit__(self, *a):
        ia.resumir, ia.pregunta_abierta, ia.corregir_abierta, ia.analizar, ia.truco = self.otras
        super().__exit__(*a)

    def _esperar(self, al_recibir, cancelar, texto):
        if al_recibir:
            al_recibir(texto[:20])
        if self.lenta:
            for _ in range(200):
                if cancelar is not None and cancelar.is_set():
                    raise ia.Cancelado()
                time.sleep(0.02)

    def resumir(self, modelo, asignatura, tema, pasaje, al_recibir=None, cancelar=None):
        self.pedidas.append(('resumir', tema, len(pasaje)))
        self._esperar(al_recibir, cancelar, '## Ideas clave\n- Algo')
        return f'## Ideas clave\n- El tema **{tema}** trata de cosas importantes del apunte.\n## Conceptos\n- **X**: y'

    def pregunta_abierta(self, modelo, asignatura, tema, pasaje, dificultad='Intermedia', evitar=(),
                         al_recibir=None, cancelar=None):
        self.pedidas.append(('abierta', tema, list(evitar)))
        self._esperar(al_recibir, cancelar, '{"pregunta"')
        return {'enunciado': f'Explica la idea principal de {tema} (versión {len(evitar) + 1}).',
                'puntos': ['Define el concepto principal', 'Relaciona con la generalización', 'Pone un ejemplo'],
                'respuesta': 'El concepto principal del tema permite generalizar a datos nuevos, por ejemplo al '
                             'clasificar ejemplos que el modelo no ha visto nunca.'}

    def corregir_abierta(self, modelo, asignatura, tema, pasaje, pregunta, puntos, respuesta, al_recibir=None,
                         cancelar=None):
        self.pedidas.append(('corregir', respuesta))
        self._esperar(al_recibir, cancelar, '{"puntos"')
        return {'nota': 7.5, 'puntos': [True, True, False], 'errores': ['Confunde entrenamiento con prueba.'],
                'comentario': 'Bien, pero falta un **ejemplo**.'}

    def analizar(self, modelo, asignatura, datos, al_recibir=None, cancelar=None):
        self.pedidas.append(('analizar', datos))
        self._esperar(al_recibir, cancelar, '## Cómo vas')
        return '## Cómo vas\nBien.\n## Puntos débiles\n- **Evaluacion**'

    def truco(self, modelo, asignatura, frente, reverso, al_recibir=None, cancelar=None):
        self.pedidas.append(('truco', frente))
        self._esperar(al_recibir, cancelar, 'Piensa en')
        return 'Piensa en una **receta**: los ingredientes son los datos.'


class PruebasEstudio(unittest.TestCase):
    def setUp(self):
        self.t = Temario()
        self.eventos = []
        self.motor = motor_mod.Motor(self.t.datos, self.eventos.append)

    def tearDown(self):
        self.motor.detener_generacion()
        self.motor.detener_resumenes()
        self.motor.banco.cerrar()
        self.t.cerrar()

    def abrir(self, examenes=None, con_ia=False):
        if con_ia:
            return self.motor.hola('IA', 'Inteligencia Artificial', str(self.t.temario), examenes)
        original = ia.modelos
        ia.modelos = lambda timeout=2.0: None
        try:
            return self.motor.hola('IA', 'Inteligencia Artificial', str(self.t.temario), examenes)
        finally:
            ia.modelos = original

    def sin_ia(self):
        self.motor.ia_estado['disponible'] = False
        self.motor.comprobar_ia = lambda preferido='': self.motor.ia_estado

    # ------------------------------------------------------------------ inicio

    def test_hola_trae_inicio_plan_y_examen(self):
        manana = (date.today() + timedelta(days=2)).isoformat()
        r = self.abrir([{'nombre': 'Parcial 1', 'fecha': manana, 'peso': 25},
                        {'nombre': 'Ya pasó', 'fecha': '2000-01-01', 'peso': 10},
                        {'nombre': 'Con nota', 'fecha': manana, 'peso': 10, 'calificado': True},
                        {'nombre': 'Sin fecha', 'fecha': 'mañana'}])
        inicio = r['inicio']
        self.assertEqual(inicio['examen']['nombre'], 'Parcial 1')
        self.assertEqual(inicio['examen']['dias'], 2)
        tipos = [t['tipo'] for t in inicio['plan']]
        self.assertIn('tarjetas', tipos)
        self.assertIn('simulacro', tipos)  # el examen está cerca
        self.assertIn('resumen', tipos)    # hay temas sin practicar
        self.assertTrue(all(t['titulo'] and 'hecha' in t for t in inicio['plan']))
        self.assertEqual(inicio['herramientas']['temas'], 3)
        self.assertEqual(r['chat'], [])

    def test_plan_marca_tareas_hechas(self):
        self.abrir()
        token = self.motor.empezar({'modo': 'rapido', 'n': 5})['sesion']['token']
        s = self.motor.banco.sesion(token)
        for i, e in enumerate(s['preguntas']):  # falla todas
            p = self.motor.banco.pregunta(e['id'])
            self.motor.responder(token, i, (e['orden'].index(p['respuesta']) + 1) % len(e['orden']))
        self.motor.terminar(token)
        plan = self.motor.plan()
        tipos = {t['tipo']: t for t in plan}
        self.assertIn('repaso', tipos)  # las falladas vuelven hoy
        self.assertIn('tema', tipos)    # y hay un tema flojo que practicar
        self.assertFalse(tipos['tema']['hecha'] and tipos['tema']['detalle'] == '')
        # Simulacro: no cambia los ajustes guardados y queda marcado en el resultado.
        antes = self.motor.banco.ajustes('IA')
        r = self.motor.empezar({'modo': 'rapido', 'n': 5, 'simulacro': True})
        self.assertEqual(r['sesion']['config']['correccion'], 'final')
        self.assertTrue(r['sesion']['config']['penalizar'])
        self.assertEqual(r['sesion']['config']['limite'], 60)
        self.assertEqual(self.motor.banco.ajustes('IA'), antes)
        fin = self.motor.terminar(r['sesion']['token'])
        self.assertTrue(fin['resultado']['simulacro'])

    # --------------------------------------------------------------- resúmenes

    def test_resumen_sin_ia_y_con_ia(self):
        self.abrir()
        self.sin_ia()
        temas = self.motor.resumenes()['temas']
        self.assertEqual({t['estado'] for t in temas}, {'no'})
        ruta = next(t['ruta'] for t in temas if 'Aprendizaje' in t['tema'])
        r = self.motor.resumen(ruta)
        self.assertEqual(r['origen'], 'auto')
        self.assertIn('## Ideas clave', r['texto'])
        self.assertIn('## Conceptos', r['texto'])
        self.assertIn('**', r['texto'])
        self.assertTrue(r['sin_ia'])
        self.assertEqual(next(t for t in self.motor.resumenes()['temas'] if t['ruta'] == ruta)['estado'], 'auto')
        # Con IA: se rehace (el automático no cuenta como hecho) y después se reutiliza.
        self.motor.ia_estado.update(disponible=True, modelo='falso:latest')
        with IAEstudio() as falsa:
            r = self.motor.resumen(ruta, id_peticion=7)
            self.assertEqual(r['origen'], 'ia')
            self.assertIn('Aprendizaje', r['texto'])
            self.motor.resumen(ruta)
            self.assertEqual(len([p for p in falsa.pedidas if p[0] == 'resumir']), 1)
        self.assertTrue(any(e.get('evento') == 'parcial' and e['id'] == 7 and e['tipo'] == 'resumen'
                            for e in self.eventos))
        with self.assertRaises(motor_mod.ErrorProfesor):
            self.motor.resumen('no_existe.pdf')

    def test_resumen_si_cambia_el_documento_queda_antiguo(self):
        self.abrir()
        self.sin_ia()
        ruta = self.motor.resumenes()['temas'][0]['ruta']
        self.motor.resumen(ruta)
        archivo = self.t.temario / ruta
        archivo.write_text(archivo.read_text(encoding='utf-8') + '\nUna frase nueva bastante larga para cambiar.',
                           encoding='utf-8')
        time.sleep(1.1)
        archivo.touch()
        self.motor.indexar()
        self.assertEqual(next(t for t in self.motor.resumenes()['temas'] if t['ruta'] == ruta)['estado'], 'antiguo')

    def test_pasaje_completo_respeta_el_limite(self):
        self.abrir()
        ruta = self.motor.resumenes()['temas'][0]['ruta']
        self.motor.fragmentos = [{'ruta': ruta, 'ubicacion': f'página {i}', 'texto': 'Frase de relleno. ' * 60}
                                 for i in range(40)]
        pasaje = self.motor._pasaje_completo(ruta, 3000)
        self.assertLessEqual(len(pasaje), 3200)
        self.assertGreater(len(pasaje), 1500)

    def test_resumir_todos_en_segundo_plano_y_detener(self):
        self.abrir()
        self.motor.ia_estado.update(disponible=True, modelo='falso:latest')
        with IAEstudio(lenta=True):
            r = self.motor.resumir_todos()
            self.assertEqual(r['pendientes'], 3)
            self.assertTrue(self.motor.resumenes()['trabajando'])
            time.sleep(0.2)
            self.motor.detener_resumenes()
            self.motor.hilo_resumenes.join(timeout=5)
        self.assertFalse(self.motor.hilo_resumenes.is_alive())
        self.assertTrue(any(e.get('evento') == 'resumenes' and e['estado'] == 'terminado' for e in self.eventos))
        # Sin IA no se puede.
        self.sin_ia()
        with self.assertRaises(motor_mod.ErrorProfesor):
            self.motor.resumir_todos()

    # -------------------------------------------------------------- desarrollo

    def test_desarrollo_sin_ia(self):
        self.abrir()
        self.sin_ia()
        r = self.motor.abierta()
        p = r['pregunta']
        self.assertTrue(r['sin_ia'])
        self.assertEqual(p['origen'], 'auto')
        self.assertTrue(p['enunciado'].startswith('¿Qué'))
        self.assertNotIn('puntos', p)        # la solución no se envía antes de responder
        self.assertNotIn('respuesta', p)
        guardada = self.motor.banco.abierta(p['id'])
        respuesta = guardada['respuesta']    # copiar la respuesta modelo cubre todos los puntos
        c = self.motor.corregir(p['id'], respuesta)['correccion']
        self.assertEqual(c['origen'], 'auto')
        self.assertEqual(c['nota'], 10.0)
        self.assertTrue(all(c['puntos']))
        self.assertEqual(c['respuesta_modelo'], respuesta)
        mala = self.motor.corregir(p['id'], 'No me acuerdo de nada de esto, lo siento mucho de verdad.')['correccion']
        self.assertLess(mala['nota'], 5)
        self.assertEqual(mala['intentos'], 2)
        self.assertEqual(mala['mejor'], 10.0)
        # En blanco: se enseña la solución pero no cuenta como intento.
        blanco = self.motor.corregir(p['id'], '')['correccion']
        self.assertEqual(blanco['nota'], 0)
        self.assertEqual(blanco['intentos'], 2)
        hist = self.motor.desarrollo()
        self.assertEqual(len(hist['intentos']), 2)
        self.assertTrue(hist['intentos'][0]['tema'])
        # Cuenta como actividad de hoy.
        self.assertGreater(self.motor.banco.actividad('IA').get(date.today().isoformat(), 0), 0)

    def test_desarrollo_con_ia_reutiliza_y_evita_repetir(self):
        self.abrir()
        self.motor.ia_estado.update(disponible=True, modelo='falso:latest')
        ruta = self.motor.resumenes()['temas'][1]['ruta']
        with IAEstudio() as falsa:
            p = self.motor.abierta(ruta, id_peticion=3)['pregunta']
            self.assertEqual(p['origen'], 'ia')
            self.assertEqual(p['ruta'], ruta)
            self.assertEqual(p['n_puntos'], 3)
            # Mientras no la respondas, se reutiliza (no se gasta otra llamada a la IA).
            self.assertEqual(self.motor.abierta(ruta)['pregunta']['id'], p['id'])
            c = self.motor.corregir(p['id'], 'El concepto sirve para generalizar a datos nuevos sin memorizar.', id_peticion=4)
            self.assertEqual(c['correccion']['nota'], 7.5)
            self.assertEqual(c['correccion']['puntos'], [True, True, False])
            self.assertEqual(c['correccion']['errores'], ['Confunde entrenamiento con prueba.'])
            self.assertIn('puntos', c['pregunta'])
            # Ya respondida: la siguiente es nueva y se pide evitando la anterior.
            q = self.motor.abierta(ruta)['pregunta']
            self.assertNotEqual(q['id'], p['id'])
            self.assertIn(p['enunciado'], [x for x in falsa.pedidas if x[0] == 'abierta'][-1][2])
            self.assertEqual(self.motor.descartar_abierta(q['id'])['mensaje'][:2], 'De')
            self.assertNotIn(q['id'], {a['id'] for a in self.motor.banco.abiertas('IA')})
        with self.assertRaises(motor_mod.ErrorProfesor):
            self.motor.corregir('inventada', 'algo')

    def test_correccion_de_la_ia_no_contradice_los_puntos(self):
        original = ia.chat
        ia.chat = lambda *a, **k: json.dumps({'puntos': [False, False, False], 'errores': [], 'comentario': 'x',
                                               'nota': 9.5})
        try:
            c = ia.corregir_abierta('m', 'IA', 'T', 'pasaje', 'P', ['a', 'b', 'c'], 'respuesta')
        finally:
            ia.chat = original
        self.assertLessEqual(c['nota'], 3.0)

    def test_cancelar_tarea_de_ia(self):
        self.abrir()
        self.motor.ia_estado.update(disponible=True, modelo='falso:latest')
        ruta = self.motor.resumenes()['temas'][0]['ruta']
        errores = []

        def pedir():
            try:
                self.motor.abierta(ruta, id_peticion=55)
            except motor_mod.ErrorProfesor as e:
                errores.append(str(e))

        with IAEstudio(lenta=True):
            hilo = threading.Thread(target=pedir)
            hilo.start()
            time.sleep(0.2)
            self.motor.cancelar(55)
            hilo.join(timeout=5)
        self.assertEqual(errores, ['Pregunta cancelada.'])

    # ---------------------------------------------------------------- análisis

    def test_analisis_sin_ia_y_con_ia(self):
        self.abrir([{'nombre': 'Final', 'fecha': (date.today() + timedelta(days=9)).isoformat(), 'peso': 40}])
        self.sin_ia()
        r = self.motor.analisis()
        self.assertEqual(r['origen'], 'auto')
        self.assertIn('## Cómo vas', r['texto'])
        self.assertIn('Final', r['texto'])
        # Se guarda: la segunda vez no se recalcula salvo que se pida.
        self.assertEqual(self.motor.analisis()['texto'], r['texto'])
        self.motor.ia_estado.update(disponible=True, modelo='falso:latest')
        self.motor.empezar({'modo': 'rapido', 'n': 5})
        with IAEstudio() as falsa:
            r = self.motor.analisis(rehacer=True, id_peticion=9)
        self.assertEqual(r['origen'], 'ia')
        datos = [p for p in falsa.pedidas if p[0] == 'analizar'][0][1]
        self.assertIn('Próxima prueba: Final en 9 días', datos)
        self.assertIn('sin practicar', datos)

    # ------------------------------------------------------------------ trucos

    def test_truco_se_guarda_y_sin_ia_avisa(self):
        self.abrir()
        self.motor.ia_estado.update(disponible=True, modelo='falso:latest')
        with IAEstudio() as falsa:
            r = self.motor.truco('g123', '¿Qué es X?', 'Una cosa', id_peticion=1)
            self.assertIn('**receta**', r['texto'])
            self.motor.truco('g123')
            self.assertEqual(len([p for p in falsa.pedidas if p[0] == 'truco']), 1)
        self.sin_ia()
        with self.assertRaises(motor_mod.ErrorProfesor):
            self.motor.truco('otra', 'a', 'b')

    def test_duda_sobre_pregunta_de_mis_fallos(self):
        with IAFalsa() as falsa:
            self.motor.hola('IA', 'Inteligencia Artificial', str(self.t.temario))
            token = self.motor.empezar({'modo': 'rapido', 'n': 5})['sesion']['token']
            ident = self.motor.pregunta(token, 0)['id']
            capturado = {'contextos': []}
            original = falsa.responder_duda

            def responder(modelo, asignatura, duda, pasajes, contexto='', **k):
                capturado['contextos'].append(contexto)
                return original(modelo, asignatura, duda, pasajes, contexto, **k)

            ia.responder_duda = responder
            self.motor.duda('¿Por qué?', pregunta=ident, etiqueta='Sobre una pregunta de «Mis fallos»')
            self.motor.duda('¿Y esta tarjeta?', pregunta='Tarjeta: ¿Qué es la curva ROC? -> una gráfica')
        self.assertIn('Respuesta correcta:', capturado['contextos'][0])
        self.assertIn('curva ROC', capturado['contextos'][1])
        self.assertEqual(self.motor.conversacion()['chat'][0]['contexto'], 'Sobre una pregunta de «Mis fallos»')


class PruebasTexto(unittest.TestCase):
    def test_texto_rico(self):
        md = '### Ideas\n* **Uno**: a\n  * dos\n1) tres\n**Conceptos:**\n| a | b |\n|---|---|\n| c | d |\nsin *cursiva*'
        self.assertEqual(ia.texto_rico(md), '## Ideas\n- **Uno**: a\n  - dos\n1. tres\n## Conceptos\na · b\nc · d\nsin cursiva')

    def test_latex(self):
        casos = {'IA \\supset AA \\supset AP': 'IA ⊃ AA ⊃ AP',
                 '$\\alpha \\leq 1$ y $$x \\in A$$': 'α ≤ 1 y x ∈ A',
                 '\\leftarrow y \\le 3': '← y ≤ 3',
                 '\\sqrt{n} y \\mathcal{L}(\\theta)': '√(n) y L(θ)',
                 'coste del 5\\% y \\hat{y}': 'coste del 5% y y'}
        for entrada, salida in casos.items():
            self.assertEqual(ia.texto_rico(entrada), salida)

    def test_reparar_escapes_perdidos_en_json(self):
        # La IA escribe «\text» sin escapar dentro del JSON: llega un tabulador seguido de «ext».
        roto = json.loads('{"e": "baja $1 - \\text{especificidad}$ y \\frac{a}{b}"}')['e']
        self.assertIn('\t', roto)
        self.assertEqual(ia.reparar(roto), 'baja 1 - especificidad y (a)/(b)')
        self.assertEqual(ia.reparar('•Una curva ROC'), 'Una curva ROC')
        self.assertEqual(ia.texto_rico('línea\r\nsiguiente'), 'línea\nsiguiente')


class PruebasProtocoloEstudio(unittest.TestCase):
    def test_ordenes_de_estudio(self):
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
            r = pedir(id=1, cmd='hola', codigo='IA', nombre='IA', temario=str(t.temario),
                      examenes=[{'nombre': 'P1', 'fecha': date.today().isoformat(), 'peso': 20}])
            self.assertTrue(r['ok'], r)
            self.assertEqual(r['inicio']['examen']['dias'], 0)
            ruta = r['archivos'][0]['ruta']
            r = pedir(id=2, cmd='abierta')
            self.assertTrue(r['ok'], r)
            abierta = r['pregunta']['id']
            for i, orden in enumerate([{'cmd': 'inicio'}, {'cmd': 'examenes', 'examenes': []},
                                       {'cmd': 'resumenes'}, {'cmd': 'resumen', 'ruta': ruta},
                                       {'cmd': 'desarrollo'}, {'cmd': 'corregir', 'pregunta': abierta,
                                                               'respuesta': 'una respuesta cualquiera de prueba'},
                                       {'cmd': 'descartar_abierta', 'pregunta': abierta},
                                       {'cmd': 'analisis'}, {'cmd': 'conversacion'}, {'cmd': 'nueva_conversacion'},
                                       {'cmd': 'detener_resumenes'}]):
                r = pedir(id=200 + i, **orden)
                self.assertTrue(r['ok'], (orden, r))
            self.assertFalse(pedir(id=300, cmd='resumir_todos')['ok'])  # sin IA
            self.assertFalse(pedir(id=301, cmd='truco', tarjeta='x')['ok'])
            proceso.stdin.write('{"cmd": "cancelar", "tarea": 5}\n{"cmd": "salir"}\n')
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
