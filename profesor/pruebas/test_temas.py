"""Pruebas del agrupado de apartados en temas (temas.py).

Uso:  python profesor/pruebas/test_temas.py
"""
import os
import json
import sys
import unittest
from pathlib import Path

os.environ["UNIVERSALIS_SIN_RESERVA"] = "1"  # sin trabajo de IA en segundo plano
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import ia  # noqa: E402
import temas  # noqa: E402
from temas import agrupar, clave_natural, numero_tema, propuesta_ia  # noqa: E402

IA_RUTAS = ['1_1 Concepto IA.pdf', '1_2 IA como tendencia.pdf', '10_30 Redes.pdf', '2_4. Definición del AA.pdf',
            '6_16 Funciones de Perdida y Metricas_Concepto.pdf', '6_17 Funciones de Perdida y Metricas_Regresión.pdf',
            'Tema3/Apuntes.pdf', 'Prácticas/P1.pdf', 'Prácticas/P2.pdf', 'Ej1.Enero2013.pdf', 'Ej1.Julio2014.pdf',
            'Guía docente.pdf']


class Numeros(unittest.TestCase):
    def test_numero_tema(self):
        casos = {'1_1 Concepto IA': 1, '2_4. Definición': 2, 'Tema1.1_ESQUEMA(1)': 1, '1.1 Proceso': 1,
                 'Tema 12 - Grafos': 12, 'T3 Árboles': 3, 'Unidad 4': 4, 'Ej1.Enero2013': None,
                 'Gestión de memoria': None, '2023 examen': None, 'L1.resumen_GDB': None}
        for nombre, esperado in casos.items():
            self.assertEqual(numero_tema(nombre), esperado, nombre)

    def test_orden_natural(self):
        self.assertEqual(sorted(['10_1 a', '2_4 b', '7_20 c', '7_19 d'], key=clave_natural),
                         ['2_4 b', '7_19 d', '7_20 c', '10_1 a'])


class Agrupar(unittest.TestCase):
    def test_sin_ia(self):
        g = agrupar(IA_RUTAS)
        self.assertEqual(g['1_1 Concepto IA.pdf']['grupo'], 'Tema 1')
        self.assertEqual(g['1_2 IA como tendencia.pdf']['grupo_orden'], g['1_1 Concepto IA.pdf']['grupo_orden'])
        self.assertEqual(g['Tema3/Apuntes.pdf']['grupo'], 'Tema 3 · Apuntes')  # número de la carpeta
        self.assertEqual(g['6_16 Funciones de Perdida y Metricas_Concepto.pdf']['grupo'],
                         'Tema 6 · Funciones de Pérdida y Métricas')  # con las tildes puestas
        self.assertEqual(g['Prácticas/P1.pdf']['grupo'], 'Prácticas')
        self.assertEqual(g['Ej1.Enero2013.pdf']['grupo_orden'], g['Ej1.Julio2014.pdf']['grupo_orden'])
        self.assertEqual(g['Guía docente.pdf']['grupo'], temas.SIN_TEMA)
        # Orden: los temas numerados (1, 2, 3, 6, 10), luego carpetas y prefijos, y lo suelto al final
        orden = [r for r, _ in sorted(g.items(), key=lambda x: x[1]['orden'])]
        self.assertEqual(orden[:3], ['1_1 Concepto IA.pdf', '1_2 IA como tendencia.pdf', '2_4. Definición del AA.pdf'])
        self.assertEqual(orden[5], '6_17 Funciones de Perdida y Metricas_Regresión.pdf')
        self.assertEqual(orden[6], '10_30 Redes.pdf')
        self.assertEqual(orden[-1], 'Guía docente.pdf')
        self.assertEqual(sorted(v['orden'] for v in g.values()), list(range(1, len(IA_RUTAS) + 1)))

    def test_propuesta_de_la_ia(self):
        respuesta = {'temas': [
            {'titulo': 'Tema 1: introducción a la IA', 'apartados': [1, 2, 12]},  # la guía docente va al tema 1
            {'titulo': 'Exámenes de otros años', 'apartados': [10, 11]},
            {'titulo': 'Aprendizaje automático', 'apartados': [4, 99]},           # índice inválido: se ignora
            {'titulo': '', 'apartados': [3]}]}                                    # sin título: se ignora
        original = ia.chat
        ia.chat = lambda *a, **k: json.dumps(respuesta)
        try:
            p = propuesta_ia('falso', 'IA', IA_RUTAS, {})
        finally:
            ia.chat = original
        self.assertEqual(p['titulos'], {'1': 'Introducción a la IA', '2': 'Aprendizaje automático'})
        g = agrupar(IA_RUTAS, json.loads(json.dumps(p)))  # como si viniera de la base de datos
        self.assertEqual(g['1_1 Concepto IA.pdf']['grupo'], 'Tema 1 · Introducción a la IA')
        self.assertEqual(g['Guía docente.pdf']['grupo'], 'Tema 1 · Introducción a la IA')
        self.assertEqual(g['Ej1.Enero2013.pdf']['grupo'], 'Exámenes de otros años')
        self.assertEqual(g['2_4. Definición del AA.pdf']['grupo'], 'Tema 2 · Aprendizaje automático')
        self.assertEqual(g['10_30 Redes.pdf']['grupo'], 'Tema 10 · Redes')

    def test_sin_numeros_la_ia_numera(self):
        rutas = ['Gestión de memoria.pdf', 'Memoria virtual.pdf', 'Consola de Linux.pdf']
        p = {'titulos': {}, 'sueltos': {'Gestión de memoria.pdf': 'g0', 'Memoria virtual.pdf': 'g0',
                                         'Consola de Linux.pdf': 'g1'},
             'grupos': [{'clave': 'g0', 'titulo': 'Gestión de memoria'}, {'clave': 'g1', 'titulo': 'Entorno Linux'}]}
        g = agrupar(rutas, p)
        self.assertEqual(g['Memoria virtual.pdf']['grupo'], 'Tema 1 · Gestión de memoria')
        self.assertEqual(g['Consola de Linux.pdf']['grupo'], 'Tema 2 · Entorno Linux')


class Textos(unittest.TestCase):
    def test_nombre_tema_con_tildes(self):
        from generador import nombre_tema
        self.assertEqual(nombre_tema('3_1. Validacion Cruzada.pdf'), 'Validación Cruzada')
        self.assertEqual(nombre_tema('Funciones de Perdida y Metricas.pdf'), 'Funciones de Pérdida y Métricas')
        self.assertEqual(nombre_tema('2_5 Regularizaciones.pdf'), 'Regularizaciones')
        self.assertEqual(nombre_tema('Modelos NO lineales.pdf'), 'Modelos NO lineales')
        self.assertEqual(nombre_tema('Clasificación.pdf'), 'Clasificación')

    def test_raiz(self):
        from texto import raiz
        self.assertEqual(raiz('Validaciones'), raiz('validación'))
        self.assertEqual(raiz('neuronales'), raiz('neuronal'))
        self.assertEqual(raiz('modelos'), raiz('modelo'))

    def test_extracto(self):
        # Lo relevante de un fragmento largo, en su orden y sin pasarse del límite
        from motor import _extracto
        from texto import es_prosa as _es_prosa
        from texto import raices
        relleno = 'Este párrafo habla de otra cosa distinta sin relación alguna con lo buscado. ' * 12
        texto = relleno + 'La validación cruzada divide los datos en k porciones. ' + relleno + 'Se repite k veces.'
        e = _extracto(texto, raices('validación cruzada'), limite=300)
        self.assertIn('validación cruzada', e)
        self.assertLessEqual(len(e), 500)
        self.assertEqual(_extracto('corto', ['x']), 'corto')
        self.assertFalse(_es_prosa('4 3,6 2 -0,40 -2 -1,8 -1 0,20 6 6,6'))
        self.assertTrue(_es_prosa('La validación cruzada divide los datos.'))

    def test_reintento_si_ollama_se_cae(self):
        # Ollama no contesta: se arranca y se reintenta una vez (solo en primer plano)
        import urllib.error

        class Respuesta:
            def __enter__(self):
                return iter([b'{"message": {"content": "ok"}, "done": true}'])

            def __exit__(self, *a):
                return False

        intentos, arranques = [], []

        def peticion(ruta, cuerpo=None, timeout=5):
            intentos.append(ruta)
            if len(intentos) == 1:
                raise urllib.error.URLError('conexión rechazada')
            return Respuesta()

        viejos = ia._peticion, ia.arrancar_ollama
        ia._peticion, ia.arrancar_ollama = peticion, lambda: arranques.append(1) or True
        try:
            self.assertEqual(ia.chat('m', []), 'ok')
            self.assertEqual((len(intentos), len(arranques)), (2, 1))
            intentos.clear()
            with self.assertRaises(ia.SinConexion):  # la reserva de fondo no reintenta
                ia.chat('m', [], fondo=True)
        finally:
            ia._peticion, ia.arrancar_ollama = viejos

    def test_opciones_fijas(self):
        # Mismo num_ctx y num_thread en todas las llamadas: si cambian, Ollama recarga el modelo.
        enviados = []

        class Respuesta:
            def __enter__(self):
                return iter([b'{"message": {"content": "ok"}, "done": true}'])

            def __exit__(self, *a):
                return False

        viejo = ia._peticion
        ia._peticion = lambda ruta, cuerpo=None, timeout=5: (enviados.append(cuerpo), Respuesta())[1]
        try:
            ia.chat('m', [], opciones={'num_ctx': 2048, 'num_predict': 10})
            ia.chat('m', [], opciones={'temperature': 0})
        finally:
            ia._peticion = viejo
        for c in enviados:
            self.assertEqual(c['options']['num_ctx'], ia.CONTEXTO)
            self.assertEqual(c['options']['num_thread'], ia.HILOS)


if __name__ == '__main__':
    unittest.main()
