"""Pruebas del motor del profesor.  Ejecutar:  python -m unittest discover profesor/pruebas

No necesitan Ollama: la IA se sustituye por una falsa que responde al instante.
"""
import os
import json
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import zipfile
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
os.environ["UNIVERSALIS_SIN_RESERVA"] = "1"  # sin trabajo de IA en segundo plano
sys.path.insert(0, str(RAIZ))

import ia  # noqa: E402
import motor as motor_mod  # noqa: E402
from generador import Generador, nombre_tema  # noqa: E402
from texto import clave, extraer, frases, limpiar  # noqa: E402

TEMA_1 = """Tema 1. Aprendizaje automático
• El aprendizaje supervisado es una técnica que utiliza ejemplos etiquetados para entrenar
un modelo capaz de predecir la etiqueta de ejemplos nuevos.
• El aprendizaje no supervisado es una técnica que descubre estructura en datos sin
etiquetas, por ejemplo agrupando los ejemplos parecidos.
• El sobreajuste es un fenómeno en el que el modelo memoriza los datos de entrenamiento y
pierde capacidad de generalización sobre datos nuevos.
• La validación cruzada es un procedimiento que divide los datos en varios bloques para
estimar el error de generalización de forma más fiable.
• Los algoritmos heurísticos son procedimientos iterativos que utilizan reglas generales para
mejorar sucesivamente la solución sin garantizar el óptimo.
4
"""

TEMA_2 = """Tema 2. Evaluación de modelos
La matriz de confusión es una tabla que resume los aciertos y errores de un clasificador
para cada una de las clases del problema.
La curva ROC es una gráfica que muestra la relación entre la tasa de verdaderos positivos y
la tasa de falsos positivos al variar el umbral de decisión.
La regularización es una técnica que penaliza la complejidad del modelo para reducir el
sobreajuste y mejorar la generalización en datos nuevos.
El descenso por gradiente es un algoritmo de optimización que actualiza los parámetros en
la dirección opuesta al gradiente de la función de pérdida.
Para evaluar la calidad predictiva se emplea una partición de prueba independiente de la
partición de entrenamiento, de modo que la estimación no esté sesgada.
"""


def crear_docx(ruta, parrafos):
    cuerpo = ''.join(f'<w:p><w:r><w:t>{p}</w:t></w:r></w:p>' for p in parrafos)
    xml = ('<?xml version="1.0" encoding="UTF-8"?><w:document xmlns:w="http://schemas.openxmlformats.org/'
           f'wordprocessingml/2006/main"><w:body>{cuerpo}</w:body></w:document>')
    with zipfile.ZipFile(ruta, 'w') as z:
        z.writestr('word/document.xml', xml)


class Temario:
    def __init__(self):
        self.tmp = tempfile.TemporaryDirectory()
        base = Path(self.tmp.name)
        self.temario = base / 'Asignaturas' / 'IA' / 'Temario'
        self.temario.mkdir(parents=True)
        (self.temario / '1_1 Aprendizaje.txt').write_text(TEMA_1.replace('\n', '\r\n'), encoding='utf-8')
        (self.temario / '2_1. Evaluacion.md').write_text(TEMA_2, encoding='utf-8')
        crear_docx(self.temario / '3 Optimizacion.docx',
                   ['La función de pérdida es una medida que cuantifica la diferencia entre la predicción '
                    'del modelo y el valor real observado en los datos.',
                    'El aprendizaje por refuerzo es un paradigma en el que un agente aprende mediante '
                    'recompensas obtenidas al interactuar con su entorno.'])
        self.datos = base / 'datos'

    def cerrar(self):
        self.tmp.cleanup()


class IAFalsa:
    """Sustituye a Ollama: devuelve preguntas correctas basadas en el pasaje recibido."""

    def __init__(self, retardo=0.0):
        self.retardo = retardo
        self.llamadas = 0

    def __enter__(self):
        self.originales = (ia.modelos, ia.generar_lote, ia.revisar_lote, ia.responder_duda)
        ia.modelos = lambda timeout=2.0: ['falso:latest']
        ia.generar_lote = self.generar_lote
        ia.revisar_lote = lambda modelo, pasaje, asignatura, preguntas, cancelar=None, fondo=False: (preguntas, 0)
        ia.responder_duda = self.responder_duda
        return self

    def __exit__(self, *_):
        ia.modelos, ia.generar_lote, ia.revisar_lote, ia.responder_duda = self.originales

    def generar_lote(self, modelo, pasaje, asignatura, dificultad, n=3, evitar=(), al_recibir=None, cancelar=None,
                     formatos=None, fondo=False, al_pregunta=None, verificar_falsas=True):
        self.formatos = list(formatos or [])
        self.llamadas += 1
        time.sleep(self.retardo)
        if al_recibir:
            al_recibir('{"preguntas": [')
        resultado = []
        for frase in frases(pasaje):
            if len(resultado) == n:
                break
            enunciado = f'¿Qué afirma el temario sobre «{frase[:40]}…» (variante {self.llamadas})?'
            if any(clave(enunciado) == clave(e) for e in evitar):
                continue
            resultado.append({'enunciado': enunciado,
                              'opciones': [frase[:60], 'Una opción falsa', 'Otra opción falsa', 'Nada de eso'],
                              'respuesta': 0, 'explicacion': 'Lo dice el temario.', 'cita': frase})
            if al_pregunta and len(resultado) == 1:
                al_pregunta(resultado[0])  # la primera llega antes que el resto del lote
        return resultado, 0

    def responder_duda(self, modelo, asignatura, duda, pasajes, contexto='', al_recibir=None, cancelar=None,
                       historial=(), trozo=False):
        self.historial = list(historial)
        if al_recibir:
            al_recibir('Te lo **explico**')
        return 'Te lo **explico** con el temario [1].'


# ======================================================================= texto

class PruebasTexto(unittest.TestCase):
    def test_limpiar_une_lineas_y_separa_vinetas(self):
        crudo = 'Título\r\n• El modelo aprende de los\r\ndatos de entrenamiento.\r\n• Otra idea dis-\r\ntinta aquí.\r\n12\r\n'
        limpio = limpiar(crudo)
        self.assertIn('El modelo aprende de los datos de entrenamiento.', limpio)
        self.assertIn('Otra idea distinta aquí.', limpio)
        self.assertNotIn('12', limpio)

    def test_frases_descarta_formulas_y_etiquetas(self):
        texto = ('Y = f(X) + error donde el error es aleatorio y no depende de nada más que del ruido\n'
                 'Error Test Error Entrenamiento Curva Sobreajuste Infra Ajuste Complejidad Modelo Óptimo\n'
                 'La regularización es una técnica que penaliza la complejidad del modelo para generalizar.')
        resultado = frases(texto)
        self.assertEqual(len(resultado), 1)
        self.assertTrue(resultado[0].startswith('La regularización'))

    def test_extraer_formatos(self):
        t = Temario()
        try:
            for archivo in t.temario.iterdir():
                paginas = extraer(archivo)
                self.assertTrue(paginas and any(len(texto) > 50 for _, texto in paginas), archivo.name)
        finally:
            t.cerrar()

    def test_nombre_tema(self):
        self.assertEqual(nombre_tema('2_5. Aprendizaje supervisado.pdf'), 'Aprendizaje supervisado')
        self.assertEqual(nombre_tema('sub/7_19 Error de Modelo.pdf'), 'Error de Modelo')


# =================================================================== generador

class PruebasGenerador(unittest.TestCase):
    def setUp(self):
        self.fragmentos = [{'ruta': 'a.txt', 'ubicacion': 'texto', 'texto': limpiar(TEMA_1)},
                           {'ruta': 'b.md', 'ubicacion': 'texto', 'texto': limpiar(TEMA_2)}]

    def comprobar(self, p):
        self.assertEqual(len(p['opciones']), 2 if p['tipo'] == 'vf' else 4)
        self.assertEqual(len({clave(o) for o in p['opciones']}), len(p['opciones']), p['opciones'])
        self.assertTrue(0 <= p['respuesta'] < len(p['opciones']))
        self.assertIn(clave(p['cita']), clave(TEMA_1 + ' ' + TEMA_2).replace('  ', ' ') + ' ' +
                      clave(limpiar(TEMA_1) + ' ' + limpiar(TEMA_2)))

    def test_glosario_detecta_definiciones(self):
        g = Generador('IA', self.fragmentos, semilla=3)
        terminos = {clave(d['termino']) for d in g.definiciones}
        for esperado in ('aprendizaje supervisado', 'sobreajuste', 'curva roc', 'regularizacion'):
            self.assertIn(esperado, terminos)

    def test_preguntas_validas_y_sin_repetir(self):
        g = Generador('IA', self.fragmentos, semilla=7)
        preguntas = g.generar(8, 'Intermedia', 'variado')
        self.assertGreaterEqual(len(preguntas), 6)
        self.assertEqual(len({p['id'] for p in preguntas}), len(preguntas))
        for p in preguntas:
            self.comprobar(p)

    def test_definicion_tiene_respuesta_correcta(self):
        g = Generador('IA', self.fragmentos, semilla=1)
        for d in g.definiciones:
            p = g.definicion(d, 'Intermedia')
            if p:
                self.assertEqual(clave(p['opciones'][p['respuesta']]), clave(d['termino']))
                self.assertNotIn(clave(d['termino']), clave(p['enunciado']))

    def test_inversa_y_falsa(self):
        g = Generador('IA', self.fragmentos, semilla=4)
        inversas = [p for p in (g.inversa(d, 'Intermedia') for d in g.definiciones) if p]
        falsas = [p for p in (g.falsa(d, 'Avanzada') for d in g.definiciones) if p]
        self.assertTrue(inversas and falsas)
        for p in inversas + falsas:
            self.comprobar(p)
        for p in falsas:
            self.assertEqual(p['formato'], 'falsa')
            # La falsa no es ninguna frase del temario; las otras tres sí.
            self.assertNotIn(clave(p['opciones'][p['respuesta']]), clave(limpiar(TEMA_1) + ' ' + limpiar(TEMA_2)))

    def test_huecos_concuerdan_en_genero(self):
        g = Generador('IA', self.fragmentos, semilla=5)
        frase = 'Para reducir el error se aplica la regularización sobre los parámetros del modelo.'
        p = g.completar(frase, 'a.txt', 'texto', 'Intermedia')
        if p and 'la _____' in p['enunciado']:
            for o in p['opciones']:
                self.assertNotIn(o.lower(), ('sobreajuste', 'aprendizaje supervisado', 'descenso por gradiente'))

    def test_formatos_del_generador(self):
        g = Generador('IA', self.fragmentos, semilla=6)
        solo = g.generar(6, 'Avanzada', 'opcion', formatos=['falsa'])
        self.assertTrue(solo)
        self.assertEqual({p['formato'] for p in solo}, {'falsa'})

    def test_verdadero_falso(self):
        g = Generador('IA', self.fragmentos, semilla=2)
        preguntas = g.generar(6, 'Básica', 'vf')
        self.assertTrue(preguntas)
        for p in preguntas:
            self.assertEqual(p['tipo'], 'vf')
            self.assertEqual(p['opciones'], ['Verdadero', 'Falso'])
            # Si es verdadera, la afirmación es exactamente la frase citada.
            if p['respuesta'] == 0:
                self.assertIn(clave(p['cita']), clave(p['enunciado']))


# ========================================================================= IA

class PruebasValidacionIA(unittest.TestCase):
    PASAJE = limpiar(TEMA_1)

    def pregunta(self, **cambios):
        base = {'enunciado': '¿Qué caracteriza al sobreajuste en un modelo de aprendizaje?',
                'opciones': ['Memoriza los datos y generaliza peor', 'Generaliza mejor que nunca',
                             'Necesita menos datos', 'Elimina el ruido'],
                'correcta': 0, 'explicacion': 'El modelo memoriza los datos de entrenamiento y pierde generalización.'}
        base.update(cambios)
        return base

    def test_valida_y_localiza_cita(self):
        p = ia.validar(self.pregunta(), self.PASAJE)
        self.assertIn('sobreajuste', p['cita'])

    def test_rechaza_opciones_repetidas(self):
        with self.assertRaises(ValueError):
            ia.validar(self.pregunta(opciones=['A', 'a', 'B', 'C']), self.PASAJE)

    def test_rechaza_todas_las_anteriores(self):
        with self.assertRaises(ValueError):
            ia.validar(self.pregunta(opciones=['Uno', 'Dos', 'Tres', 'Todas las anteriores']), self.PASAJE)

    def test_rechaza_sin_respaldo(self):
        with self.assertRaises(ValueError):
            ia.validar(self.pregunta(enunciado='¿Cuál es la capital de Mongolia según la historia?',
                                     opciones=['Ulán Bator', 'Pekín', 'Tokio', 'Seúl'],
                                     explicacion='Ulán Bator es la capital.'), self.PASAJE)

    def test_quita_letras_de_las_opciones(self):
        p = ia.validar(self.pregunta(opciones=['A) Memoriza los datos y generaliza peor', 'B) Generaliza mejor',
                                               'C) Necesita menos datos', 'D) Elimina el ruido']), self.PASAJE)
        self.assertEqual(p['opciones'][0], 'Memoriza los datos y generaliza peor')

    def test_formato_se_deduce_de_la_pregunta(self):
        hueco = self.pregunta(formato='definicion',
                              enunciado='Si un modelo acierta todo en entrenamiento y falla con datos nuevos, sufre _____.',
                              opciones=['sobreajuste', 'regularización', 'validación', 'heurística'],
                              explicacion='El sobreajuste memoriza los datos de entrenamiento y pierde generalización.')
        p = ia.validar(hueco, self.PASAJE, 'aplicacion')
        self.assertEqual(p['formato'], 'completar')
        self.assertIn('_____', p['enunciado'])
        falsa = self.pregunta(formato='completar', enunciado='¿Cuál de las siguientes afirmaciones es INCORRECTA?')
        self.assertEqual(ia.validar(falsa, self.PASAJE, 'falsa')['formato'], 'falsa')

    def test_motivos_de_la_ia(self):
        motivos = ['Correcto: memoriza el ruido.', 'Al revés: generaliza peor.', 'No: necesita los mismos datos.',
                   'No: el ruido lo memoriza.']
        p = ia.validar(self.pregunta(por_que=motivos), self.PASAJE)
        self.assertEqual(p['motivos'], motivos)
        self.assertEqual(ia.validar(self.pregunta(por_que=['corto']), self.PASAJE)['motivos'], [])

    def test_rechaza_enunciado_sin_pregunta(self):
        caso = self.pregunta(formato='aplicacion', enunciado='Un modelo acierta en entrenamiento y falla con datos '
                                                             'nuevos porque ha memorizado los ejemplos.')
        with self.assertRaises(ValueError):
            ia.validar(caso, self.PASAJE, 'aplicacion')

    def test_rechaza_frase_copiada_del_apunte(self):
        copia = self.pregunta(enunciado='El sobreajuste es un fenómeno en el que el modelo memoriza los datos de '
                                        'entrenamiento y _____ capacidad de generalización sobre datos nuevos.',
                              opciones=['pierde', 'gana', 'mantiene', 'duplica'])
        with self.assertRaises(ValueError):
            ia.validar(copia, self.PASAJE, 'completar')

    def test_rechaza_correcta_delatada_por_su_longitud(self):
        larga = self.pregunta(opciones=['El modelo memoriza los datos de entrenamiento y por eso pierde capacidad de '
                                        'generalización con los datos nuevos', 'Mejora', 'Acelera', 'Simplifica'])
        with self.assertRaises(ValueError):
            ia.validar(larga, self.PASAJE)

    def test_cita_en_diapositivas_sin_frases(self):
        diapositiva = ('Modelo de producción antes de 2000\nf(K, L)\nModelo de producción después de 2000\n'
                       'f(K, L, D): capital, trabajo y datos\nEconomías de escala y de alcance')
        p = ia.validar({'enunciado': '¿Qué factor nuevo incorpora el modelo de producción a partir del año 2000?',
                        'opciones': ['Los datos', 'La tierra', 'La energía', 'El transporte'], 'correcta': 0,
                        'explicacion': 'Después de 2000 el modelo de producción es f(K, L, D): capital, trabajo y datos.'},
                       diapositiva, 'definicion')
        self.assertIn('datos', p['cita'])

    def test_lote_entrega_cada_pregunta_al_terminarla_y_revisa_las_falsas(self):
        buena = self.pregunta(formato='definicion')
        falsa = self.pregunta(formato='falsa', enunciado='¿Cuál de las siguientes afirmaciones es INCORRECTA?')
        texto = json.dumps({'preguntas': [buena, falsa]}, ensure_ascii=False)
        corte = texto.index('}, {') + 1  # justo después de cerrar la primera pregunta
        entregadas, momentos = [], []

        def chat_falso(modelo, mensajes, formato=None, opciones=None, al_recibir=None, **k):
            al_recibir(texto[:corte])
            momentos.append(len(entregadas))  # la primera ya ha llegado con el lote a medias
            al_recibir(texto)
            return texto

        originales = ia.chat, ia.revisar_lote
        revisadas = []
        ia.chat = chat_falso
        ia.revisar_lote = lambda modelo, pasaje, asignatura, preguntas, cancelar=None, fondo=False: (
            revisadas.extend(preguntas) or ([], len(preguntas)))
        try:
            validas, descartadas = ia.generar_lote('m', self.PASAJE, 'IA', 'Intermedia', n=2,
                                                   formatos=['definicion', 'falsa'], al_pregunta=entregadas.append)
        finally:
            ia.chat, ia.revisar_lote = originales
        self.assertEqual(momentos, [1])
        self.assertEqual([p['formato'] for p in revisadas], ['falsa'])  # la falsa pasa por la revisión
        self.assertEqual(len(validas), 1)
        self.assertEqual(descartadas, 1)
        self.assertEqual(entregadas, validas)

    def test_elegir_formatos_varia_y_respeta_los_permitidos(self):
        vistos = set()
        for ronda in range(4):
            lote = ia.elegir_formatos(3, 'Intermedia', None, ronda)
            self.assertEqual(len(set(lote)), 3)
            vistos.update(lote)
        self.assertGreaterEqual(len(vistos), 5)
        self.assertEqual(set(ia.elegir_formatos(3, 'Avanzada', ['falsa'], 0)), {'falsa'})

    def test_texto_plano_quita_markdown(self):
        md = 'El **sobreajuste** (*overfitting*) importa.\n\n*   **Idea:** simple.\n    * *Ej.:* recta.\n### Fin'
        self.assertEqual(ia.texto_plano(md),
                         'El sobreajuste (overfitting) importa.\n\n• Idea: simple.\n    ◦ Ej.: recta.\nFin')

    def test_texto_plano_quita_latex(self):
        md = 'Estado $\\rightarrow$ Acción, $\\frac{a}{b}$ y $\\alpha \\leq 1$'
        self.assertEqual(ia.texto_plano(md), 'Estado → Acción, (a)/(b) y α ≤ 1')

    def test_modelo_recomendado(self):
        self.assertEqual(ia.modelo_recomendado(['llama3.2:1b', 'gemma4:latest']), 'gemma4:latest')
        self.assertEqual(ia.modelo_recomendado(['x:1', 'y:2'], 'y:2'), 'y:2')
        self.assertEqual(ia.modelo_recomendado([]), '')


# ====================================================================== motor

class PruebasMotor(unittest.TestCase):
    def setUp(self):
        self.t = Temario()
        self.eventos = []
        self.motor = motor_mod.Motor(self.t.datos, self.eventos.append)

    def tearDown(self):
        self.motor.detener_generacion()
        self.motor.banco.cerrar()
        self.t.cerrar()

    def abrir(self):
        with IAFalsa():
            return self.motor.hola('IA', 'Inteligencia Artificial', str(self.t.temario))

    def test_hola_indexa_y_reindexa_solo_cambios(self):
        estado = self.abrir()
        self.assertEqual(len(estado['archivos']), 3)
        self.assertTrue(all(not a['error'] for a in estado['archivos']))
        self.assertTrue(estado['ia']['disponible'])
        leidos = len([e for e in self.eventos if e.get('evento') == 'leyendo'])
        self.assertEqual(leidos, 3)
        self.abrir()
        self.assertEqual(len([e for e in self.eventos if e.get('evento') == 'leyendo']), leidos)
        (self.t.temario / '1_1 Aprendizaje.txt').unlink()
        estado = self.abrir()
        self.assertEqual(len(estado['archivos']), 2)

    def test_test_rapido_completo(self):
        self.abrir()
        r = self.motor.empezar({'modo': 'rapido', 'n': 5, 'tipo': 'opcion'})
        token = r['sesion']['token']
        self.assertEqual(r['sesion']['n'], 5)
        aciertos = 0
        for i in range(5):
            p = self.motor.pregunta(token, i)
            self.assertFalse(p.get('preparando'))
            eleccion = 0 if i % 2 else 1
            res = self.motor.responder(token, i, eleccion)
            self.assertTrue(res['corregida'])
            self.assertIn('explicacion', res)
            aciertos += res['acierto']
            self.assertEqual(res['acierto'], res['correcta'] == eleccion)
        fin = self.motor.terminar(token)
        self.assertEqual(fin['resultado']['aciertos'], aciertos)
        self.assertAlmostEqual(fin['resultado']['nota'], aciertos * 2.0)
        self.assertEqual(fin['resumen']['tests'], 1)
        with self.assertRaises(motor_mod.ErrorProfesor):
            self.motor.responder(token, 0, 0)

    def test_no_se_puede_cambiar_respuesta_corregida(self):
        self.abrir()
        token = self.motor.empezar({'modo': 'rapido', 'n': 5})['sesion']['token']
        primera = self.motor.responder(token, 0, 0)
        segunda = self.motor.responder(token, 0, 1)
        self.assertEqual(segunda['eleccion'], primera['eleccion'])

    def test_modo_examen_y_penalizacion(self):
        self.abrir()
        token = self.motor.empezar({'modo': 'rapido', 'n': 5, 'tipo': 'opcion', 'correccion': 'final',
                                    'penalizar': True})['sesion']['token']
        correctas = []
        for i in range(5):
            r = self.motor.responder(token, i, 0)
            self.assertFalse(r['corregida'])          # no se revela antes de entregar
            self.assertNotIn('correcta', r)
            self.assertEqual(r['sesion']['estados'][i], 'respondida')
        # Se cambia de opinión en la primera: vale la última respuesta.
        self.motor.responder(token, 0, 1)
        fin = self.motor.terminar(token)['resultado']
        for i in range(5):
            p = self.motor.pregunta(token, i)
            self.assertTrue(p['corregida'])
            correctas.append(p['acierto'])
        self.assertEqual(self.motor.pregunta(token, 0)['eleccion'], 1)
        aciertos = sum(correctas)
        esperado = max(0.0, aciertos - (5 - aciertos) / 3) * 2
        self.assertAlmostEqual(fin['nota'], round(esperado, 2))

    def test_repaso_de_fallos(self):
        self.abrir()
        token = self.motor.empezar({'modo': 'rapido', 'n': 5, 'tipo': 'opcion'})['sesion']['token']
        falladas = set()
        for i in range(5):
            p = self.motor.pregunta(token, i)
            r = self.motor.responder(token, i, (p['opciones'].index(p['opciones'][0]) + 1) % 4)
            if not r['acierto']:
                falladas.add(r['id'])
        self.motor.terminar(token)
        self.assertEqual(self.motor.progreso()['resumen']['repasar'], len(falladas))
        if falladas:
            r = self.motor.empezar({'modo': 'rapido', 'n': 10, 'repaso': True})
            ids = {self.motor.pregunta(r['sesion']['token'], i)['id'] for i in range(r['sesion']['n'])}
            self.assertEqual(ids, falladas)

    def test_repaso_sin_fallos_avisa(self):
        self.abrir()
        with self.assertRaises(motor_mod.ErrorProfesor):
            self.motor.empezar({'modo': 'rapido', 'n': 5, 'repaso': True})

    def test_filtrar_por_temas(self):
        estado = self.abrir()
        ruta = next(a['ruta'] for a in estado['archivos'] if a['ruta'].startswith('2_1'))
        token = self.motor.empezar({'modo': 'rapido', 'n': 5, 'temas': [ruta]})['sesion']['token']
        for i in range(self.motor.pregunta(token, 0)['n']):
            self.assertEqual(self.motor.pregunta(token, i)['ruta'], ruta)

    def test_modo_ia_rellena_en_segundo_plano(self):
        with IAFalsa(retardo=0.05):
            self.motor.hola('IA', 'Inteligencia Artificial', str(self.t.temario))
            r = self.motor.empezar({'modo': 'ia', 'n': 5})
            token = r['sesion']['token']
            self.assertTrue(r['pregunta'].get('preparando'))
            fin = time.time() + 10
            while time.time() < fin and 'preparando' in self.motor._resumen_sesion(self.motor.banco.sesion(token))['estados']:
                time.sleep(0.05)
        estados = self.motor._resumen_sesion(self.motor.banco.sesion(token))['estados']
        self.assertNotIn('preparando', estados)
        self.assertTrue(any(e.get('evento') == 'pregunta_lista' for e in self.eventos))
        p = self.motor.pregunta(token, 0)
        self.assertEqual(p['origen'], 'ia')
        self.assertEqual(len(p['opciones']), 4)

    def test_modo_mixto_empieza_al_instante_y_sustituye_no_vistas(self):
        with IAFalsa(retardo=0.2):
            self.motor.hola('IA', 'Inteligencia Artificial', str(self.t.temario))
            r = self.motor.empezar({'modo': 'mixto', 'n': 5, 'tipo': 'opcion'})
            token = r['sesion']['token']
            primera = r['pregunta']
            self.assertFalse(primera.get('preparando'))
            fin = time.time() + 10
            while time.time() < fin and not any(e.get('evento') == 'generacion' and e.get('estado') == 'terminada'
                                                for e in self.eventos):
                time.sleep(0.05)
        # La pregunta ya mostrada no se ha cambiado; las demás son de la IA.
        self.assertEqual(self.motor.pregunta(token, 0)['id'], primera['id'])
        origenes = [self.motor.pregunta(token, i)['origen'] for i in range(1, 5)]
        self.assertEqual(origenes, ['ia'] * 4)

    def test_ia_recibe_formatos_variados_y_los_permitidos(self):
        with IAFalsa() as falsa:
            self.motor.hola('IA', 'Inteligencia Artificial', str(self.t.temario))
            r = self.motor.empezar({'modo': 'ia', 'n': 5, 'formatos': ['aplicacion', 'relacion', 'inventado']})
            self.assertEqual(r['sesion']['config']['formatos'], ['aplicacion', 'relacion'])
            fin = time.time() + 10
            while time.time() < fin and not any(e.get('evento') == 'generacion' and e.get('estado') == 'terminada'
                                                for e in self.eventos):
                time.sleep(0.05)
            self.assertTrue(set(falsa.formatos) <= {'aplicacion', 'relacion'}, falsa.formatos)
        token = r['sesion']['token']
        p = self.motor.pregunta(token, 0)
        self.assertIn('formato_nombre', p)
        self.motor.responder(token, 0, 0)
        resultado = self.motor.terminar(token)['resultado']
        self.assertTrue(resultado['por_formato'])
        self.assertEqual(sum(f['total'] for f in resultado['por_formato']), 5)

    def test_test_global_reparte_los_temas(self):
        self.abrir()
        self.motor.rutas_sesion = None
        rutas = sorted({f['ruta'] for f in self.motor.fragmentos})
        vistas = [self.motor._elegir_pasaje(rutas)[0] for _ in range(len(rutas))]
        self.assertEqual(sorted(vistas), rutas)  # una vuelta completa antes de repetir tema
        ruta, ventana, mezclado = self.motor._elegir_pasaje(rutas, mezclar=True)
        self.assertTrue(mezclado)
        self.assertEqual(len({f['ruta'] for f in ventana}), 2)

    def test_acertar_dudando_cuenta_pero_vuelve_a_repaso(self):
        self.abrir()
        token = self.motor.empezar({'modo': 'rapido', 'n': 5, 'tipo': 'opcion'})['sesion']['token']
        p = self.motor.pregunta(token, 0)
        r = self.motor.responder(token, 0, self.correcta(token, 0), dudosa=True)
        self.assertTrue(r['acierto'] and r['dudosa'])
        self.assertIn(p['id'], {q['id'] for q in self.motor.banco.para_repasar('IA')})
        resultado = self.motor.terminar(token)['resultado']
        self.assertEqual(resultado['dudosas'], 1)
        self.assertTrue(resultado['preguntas'][0]['dudosa'])
        # «Repetir las falladas» también la incluye, junto a las que quedaron en blanco
        repetido = self.motor.empezar({'repetir': token})
        self.assertEqual(repetido['sesion']['n'], 5)

    def test_dudosa_en_modo_examen(self):
        self.abrir()
        token = self.motor.empezar({'modo': 'rapido', 'n': 5, 'correccion': 'final'})['sesion']['token']
        self.motor.responder(token, 0, self.correcta(token, 0), dudosa=True)
        self.assertTrue(self.motor.pregunta(token, 0)['dudosa'])
        self.assertEqual(self.motor.terminar(token)['resultado']['dudosas'], 1)

    def test_motivos_de_cada_opcion_al_corregir(self):
        self.abrir()
        token = self.motor.empezar({'modo': 'rapido', 'n': 10, 'tipo': 'opcion',
                                    'formatos': ['definicion']})['sesion']['token']
        con_motivos = 0
        for i in range(self.motor.pregunta(token, 0)['n']):
            r = self.motor.responder(token, i, 0)
            if r.get('motivos'):
                con_motivos += 1
                self.assertEqual(len(r['motivos']), len(r['opciones']))
                self.assertTrue(r['motivos'][r['correcta']].startswith('Correcta'))
        self.assertGreater(con_motivos, 0)

    def correcta(self, token, indice):
        """Índice (tal como se muestra) de la opción buena, mirando el banco."""
        s = self.motor.banco.sesion(token)
        e = s['preguntas'][indice]
        p = self.motor.banco.pregunta(e['id'])
        orden = e['orden'] or list(range(len(p['opciones'])))
        return orden.index(p['respuesta'])

    def test_reserva_prepara_preguntas_de_cada_tema(self):
        with IAFalsa() as falsa:
            original = falsa.generar_lote

            def con_formato(*a, **k):
                self.assertTrue(k.get('fondo'))  # la reserva nunca va en primer plano
                lote, d = original(*a, **k)
                return [{**q, 'formato': 'aplicacion'} for q in lote], d

            ia.generar_lote = con_formato
            self.motor.hola('IA', 'Inteligencia Artificial', str(self.t.temario))
            self.motor._reservar('IA', threading.Event())  # sin hilo: termina al llenar la reserva
        nuevas = self.motor.nuevas_ia()
        rutas = {f['ruta'] for f in self.motor.fragmentos}
        self.assertEqual(set(nuevas), rutas)
        self.assertTrue(all(n >= self.motor.RESERVA for n in nuevas.values()))
        self.assertTrue(any(e.get('evento') == 'reserva' and e['estado'] == 'completa' for e in self.eventos))
        # El siguiente test de IA empieza con ellas, sin esperar a la IA.
        r = self.motor.empezar({'modo': 'mixto', 'n': 5})
        self.assertEqual(r['pregunta']['origen'], 'ia')
        self.motor.detener_generacion()

    def test_llamadas_de_fondo_ceden_el_paso(self):
        ia._en_primer_plano += 1
        try:
            self.assertFalse(ia.libre())
            with self.assertRaises(ia.Cancelado):
                ia.chat('modelo', [], fondo=True)
        finally:
            ia._en_primer_plano -= 1
        self.assertTrue(ia.libre())

    def test_rellenar_huecos_si_la_ia_no_llega(self):
        with IAFalsa(retardo=5):
            self.motor.hola('IA', 'Inteligencia Artificial', str(self.t.temario))
            token = self.motor.empezar({'modo': 'ia', 'n': 5})['sesion']['token']
            r = self.motor.rellenar(token)
        self.assertNotIn('preparando', r['sesion']['estados'])

    def test_continuar_test_pendiente(self):
        self.abrir()
        token = self.motor.empezar({'modo': 'rapido', 'n': 5})['sesion']['token']
        self.motor.responder(token, 0, 0)
        self.motor.responder(token, 1, 0)
        estado = self.abrir()
        self.assertEqual(estado['pendiente']['token'], token)
        r = self.motor.continuar()
        self.assertEqual(r['pregunta']['indice'], 2)

    def test_descartar_pregunta(self):
        self.abrir()
        token = self.motor.empezar({'modo': 'rapido', 'n': 5})['sesion']['token']
        ident = self.motor.pregunta(token, 0)['id']
        self.motor.descartar(token, 0)
        ids = {p['id'] for p in self.motor.banco.preguntas('IA')}
        self.assertNotIn(ident, ids)

    def test_duda_sin_ia_avisa_en_vez_de_responder(self):
        # Sin IA no se devuelve una respuesta genérica (trozos del temario): se avisa de que hace falta
        self.abrir()
        self.motor.ia_estado['disponible'] = False
        original = ia.modelos
        ia.modelos = lambda timeout=2.0: None
        try:
            with self.assertRaises(motor_mod.ErrorProfesor) as e:
                self.motor.duda('¿Qué es el sobreajuste?')
        finally:
            ia.modelos = original
        self.assertIn('IA', str(e.exception))
        self.assertEqual(self.motor.conversacion()['chat'], [])  # y no se guarda en la conversación

    def test_duda_con_ia(self):
        with IAFalsa() as falsa:
            self.motor.hola('IA', 'Inteligencia Artificial', str(self.t.temario))
            r = self.motor.duda('¿Qué es la curva ROC?')
            self.motor.duda('¿Y un ejemplo?', historial=[{'pregunta': '¿Qué es la curva ROC?',
                                                          'respuesta': r['respuesta']}])
        self.assertIn('[1]', r['respuesta'])
        self.assertIn('**explico**', r['respuesta'])  # la negrita llega para que la ventana la dibuje
        # La conversación se guarda y vuelve al abrir el profesor.
        self.assertEqual([t['pregunta'] for t in self.motor.conversacion()['chat']],
                         ['¿Qué es la curva ROC?', '¿Y un ejemplo?'])
        self.assertEqual(self.motor.nueva_conversacion()['chat'], [])
        self.assertEqual(self.motor.conversacion()['chat'], [])
        self.assertIn('Evaluación', r['fuentes'][0]['titulo'])
        self.assertEqual(falsa.historial[0][0], '¿Qué es la curva ROC?')

    # ------------------------------------------------------------ mis fallos

    def responder_todo(self, token, acertar):
        """Responde todas las preguntas, bien o mal a propósito. Devuelve los ids."""
        s = self.motor.banco.sesion(token)
        ids = []
        for i, e in enumerate(s['preguntas']):
            p = self.motor.banco.pregunta(e['id'])
            buena = e['orden'].index(p['respuesta'])
            self.motor.responder(token, i, buena if acertar else (buena + 1) % len(e['orden']))
            ids.append(e['id'])
        return ids

    def test_fallos_se_guardan_solos_y_se_repasan(self):
        self.abrir()
        token = self.motor.empezar({'modo': 'rapido', 'n': 5})['sesion']['token']
        falladas = set(self.responder_todo(token, acertar=False))
        fin = self.motor.terminar(token)
        self.assertEqual(fin['resumen']['mis_fallos'], len(falladas))
        lista = self.motor.fallos()['fallos']
        self.assertEqual({f['id'] for f in lista}, falladas)
        self.assertTrue(all(f['tema'] and f['respuesta'] for f in lista))
        # Test solo con mis fallos (todas: n=0); acertarlas y quitarlas.
        r = self.motor.empezar({'fallos': True, 'n': 0, 'modo': 'ia'})
        self.assertEqual(r['sesion']['config']['modo'], 'rapido')
        token2 = r['sesion']['token']
        self.assertTrue(r['pregunta']['de_fallos'])
        self.assertTrue(r['pregunta']['guardada'])
        self.assertEqual(set(self.responder_todo(token2, acertar=True)), falladas)
        fin = self.motor.terminar(token2)
        self.assertEqual(fin['resultado']['acertadas_guardadas'], len(falladas))
        self.assertTrue(fin['resultado']['de_fallos'])
        quitado = self.motor.quitar_fallo(token=token2, acertadas=True)
        self.assertEqual(quitado['quitadas'], len(falladas))
        self.assertEqual(quitado['fallos'], [])
        with self.assertRaises(motor_mod.ErrorProfesor):
            self.motor.empezar({'fallos': True, 'n': 0})

    def test_guardar_a_mano_y_sin_autoguardado(self):
        self.abrir()
        token = self.motor.empezar({'modo': 'rapido', 'n': 5, 'auto_guardar': False})['sesion']['token']
        self.responder_todo(token, acertar=False)
        self.assertEqual(self.motor.fallos()['fallos'], [])
        r = self.motor.guardar(token, 2, True)
        self.assertTrue(r['pregunta']['guardada'])
        self.assertEqual(len(self.motor.fallos()['fallos']), 1)
        self.motor.quitar_fallo(r['pregunta']['id'])
        self.assertEqual(self.motor.fallos()['fallos'], [])
        self.assertFalse(self.motor.guardar(token, 2, False)['pregunta']['guardada'])

    def test_temario_vacio(self):
        for f in self.t.temario.iterdir():
            f.unlink()
        self.abrir()
        with self.assertRaises(motor_mod.ErrorProfesor):
            self.motor.empezar({'modo': 'rapido'})

    def test_tarjetas_repeticion_espaciada(self):
        self.abrir()
        self.motor._imagen = lambda t: {'archivo': 'figura.jpg', 'pie': t['ubicacion']}
        r = self.motor.tarjetas(n=8)
        self.assertGreater(r['mazo']['total'], 0)
        self.assertEqual(r['mazo']['nuevas'], r['mazo']['total'])
        tarjetas = r['tarjetas']
        self.assertTrue(0 < len(tarjetas) <= 8)
        for t in tarjetas:
            self.assertTrue(t['frente'] and t['reverso'] and t['tema'] and t['nueva'])
        # La primera trae su figura; las demás llegan después como eventos.
        self.assertTrue(tarjetas[0]['imagen'])
        esperadas = {t['id'] for t in tarjetas[1:]}
        for _ in range(100):
            if {e['tarjeta'] for e in self.eventos if e.get('evento') == 'imagen_tarjeta'} >= esperadas:
                break
            time.sleep(0.05)
        self.assertEqual({e['tarjeta'] for e in self.eventos if e.get('evento') == 'imagen_tarjeta'}, esperadas)
        self.assertIn('concepto', {t['tipo'] for t in self.motor.tarjetas(n=500)['tarjetas']})
        # «Otra vez» la deja pendiente; «Bien» la manda a otro día.
        mal, bien = tarjetas[0]['id'], tarjetas[1]['id']
        self.assertEqual(self.motor.tarjeta(mal, 0)['caja'], 0)
        fin = self.motor.tarjeta(bien, 2)
        self.assertEqual(fin['caja'], 2)
        self.assertEqual(fin['resumen']['tarjetas_pendientes'], 1)
        self.assertEqual(fin['resumen']['hoy'], 2)  # los repasos cuentan para la racha
        siguiente = self.motor.tarjetas(n=500)
        ids = [t['id'] for t in siguiente['tarjetas']]
        self.assertIn(mal, ids)
        self.assertNotIn(bien, ids)
        self.assertEqual(siguiente['mazo']['pendientes'], 1)
        # Con todo repasado, «extra» ofrece las menos sabidas igualmente.
        for i in ids:
            self.motor.tarjeta(i, 1)
        self.assertEqual(self.motor.tarjetas()['tarjetas'], [])
        self.assertTrue(self.motor.tarjetas(n=5, extra=True)['tarjetas'])

    def test_repetir_falladas_de_un_test(self):
        self.abrir()
        token = self.motor.empezar({'modo': 'rapido', 'n': 5, 'tipo': 'opcion'})['sesion']['token']
        for i in range(4):  # la última queda en blanco
            p = self.motor.pregunta(token, i)
            correcta = self.motor.banco.pregunta(p['id'])['respuesta']
            orden = self.motor._sesion(token)['preguntas'][i]['orden']
            eleccion = orden.index(correcta) if i < 2 else (orden.index(correcta) + 1) % len(orden)
            self.motor.responder(token, i, eleccion)
        self.motor.terminar(token)
        r = self.motor.empezar({'repetir': token, 'modo': 'ia'})
        self.assertEqual(r['sesion']['n'], 3)
        self.assertEqual(r['sesion']['config']['modo'], 'rapido')
        token2 = r['sesion']['token']
        self.responder_todo(token2, acertar=True)
        self.assertTrue(self.motor.terminar(token2)['resultado']['repeticion'])
        with self.assertRaises(motor_mod.ErrorProfesor):
            self.motor.empezar({'repetir': token2})

    def test_resumen_para_la_ventana_principal(self):
        self.abrir()
        token = self.motor.empezar({'modo': 'rapido', 'n': 5})['sesion']['token']
        self.responder_todo(token, acertar=False)
        self.motor.terminar(token)
        datos = json.loads((self.t.datos / 'resumen.json').read_text(encoding='utf-8'))
        self.assertEqual(datos['IA']['tests'], 1)
        self.assertEqual(datos['IA']['precision'], 0)
        self.assertEqual(datos['IA']['mis_fallos'], 5)
        self.assertEqual(datos['IA']['racha'], 1)
        self.assertEqual(datos['IA']['ultima_nota'], 0)

    def test_racha(self):
        from datetime import date
        hoy = date(2026, 10, 11)
        self.assertEqual(motor_mod.racha({}, hoy), 0)
        self.assertEqual(motor_mod.racha({'2026-10-11': 3, '2026-10-10': 1, '2026-10-08': 2}, hoy), 2)
        # Si hoy aún no has estudiado, la racha de ayer sigue viva.
        self.assertEqual(motor_mod.racha({'2026-10-10': 1, '2026-10-09': 4}, hoy), 2)
        self.assertEqual(motor_mod.racha({'2026-10-08': 1}, hoy), 0)

    def test_progreso(self):
        self.abrir()
        for _ in range(2):
            token = self.motor.empezar({'modo': 'rapido', 'n': 5})['sesion']['token']
            for i in range(5):
                self.motor.responder(token, i, 0)
            self.motor.terminar(token)
        p = self.motor.progreso()
        self.assertEqual(len(p['actividad']), 56)
        self.assertEqual(sum(d['n'] for d in p['actividad']), 10)
        self.assertEqual(p['resumen']['racha'], 1)
        self.assertEqual(p['resumen']['hoy'], 10)
        self.assertEqual(len(p['historial']), 2)
        self.assertEqual(p['resumen']['respuestas'], 10)
        self.assertTrue(p['temas'])


# ==================================================================== imágenes

class PruebasImagenes(unittest.TestCase):
    def test_pdf_docx_y_texto(self):
        from io import BytesIO
        from PIL import Image, ImageDraw
        from imagenes import Imagenes
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            # PDF de 3 páginas; la figura de la 2.ª es la más cercana a la 3.ª.
            paginas = []
            for i in range(3):
                hoja = Image.new('RGB', (800, 600), 'white')
                if i == 1:
                    ImageDraw.Draw(hoja).ellipse((200, 150, 600, 450), fill='orange')
                paginas.append(hoja)
            pdf = tmp / 'tema.pdf'
            paginas[0].save(pdf, save_all=True, append_images=paginas[1:])
            img = Imagenes(tmp / 'cache')
            r = img.para(pdf, 'página 3')
            self.assertTrue(Path(r['archivo']).is_file())
            self.assertIn('página', r['pie'])
            self.assertIsNone(img.para(tmp / 'no_existe.pdf', 'página 1'))
            # Páginas enteras para el lector («ver como el PDF»)
            self.assertTrue(img.visual(pdf))
            self.assertFalse(img.visual(tmp / 'apuntes.docx'))
            hojas = img.hojas(pdf)
            self.assertEqual(len(hojas), 3)
            self.assertAlmostEqual(hojas[0][1] / hojas[0][0], 0.75, places=2)
            jpg = Path(img.pagina(pdf, 1, 1000))
            self.assertTrue(jpg.is_file())
            with Image.open(jpg) as hoja:
                self.assertEqual(hoja.size[0], 1000)
                self.assertGreater(hoja.getpixel((500, 375))[1], 100)  # el círculo naranja de la página 2
                self.assertLess(hoja.getpixel((500, 375))[2], 100)
            self.assertEqual(str(jpg), img.pagina(pdf, 1, 1000))  # de la caché
            with self.assertRaises(ValueError):
                img.pagina(pdf, 7, 1000)
            # DOCX con una imagen en word/media
            docx = tmp / 'apuntes.docx'
            crear_docx(docx, ['Texto de prueba suficientemente largo para el documento.'])
            buf = BytesIO()
            Image.effect_noise((300, 300), 60).convert('RGB').save(buf, 'PNG')
            with zipfile.ZipFile(docx, 'a') as z:
                z.writestr('word/media/image1.png', buf.getvalue())
            self.assertTrue(Path(img.para(docx, 'documento')['archivo']).is_file())
            txt = tmp / 'notas.txt'
            txt.write_text('hola', encoding='utf-8')
            self.assertIsNone(img.para(txt, 'texto'))

    def test_pregunta_trae_campo_imagen(self):
        t = Temario()
        try:
            m = motor_mod.Motor(t.datos)
            with IAFalsa():
                m.hola('IA', 'IA', str(t.temario))
            token = m.empezar({'modo': 'rapido', 'n': 5})['sesion']['token']
            # Temario sin figuras: None (la ventana dibuja entonces una portada del tema).
            self.assertIn('imagen', m.pregunta(token, 0))
            m.banco.cerrar()
        finally:
            t.cerrar()


# =================================================================== servidor

class PruebasServidor(unittest.TestCase):
    def test_protocolo(self):
        t = Temario()
        proceso = subprocess.Popen([sys.executable, str(RAIZ / 'servidor.py'), str(t.datos)],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   encoding='utf-8', env={**__import__('os').environ, 'UNIVERSALIS_OLLAMA': 'http://127.0.0.1:9'})
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
            self.assertTrue(r['ok'])
            self.assertFalse(r['ia']['disponible'])
            r = pedir(id=2, cmd='empezar', config={'modo': 'mixto', 'n': 5})
            self.assertTrue(r['ok'], r)
            self.assertEqual(r['sesion']['config']['modo'], 'rapido')  # sin IA, cae a rápido
            token = r['sesion']['token']
            r = pedir(id=3, cmd='responder', token=token, indice=0, eleccion=0)
            self.assertTrue(r['pregunta']['corregida'])
            # Todas las órdenes que usa la ventana responden con ok=True.
            for i, orden in enumerate([{'cmd': 'pregunta', 'token': token, 'indice': 1},
                                       {'cmd': 'descartar', 'token': token, 'indice': 1},
                                       {'cmd': 'precalentar'}, {'cmd': 'ia'}, {'cmd': 'progreso'},
                                       {'cmd': 'continuar'}, {'cmd': 'detener'}, {'cmd': 'reindexar'},
                                       {'cmd': 'guardar', 'token': token, 'indice': 1, 'guardar': True},
                                       {'cmd': 'fallos'}, {'cmd': 'quitar_fallo', 'token': token, 'acertadas': True},
                                       {'cmd': 'tarjetas', 'n': 5}, {'cmd': 'tarjetas', 'extra': True},
                                       {'cmd': 'terminar', 'token': token}]):
                r = pedir(id=100 + i, **orden)
                self.assertTrue(r['ok'], (orden, r))
            # Sin IA, una duda no se responde con trozos del temario: se avisa de que hace falta la IA.
            r = pedir(id=5, cmd='duda', texto='¿Qué es el sobreajuste?', historial=[])
            self.assertFalse(r['ok'])
            self.assertIn('IA', r['mensaje'])
            r = pedir(id=4, cmd='inventada')
            self.assertFalse(r['ok'])
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
