"""Pruebas de la conexión con la IA (ia.py): ia.json, Ollama por defecto y servidores compatibles con
la API de OpenAI (LM Studio, llama.cpp, vLLM, OpenAI…). Se usa un servidor HTTP falso en este equipo.

Uso:  python profesor/pruebas/test_conexion_ia.py
"""
import json
import os
import sys
import tempfile
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

os.environ["UNIVERSALIS_SIN_RESERVA"] = "1"
for _v in ('UNIVERSALIS_IA_PROVEEDOR', 'UNIVERSALIS_IA_URL', 'UNIVERSALIS_IA_CLAVE', 'UNIVERSALIS_IA_MODELO',
           'UNIVERSALIS_IA_NOMBRE', 'UNIVERSALIS_OLLAMA'):
    os.environ.pop(_v, None)
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import ia  # noqa: E402


class Falso(BaseHTTPRequestHandler):
    """Imita /v1/models y /v1/chat/completions (en streaming, con eventos SSE)."""
    recibidos = []
    trozos = ['Hola', ', ', 'mundo']
    clave = 'secreta'

    def log_message(self, *a):
        pass

    def _json(self, codigo, datos):
        cuerpo = json.dumps(datos).encode('utf-8')
        self.send_response(codigo)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(cuerpo)))
        self.end_headers()
        self.wfile.write(cuerpo)

    def _autorizado(self):
        if self.headers.get('Authorization') != 'Bearer ' + Falso.clave:
            self._json(401, {'error': {'message': 'Invalid API key'}})
            return False
        return True

    def do_GET(self):
        if self.path != '/v1/models':
            return self._json(404, {'error': 'no existe'})
        if self._autorizado():
            self._json(200, {'data': [{'id': 'mi-modelo'}, {'id': 'text-embedding-3-small'}, {'id': 'otro'}]})

    def do_POST(self):
        if self.path != '/v1/chat/completions':
            return self._json(404, {'error': 'no existe'})
        if not self._autorizado():
            return
        cuerpo = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        Falso.recibidos.append(cuerpo)
        self.send_response(200)
        self.send_header('Content-Type', 'text/event-stream')
        self.end_headers()
        self.wfile.write(b': keep-alive\n\n')
        for t in Falso.trozos:
            evento = {'choices': [{'delta': {'content': t}, 'finish_reason': None}]}
            self.wfile.write(b'data: ' + json.dumps(evento).encode('utf-8') + b'\n\n')
        self.wfile.write(b'data: ' + json.dumps({'choices': [{'delta': {}, 'finish_reason': 'stop'}]}).encode() + b'\n\n')
        self.wfile.write(b'data: [DONE]\n\n')


class ConexionIA(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.servidor = ThreadingHTTPServer(('127.0.0.1', 0), Falso)
        cls.url = f'http://127.0.0.1:{cls.servidor.server_address[1]}/v1'
        threading.Thread(target=cls.servidor.serve_forever, daemon=True).start()

    @classmethod
    def tearDownClass(cls):
        cls.servidor.shutdown()
        ia.configurar(Path(tempfile.gettempdir()) / 'no-existe-ia.json')  # deja Ollama, como al empezar

    def setUp(self):
        self.carpeta = tempfile.TemporaryDirectory()
        self.ruta = Path(self.carpeta.name) / 'ia.json'
        Falso.recibidos.clear()
        Falso.trozos = ['Hola', ', ', 'mundo']

    def tearDown(self):
        self.carpeta.cleanup()

    def escribir(self, datos):
        self.ruta.write_text(json.dumps(datos), encoding='utf-8')
        return ia.configurar(self.ruta)

    def test_sin_archivo_es_ollama(self):
        c = ia.configurar(self.ruta)
        self.assertEqual(c['proveedor'], 'ollama')
        self.assertEqual(ia.URL, 'http://127.0.0.1:11434')
        self.assertEqual(ia.nombre_servidor(), 'Ollama')
        self.assertEqual(c['aviso'], '')

    def test_archivo_roto_avisa_y_usa_ollama(self):
        self.ruta.write_text('{ esto no es json', encoding='utf-8')
        c = ia.configurar(self.ruta)
        self.assertEqual(c['proveedor'], 'ollama')
        self.assertIn('ia.json', c['aviso'])
        c = self.escribir({'proveedor': 'chatgpt-web'})
        self.assertEqual(c['proveedor'], 'ollama')
        self.assertIn('desconocido', c['aviso'])

    def test_clave_desde_variable_de_entorno(self):
        os.environ['MI_CLAVE_PRUEBA'] = 'abc'
        try:
            c = self.escribir({'proveedor': 'openai', 'url': self.url + '/', 'clave': 'env:MI_CLAVE_PRUEBA'})
        finally:
            del os.environ['MI_CLAVE_PRUEBA']
        self.assertEqual(c['clave'], 'abc')
        self.assertEqual(ia.URL, self.url)  # sin la barra final

    def test_modelos_y_chat_compatible_openai(self):
        self.escribir({'proveedor': 'openai', 'url': self.url, 'clave': 'secreta', 'nombre': 'Servidor falso'})
        self.assertEqual(ia.modelos(), ['mi-modelo', 'otro'])  # sin los de «embeddings»
        self.assertTrue(ia.instalado())
        self.assertTrue(ia.arrancar())
        self.assertTrue(ia.precalentar('mi-modelo'))  # no hace nada con un servidor externo
        vistos = []
        texto = ia.chat('mi-modelo', [{'role': 'user', 'content': 'hola'}], opciones={'num_predict': 50},
                        al_recibir=vistos.append)
        self.assertEqual(texto, 'Hola, mundo')
        self.assertEqual(vistos[-1], 'Hola, mundo')
        enviado = Falso.recibidos[-1]
        self.assertEqual(enviado['model'], 'mi-modelo')
        self.assertEqual(enviado['max_tokens'], 50)
        self.assertTrue(enviado['stream'])
        self.assertNotIn('options', enviado)  # las opciones de Ollama no se mandan

    def test_modelo_fijado_en_ia_json(self):
        self.escribir({'proveedor': 'openai', 'url': self.url, 'clave': 'secreta', 'modelo': 'otro'})
        self.assertEqual(ia.modelos(), ['otro'])
        self.assertEqual(ia.modelo_recomendado(ia.modelos()), 'otro')

    def test_formatos_json(self):
        esquema = {'type': 'object', 'properties': {'a': {'type': 'string'}}}
        Falso.trozos = ['{"a": ', '"b"}']
        for modo, tipo in (('esquema', 'json_schema'), ('objeto', 'json_object'), ('texto', None)):
            self.escribir({'proveedor': 'openai', 'url': self.url, 'clave': 'secreta', 'json': modo})
            self.assertEqual(json.loads(ia.chat('m', [{'role': 'user', 'content': 'x'}], formato=esquema)), {'a': 'b'})
            enviado = Falso.recibidos[-1]
            self.assertEqual((enviado.get('response_format') or {}).get('type'), tipo, modo)
            if modo != 'esquema':  # sin esquema en la API, va escrito en el mensaje
                self.assertIn('esquema', enviado['messages'][-1]['content'])

    def test_sin_limite_de_tokens(self):
        self.escribir({'proveedor': 'openai', 'url': self.url, 'clave': 'secreta', 'max_tokens': False})
        ia.chat('m', [{'role': 'user', 'content': 'x'}], opciones={'num_predict': 50})
        self.assertNotIn('max_tokens', Falso.recibidos[-1])
        self.assertNotIn('reasoning_effort', Falso.recibidos[-1])  # solo si se pide

    def test_razonamiento(self):
        self.escribir({'proveedor': 'openai', 'url': self.url, 'clave': 'secreta', 'razonamiento': 'none'})
        ia.chat('m', [{'role': 'user', 'content': 'x'}])
        self.assertEqual(Falso.recibidos[-1]['reasoning_effort'], 'none')

    def test_quita_el_razonamiento(self):
        self.escribir({'proveedor': 'openai', 'url': self.url, 'clave': 'secreta'})
        Falso.trozos = ['<think>', 'pienso…', '</think>\n', 'Respuesta']
        vistos = []
        self.assertEqual(ia.chat('m', [], al_recibir=vistos.append), 'Respuesta')
        self.assertNotIn('pienso', ''.join(vistos))

    def test_clave_mala_da_un_error_claro(self):
        self.escribir({'proveedor': 'openai', 'url': self.url, 'clave': 'mala'})
        self.assertIsNone(ia.modelos())
        with self.assertRaises(ia.ErrorIA) as e:
            ia.chat('m', [])
        self.assertIn('401', str(e.exception))
        self.assertIn('clave', str(e.exception))

    def test_servidor_apagado(self):
        self.escribir({'proveedor': 'openai', 'url': 'http://127.0.0.1:9/v1'})
        self.assertIsNone(ia.modelos())
        self.assertFalse(ia.arrancar())
        with self.assertRaises(ia.SinConexion):
            ia.chat('m', [])


if __name__ == '__main__':
    unittest.main()
