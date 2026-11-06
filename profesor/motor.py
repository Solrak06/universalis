"""Lógica del profesor: temario, tests, corrección, progreso y dudas.

Modos de test:
  rapido  preguntas generadas al instante a partir del temario, sin IA.
  ia      solo preguntas redactadas por la IA (las que faltan se preparan mientras respondes).
  mixto   lo mejor de ambos: empieza al instante y la IA sustituye las preguntas rápidas
          que aún no has visto en cuanto tiene listas las suyas.
"""
import json
import os
import math
import random
import re
import threading
import time
import uuid
from collections import Counter
from datetime import date, timedelta
from pathlib import Path

import ia
from banco import Banco
from estudio import Estudio
from herramientas import Herramientas
from temas import Temas
from generador import Generador, _mayuscula, identificador, nombre_tema
from imagenes import Imagenes
from texto import FORMATOS, clave, es_prosa, extraer, frases, limpiar, raices, terminos

CANTIDADES = (5, 10, 15, 20, 25, 30)
DIFICULTADES = ('Básica', 'Intermedia', 'Avanzada')
CONFIG_INICIAL = {'modo': 'mixto', 'n': 10, 'dificultad': 'Intermedia', 'tipo': 'variado',
                  'correccion': 'inmediata', 'penalizar': False, 'temas': [], 'revision': False,
                  'modelo': '', 'auto_guardar': True, 'limite': 0,  # limite: segundos por pregunta
                  'formatos': [],  # formatos de pregunta permitidos (vacío: todos)
                  'reserva': True}  # preparar preguntas de IA en segundo plano


def clave_formato(p):
    """Formato de una pregunta (las de versiones anteriores no lo guardaban: se deduce)."""
    if p.get('tipo') == 'vf':
        return 'vf'
    formato = p.get('formato') or ''
    if formato in ia.FORMATOS:
        return formato
    return 'completar' if p.get('enunciado', '').startswith('Completa') else 'definicion'


def nombre_formato(p):
    """Nombre del formato para la ventana («Caso práctico», «Completar»…)."""
    clave_f = clave_formato(p)
    return 'Verdadero o falso' if clave_f == 'vf' else ia.FORMATOS[clave_f][0]


def racha(actividad, hoy=None):
    """Días seguidos con alguna respuesta, contando hasta hoy (o hasta ayer si hoy aún no hay)."""
    hoy = hoy or date.today()
    dia = hoy if actividad.get(hoy.isoformat()) else hoy - timedelta(days=1)
    dias = 0
    while actividad.get(dia.isoformat()):
        dias += 1
        dia -= timedelta(days=1)
    return dias


class ErrorProfesor(Exception):
    """Error que se muestra tal cual al estudiante."""


def _extracto(texto, buscadas, limite=900):
    """Lo más relevante de un fragmento para la consulta, en su orden y en como mucho `limite` caracteres.
    En CPU la IA lee unas 25 palabras por segundo antes de empezar a escribir: cada párrafo de relleno que se
    le ahorra son segundos menos de espera."""
    if len(texto) <= limite:
        return texto
    trozos = [t.strip() for t in re.split(r'(?<=[.!?:;])\s+|\n+', texto) if t.strip()]
    buscadas = set(buscadas)
    # Cada frase puntúa por las palabras de la consulta que contiene; las vecinas de una buena también suman
    puntos = [sum(1 for r in raices(t) if r in buscadas) for t in trozos]
    vecinos = [puntos[i] + 0.4 * ((puntos[i - 1] if i else 0) + (puntos[i + 1] if i + 1 < len(puntos) else 0))
               for i in range(len(trozos))]
    orden = sorted(range(len(trozos)), key=lambda i: (-vecinos[i], i))
    elegidos, total = set(), 0
    for i in orden:
        if total + len(trozos[i]) > limite and elegidos:
            continue
        elegidos.add(i)
        total += len(trozos[i]) + 1
        if total >= limite:
            break
    salida, anterior = [], -2
    for i in sorted(elegidos):
        if anterior >= 0 and i != anterior + 1:
            salida.append('[…]')
        salida.append(trozos[i])
        anterior = i
    return ' '.join(salida)[:limite + 200]


class Motor(Estudio, Herramientas, Temas):
    def __init__(self, carpeta_datos, emitir=lambda evento: None):
        self.datos = Path(carpeta_datos)
        self.datos.mkdir(parents=True, exist_ok=True)
        self.banco = Banco(self.datos / 'profesor.sqlite')
        self.emitir = emitir
        self.asignatura = None      # {'codigo', 'nombre', 'temario'}
        self.fragmentos = []
        self.indice_busqueda = None
        self.candado = threading.RLock()   # lectura-modificación-escritura de sesiones
        self.hilo_ia = None
        self.cancelar_ia = threading.Event()
        self.cancelar_duda = threading.Event()
        self.ia_estado = {'disponible': False, 'modelos': [], 'modelo': ''}
        self.imagenes = Imagenes(self.datos / 'imagenes')
        self._cache_mazo = None   # (momento, codigo, tarjetas)
        self._ronda_tarjetas = 0
        self._iniciar_estudio()
        self._iniciar_herramientas()
        self._iniciar_temas()
        self._migrar_banco_antiguo()

    # ================================================================ temario

    def hola(self, codigo, nombre, temario, examenes=None):
        """Abre una asignatura: indexa su Temario y devuelve todo lo que necesita la pantalla inicial."""
        self.detener_generacion()
        self.detener_reserva()
        self.detener_resumenes()
        self.detener_temas()
        self.asignatura = {'codigo': codigo, 'nombre': nombre or codigo, 'temario': str(temario)}
        self.poner_examenes(examenes)
        archivos = self.indexar()
        config = {**CONFIG_INICIAL, **self.banco.ajustes(codigo)}
        self.comprobar_ia(config.get('modelo', ''))
        pendiente = self.banco.sesion_pendiente(codigo)
        self.organizar_temas()  # la primera vez, la IA pone nombre a los temas en segundo plano
        return {'asignatura': self.asignatura, 'archivos': archivos, 'config': config, 'ia': self.ia_estado,
                'pendiente': self._resumen_sesion(pendiente) if pendiente else None,
                'resumen': self._resumen_progreso(), 'inicio': self.inicio(),
                'chat': self.conversacion()['chat']}

    def indexar(self):
        raiz = Path(self.asignatura['temario'])
        codigo = self.asignatura['codigo']
        if not raiz.is_dir():
            raiz.mkdir(parents=True, exist_ok=True)
        conocidos = self.banco.archivos(codigo)
        presentes = {}
        for ruta in sorted(raiz.rglob('*')):
            if ruta.is_file() and ruta.suffix.lower() in FORMATOS and not ruta.name.startswith('~$'):
                presentes[ruta.relative_to(raiz).as_posix()] = ruta
        for rel in set(conocidos) - set(presentes):
            self.banco.olvidar_archivo(codigo, rel)
        cambiados = []
        for rel, ruta in presentes.items():
            st = ruta.stat()
            firma = f'{st.st_size}:{int(st.st_mtime)}'
            if conocidos.get(rel, {}).get('firma') != firma:
                cambiados.append((rel, ruta, firma))
        for i, (rel, ruta, firma) in enumerate(cambiados):
            self.emitir({'evento': 'leyendo', 'archivo': ruta.name, 'hechos': i, 'total': len(cambiados)})
            try:
                paginas = extraer(ruta)
                fragmentos = [(u, limpiar(t)) for u, t in paginas]
                fragmentos = [(u, t) for u, t in fragmentos if len(t) >= 40]
                error = '' if fragmentos else 'Sin texto legible (¿PDF escaneado? necesita OCR).'
                self.banco.guardar_archivo(codigo, rel, firma, len(paginas), fragmentos, error)
            except Exception as e:  # un documento roto no impide usar el resto
                self.banco.guardar_archivo(codigo, rel, firma, 0, [], f'No se pudo leer: {e}')
        self.fragmentos = self.banco.fragmentos(codigo)
        self.firmas = {r: a['firma'] for r, a in self.banco.archivos(codigo).items()}
        self.indice_busqueda = None
        self._cache_mazo = None
        conteo = Counter(f['ruta'] for f in self.fragmentos)
        preguntas_ia = Counter(p['ruta'] for p in self.banco.preguntas(codigo, origen='ia'))
        archivos = []
        for rel, info in sorted(self.banco.archivos(codigo).items()):
            archivos.append({'ruta': rel, 'tema': nombre_tema(rel), 'paginas': info['paginas'],
                             'fragmentos': conteo.get(rel, 0), 'preguntas_ia': preguntas_ia.get(rel, 0),
                             'error': info['error']})
        return self.con_grupos(archivos)

    def _rutas(self, config):
        validas = {f['ruta'] for f in self.fragmentos}
        elegidas = [r for r in (config.get('temas') or []) if r in validas]
        return elegidas or sorted(validas)

    # ===================================================================== IA

    def comprobar_ia(self, preferido=''):
        ia.configurar()  # se vuelve a leer ia.json: cambiarlo no obliga a cerrar la app
        disponibles = ia.modelos()
        if disponibles is None:
            # Con la IA ocupada generando, la lista de modelos tarda más de 2 s en llegar: antes de darla por
            # apagada se le da más margen (si de verdad está cerrado, la conexión falla al momento).
            disponibles = ia.modelos(8.0)
        self.ia_estado = {'disponible': bool(disponibles), 'modelos': disponibles or [],
                          'modelo': ia.modelo_recomendado(disponibles or [], preferido),
                          'instalado': ia.instalado(), 'servidor': ia.nombre_servidor(),
                          'proveedor': ia.CONFIG.get('proveedor', 'ollama'), 'aviso': ia.CONFIG.get('aviso', '')}
        return self.ia_estado

    def arrancar_ia(self):
        ia.configurar()
        if not ia.arrancar():
            if ia.es_ollama():
                raise ErrorProfesor('No he podido arrancar Ollama. Ábrelo desde el menú Inicio '
                                    'o instálalo desde ollama.com.')
            raise ErrorProfesor(f'No consigo conectar con {ia.nombre_servidor()} en {ia.URL}. Comprueba que está '
                                'encendido y revisa datos/ia.json (ver CONECTAR_IA.md).')
        return {'ia': self.comprobar_ia()}

    def precalentar(self, modelo=''):
        modelo = modelo or self.ia_estado.get('modelo')
        if not modelo:
            return {'cargando': False}

        def cargar():
            self.emitir({'evento': 'ia', 'estado': 'cargando', 'modelo': modelo})
            listo = ia.precalentar(modelo)
            self.emitir({'evento': 'ia', 'estado': 'lista' if listo else 'error', 'modelo': modelo})
            if listo:
                self.reservar()

        threading.Thread(target=cargar, daemon=True).start()
        return {'cargando': True}

    # ================================================================== tests

    def empezar(self, config):
        if not self.asignatura:
            raise ErrorProfesor('Abre primero una asignatura.')
        config = {**CONFIG_INICIAL, **config}
        de_fallos = bool(config.get('fallos'))  # test hecho solo con «Mis fallos»
        repetir = config.get('repetir')         # token de un test: repetir sus falladas y en blanco
        if config.get('simulacro'):
            # Simulacro de examen: corrección al final, los fallos restan y tiempo limitado.
            config.update(correccion='final', penalizar=True, limite=config.get('limite') or 60)
        if config['n'] not in CANTIDADES and not (de_fallos and config['n'] == 0):
            config['n'] = 10
        if config['dificultad'] not in DIFICULTADES:
            config['dificultad'] = 'Intermedia'
        config['formatos'] = [f for f in (config.get('formatos') or []) if f in ia.FORMATOS]
        codigo = self.asignatura['codigo']
        if not self.fragmentos:
            raise ErrorProfesor('No hay apuntes legibles en el Temario de esta asignatura. Copia allí '
                                'tus PDF, Word, PowerPoint o archivos de texto y vuelve a abrir el profesor.')
        if de_fallos or repetir:
            config['modo'] = 'rapido'  # son preguntas ya hechas: no hay nada que generar
        elif config['modo'] != 'rapido' and not self.ia_estado.get('disponible'):
            self.comprobar_ia(config.get('modelo', ''))
            if not self.ia_estado['disponible']:
                if config['modo'] == 'ia':
                    raise ErrorProfesor(f'La IA ({ia.nombre_servidor()}) no está disponible. Ábrela o elige el modo Rápido.')
                config['modo'] = 'rapido'
        # Test de lo que se esta leyendo (unas paginas) o de un trozo seleccionado: sin esperar a la IA
        de_lectura = bool(config.get('paginas') or config.get('texto'))
        if de_lectura:
            config['modo'] = 'rapido'
        if not de_fallos and not repetir and not de_lectura and not config.get('repaso') and not config.get('simulacro'):
            self.banco.guardar_ajustes(codigo, {k: v for k, v in config.items() if k not in ('repaso', 'fallos')})

        rutas = self._rutas(config)
        n = config['n']
        if repetir:
            anterior = self._sesion(repetir)
            # Las falladas, las que quedaron en blanco y las acertadas sin estar seguro
            ids = [e['id'] for i, e in enumerate(anterior['preguntas'])
                   if e['id'] and (not (anterior['respuestas'].get(i) or {}).get('correcta')
                                   or (anterior['respuestas'].get(i) or {}).get('dudosa'))]
            elegidas = [(p, False) for p in (self.banco.pregunta(i) for i in ids) if p and not p['descartada']]
            if not elegidas:
                raise ErrorProfesor('En ese test no fallaste ninguna pregunta. ¡Enhorabuena!')
            random.shuffle(elegidas)
            config['n'] = len(elegidas)
            rutas = anterior['config'].get('rutas', rutas)
        elif de_fallos:
            guardadas = self.banco.guardadas(codigo)
            if not guardadas:
                raise ErrorProfesor('Aún no tienes preguntas en «Mis fallos». Se guardan solas cuando fallas '
                                    'una pregunta (o con el botón «Guardar en mis fallos»).')
            random.shuffle(guardadas)
            elegidas = [(p, False) for p in (guardadas[:n] if n else guardadas)]
        else:
            elegidas = self._elegir_preguntas(config, rutas, n)
        if config.get('repaso') and not elegidas:
            raise ErrorProfesor('No tienes fallos pendientes de repaso en estos temas. ¡Buen trabajo!')
        if not elegidas and de_lectura:
            raise ErrorProfesor('Hay muy poco texto aquí para hacer preguntas. Selecciona un trozo más largo '
                                'o haz el test de todo el tema.')
        if not elegidas and config['modo'] == 'rapido':
            raise ErrorProfesor('No he encontrado suficientes frases claras en el temario para hacer preguntas. '
                                'Prueba con el modo IA o añade apuntes con más texto.')

        entradas = []
        for p, provisional in elegidas:
            orden = [0, 1] if p['tipo'] == 'vf' else random.sample(range(len(p['opciones'])), len(p['opciones']))
            entradas.append({'id': p['id'], 'orden': orden, 'provisional': provisional, 'mostrada': False})
        if config.get('repaso') or de_fallos or repetir or config['modo'] == 'rapido':
            n = len(entradas)  # sin IA no hay quien prepare los huecos: el test se acorta
        while len(entradas) < n:
            entradas.append({'id': None, 'orden': [], 'provisional': True, 'mostrada': False})

        self.detener_generacion()
        token = uuid.uuid4().hex
        self.banco.abandonar_pendientes(codigo)
        sesion_config = {**config, 'n': n, 'rutas': rutas}
        self.banco.crear_sesion(token, codigo, sesion_config, entradas)
        if config['modo'] != 'rapido' and not config.get('repaso') and not de_fallos and not repetir and                 any(e['provisional'] for e in entradas):
            self.iniciar_generacion(token)
        return {'sesion': self._resumen_sesion(self.banco.sesion(token)), 'pregunta': self.pregunta(token, 0)}

    def _elegir_preguntas(self, config, rutas, n):
        """Lista de (pregunta, provisional). Prioriza repasos pendientes y lo menos visto."""
        codigo = self.asignatura['codigo']
        tipo, dificultad = config['tipo'], config['dificultad']

        formatos = set(config.get('formatos') or [])
        paginas = set(config.get('paginas') or [])
        texto = (config.get('texto') or '').strip()

        def encaja(p):
            if paginas and p.get('ubicacion') not in paginas:
                return False
            if formatos and p['tipo'] != 'vf' and (p.get('formato') or 'definicion') not in formatos:
                return False
            return tipo == 'variado' or p['tipo'] == tipo

        if texto:
            # Solo del trozo elegido: preguntas nuevas sacadas de ese texto
            ubicacion = config.get('ubicacion') or 'selección'
            generador = Generador(codigo, [{'ruta': rutas[0], 'ubicacion': ubicacion, 'texto': texto}])
            nuevas = generador.generar(n * 3, dificultad, tipo, formatos=config.get('formatos'))
            elegidas = []
            for p in nuevas[:n]:
                self.banco.guardar_pregunta(p)
                elegidas.append((p, False))
            return elegidas
        repasar = [p for p in self.banco.para_repasar(codigo, rutas) if encaja(p)]
        if config.get('repaso'):
            return [(p, False) for p in repasar[:n]]

        elegidas, usados = [], set()

        def tomar(lista, cupo, provisional=False):
            for p in lista:
                if len(elegidas) >= cupo:
                    break
                if p['id'] not in usados and clave(p['enunciado']) not in usados:
                    elegidas.append((p, provisional))
                    usados.update((p['id'], clave(p['enunciado'])))

        # Hasta un 30 % del test son repasos de preguntas falladas (repetición espaciada).
        tomar(repasar, max(1, n * 3 // 10) if repasar else 0)
        if (config['modo'] in ('ia', 'mixto') or paginas) and tipo != 'vf':
            banco_ia = [p for p in self.banco.preguntas(codigo, origen='ia', rutas=rutas) if encaja(p)]
            random.shuffle(banco_ia)
            # Primero las de la dificultad pedida y las de formato nuevo (las antiguas eran más fáciles).
            banco_ia.sort(key=lambda p: (p['dificultad'] != dificultad, not p.get('formato'), p['vistas'],
                                         p['ultima']))
            tomar(banco_ia, n)
        if config['modo'] in ('rapido', 'mixto'):
            generador = Generador(codigo, [f for f in self.fragmentos if f['ruta'] in rutas
                                           and (not paginas or f['ubicacion'] in paginas)])
            nuevas = generador.generar(n * 3, dificultad, tipo, formatos=config.get('formatos'))
            vistas = {p['id']: p for p in self.banco.preguntas(codigo, origen='auto', rutas=rutas)}
            nuevas.sort(key=lambda p: (vistas[p['id']]['vistas'] if p['id'] in vistas else 0))
            for p in nuevas:
                self.banco.guardar_pregunta(p)
            tomar(nuevas, n, provisional=config['modo'] == 'mixto')
        random.shuffle(elegidas)
        return elegidas

    def _resumen_sesion(self, s):
        if not s:
            return None
        estados = []
        exam = s['config'].get('correccion') == 'final' and not s['terminada']
        provisionales = s['config'].get('provisionales', {})
        for i, e in enumerate(s['preguntas']):
            if e['id'] is None:
                estados.append('preparando')
            elif i in s['respuestas']:
                estados.append('respondida' if exam else ('bien' if s['respuestas'][i]['correcta'] else 'mal'))
            elif str(i) in provisionales:
                estados.append('respondida')
            else:
                estados.append('sin')
        return {'token': s['token'], 'n': len(s['preguntas']), 'config': s['config'], 'estados': estados,
                'creada': s['creada'], 'terminada': bool(s['terminada']),
                'respondidas': sum(1 for x in estados if x in ('bien', 'mal', 'respondida')),
                'segundos': int(time.time() - s['creada'])}

    def continuar(self):
        s = self.banco.sesion_pendiente(self.asignatura['codigo'])
        if not s:
            raise ErrorProfesor('No tienes ningún test a medias en esta asignatura.')
        resumen = self._resumen_sesion(s)
        siguiente = next((i for i, e in enumerate(resumen['estados']) if e in ('sin', 'preparando')), 0)
        if s['config'].get('modo') != 'rapido' and any(e['id'] is None or (e['provisional'] and not e['mostrada'])
                                                       for e in s['preguntas']):
            self.iniciar_generacion(s['token'])
        return {'sesion': resumen, 'pregunta': self.pregunta(s['token'], siguiente)}

    def _sesion(self, token):
        s = self.banco.sesion(token)
        if not s or s['asignatura'] != (self.asignatura or {}).get('codigo'):
            raise ErrorProfesor('No encuentro ese test.')
        return s

    def pregunta(self, token, indice):
        with self.candado:
            s = self._sesion(token)
            if not 0 <= indice < len(s['preguntas']):
                raise ErrorProfesor('Esa pregunta no existe en el test.')
            entrada = s['preguntas'][indice]
            if entrada['id'] is not None and not entrada['mostrada']:
                entrada['mostrada'] = True   # una vez vista, la IA ya no la sustituye
                self.banco.actualizar_preguntas_sesion(token, s['preguntas'])
        resultado = {'indice': indice, 'n': len(s['preguntas']), 'sesion': self._resumen_sesion(s)}
        if entrada['id'] is None:
            return {**resultado, 'preparando': True}
        p = self.banco.pregunta(entrada['id'])
        orden = entrada['orden'] or list(range(len(p['opciones'])))
        terminada = bool(s['terminada'])
        exam = s['config'].get('correccion') == 'final'
        respuesta = s['respuestas'].get(indice)
        provisional = s['config'].get('provisionales', {}).get(str(indice))
        dudosa = bool((respuesta or {}).get('dudosa')) if respuesta is not None else \
            bool(s['config'].get('dudosas', {}).get(str(indice)))
        eleccion = None
        if respuesta is not None:
            eleccion = orden.index(respuesta['eleccion'])
        elif provisional is not None:
            eleccion = provisional
        revelar = respuesta is not None and (not exam or terminada)
        guardadas = self.banco.ids_guardadas(self.asignatura['codigo'])
        # Fallos de antes de este test (el de ahora no cuenta como «la fallaste antes»).
        fallos_previos = p['fallos'] - (1 if respuesta is not None and not respuesta['correcta'] else 0)
        resultado.update({
            'id': p['id'], 'enunciado': ia.reparar(p['enunciado']), 'opciones': [ia.reparar(p['opciones'][i]) for i in orden],
            'tipo': p['tipo'], 'origen': p['origen'], 'tema': nombre_tema(p['ruta']), 'ruta': p['ruta'],
            'formato': clave_formato(p), 'formato_nombre': nombre_formato(p),
            'archivo': str(Path(self.asignatura['temario']) / p['ruta']), 'ubicacion': p['ubicacion'],
            'dificultad': p['dificultad'], 'eleccion': eleccion, 'respondida': eleccion is not None,
            'corregida': revelar, 'examen': exam and not terminada,
            'vistas': p['vistas'], 'fallos_previos': max(0, fallos_previos), 'terminada': terminada,
            'guardada': p['id'] in guardadas, 'mis_fallos': len(guardadas),
            'de_fallos': bool(s['config'].get('fallos')), 'imagen': self._imagen(p), 'dudosa': dudosa})
        if revelar:
            motivos = p.get('motivos') or []
            resultado.update({'correcta': orden.index(p['respuesta']), 'acierto': bool(respuesta['correcta']),
                              'explicacion': ia.reparar(p['explicacion']), 'cita': ia.reparar(p['cita']),
                              # por qué es correcta o incorrecta cada opción, en el orden en que se muestran
                              'motivos': [ia.reparar(motivos[i]) for i in orden] if len(motivos) == len(p['opciones'])
                              else []})
        return resultado

    def _imagen(self, p):
        archivo = Path(self.asignatura['temario']) / p['ruta']
        return self.imagenes.para(archivo, p['ubicacion'], getattr(self, 'firmas', {}).get(p['ruta'], ''))

    def _registrar(self, s, token, indice, p, original, dudosa=False):
        """Guarda una respuesta definitiva y mantiene «Mis fallos» al día."""
        correcta = original == p['respuesta']
        if not self.banco.responder(token, indice, p['id'], original, correcta, dudosa and correcta):
            return
        codigo = self.asignatura['codigo']
        if not correcta and s['config'].get('auto_guardar', True):
            self.banco.guardar_fallo(codigo, p['id'])
        elif correcta and p['id'] in self.banco.ids_guardadas(codigo):
            self.banco.acierto_en_fallo(codigo, p['id'])

    def guardar(self, token, indice, guardar):
        """Botón «Guardar en mis fallos» / «Quitar de mis fallos» de una pregunta."""
        s = self._sesion(token)
        ident = s['preguntas'][indice]['id']
        if not ident:
            raise ErrorProfesor('Esta pregunta aún se está preparando.')
        if guardar:
            self.banco.guardar_fallo(self.asignatura['codigo'], ident)
        else:
            self.banco.quitar_fallo(self.asignatura['codigo'], ident)
        return {'pregunta': self.pregunta(token, indice),
                'mensaje': 'Guardada en «Mis fallos».' if guardar else 'Quitada de «Mis fallos».'}

    def fallos(self):
        """Contenido de la pantalla «Mis fallos»."""
        lista = []
        for p in self.banco.guardadas(self.asignatura['codigo']):
            lista.append({'id': p['id'], 'enunciado': ia.reparar(p['enunciado']), 'tema': nombre_tema(p['ruta']),
                          'veces': p['fallos'], 'racha': p['racha'], 'guardada': p['guardada'],
                          'origen': p['origen'], 'formato': clave_formato(p), 'formato_nombre': nombre_formato(p),
                          'respuesta': ia.reparar(p['opciones'][p['respuesta']]) if p['opciones'] else ''})
        return {'fallos': lista}

    def quitar_fallo(self, ident=None, token=None, acertadas=False):
        """Quita una pregunta de «Mis fallos», o todas las que se acertaron en un test."""
        codigo = self.asignatura['codigo']
        quitadas = 0
        if ident:
            self.banco.quitar_fallo(codigo, ident)
            quitadas = 1
        if token and acertadas:
            s = self._sesion(token)
            guardadas = self.banco.ids_guardadas(codigo)
            for i, r in s['respuestas'].items():
                if r['correcta'] and r['pregunta'] in guardadas:
                    self.banco.quitar_fallo(codigo, r['pregunta'])
                    quitadas += 1
        return {'quitadas': quitadas, **self.fallos(), 'resumen': self._resumen_progreso()}

    def responder(self, token, indice, eleccion, dudosa=False):
        """`dudosa`: el estudiante ha marcado «No estoy seguro» (si acierta, la pregunta vuelve pronto a repaso)."""
        with self.candado:
            s = self._sesion(token)
            if s['terminada']:
                raise ErrorProfesor('Este test ya está terminado.')
            entrada = s['preguntas'][indice]
            if entrada['id'] is None:
                raise ErrorProfesor('Esta pregunta aún se está preparando.')
            p = self.banco.pregunta(entrada['id'])
            orden = entrada['orden'] or list(range(len(p['opciones'])))
            if not 0 <= eleccion < len(orden):
                raise ErrorProfesor('Elige una de las opciones.')
            if s['config'].get('correccion') == 'final':
                # Modo examen: la respuesta se puede cambiar hasta entregar.
                s['config'].setdefault('provisionales', {})[str(indice)] = eleccion
                s['config'].setdefault('dudosas', {})[str(indice)] = bool(dudosa)
                with self.banco.transaccion() as db:
                    db.execute('UPDATE sesiones SET config=? WHERE token=?',
                               (json.dumps(s['config'], ensure_ascii=False), token))
            else:
                original = orden[eleccion]
                self._registrar(s, token, indice, p, original, dudosa)
        resultado = self.pregunta(token, indice)
        if not s['config'].get('correccion') == 'final':
            resultado['mensaje'] = self._comentario(resultado)
        return resultado

    def _comentario(self, r):
        if r.get('acierto'):
            frases_ok = ['¡Exacto!', '¡Muy bien!', '¡Correcto! Lo tienes claro.', '¡Eso es!', 'Perfecto.']
            if r.get('fallos_previos'):
                return '¡Bien! Esta la habías fallado antes y ahora la dominas.'
            return random.choice(frases_ok)
        return random.choice(['Casi. Mira la explicación, es una buena pista.',
                              'No pasa nada: los errores son los que más enseñan.',
                              'Esta te la volveré a preguntar más adelante para afianzarla.'])

    def descartar(self, token, indice):
        """El estudiante marca una pregunta como errónea: no vuelve a salir."""
        with self.candado:
            s = self._sesion(token)
            entrada = s['preguntas'][indice]
            if entrada['id']:
                self.banco.descartar(entrada['id'])
        return {'mensaje': 'Gracias. No volveré a hacer esa pregunta.'}

    def terminar(self, token):
        self.detener_generacion()
        self.reservar()  # al acabar, la IA repone la reserva de preguntas en cuanto quede libre
        with self.candado:
            s = self._sesion(token)
            if not s['terminada']:
                if s['config'].get('correccion') == 'final':
                    for clave_i, eleccion in s['config'].get('provisionales', {}).items():
                        i = int(clave_i)
                        entrada = s['preguntas'][i]
                        p = self.banco.pregunta(entrada['id'])
                        orden = entrada['orden'] or list(range(len(p['opciones'])))
                        self._registrar(s, token, i, p, orden[eleccion],
                                        bool(s['config'].get('dudosas', {}).get(clave_i)))
                    s = self._sesion(token)
                resultado = self._calcular_resultado(s)
                self.banco.terminar_sesion(token, resultado, int(time.time() - s['creada']))
            s = self._sesion(token)
        guardadas = self.banco.ids_guardadas(self.asignatura['codigo'])
        acertadas = sum(1 for r in s['respuestas'].values() if r['correcta'] and r['pregunta'] in guardadas)
        fallos_test = sum(1 for r in s['respuestas'].values() if not r['correcta'] and r['pregunta'] in guardadas)
        return {'resultado': {**s['resultado'], 'segundos': s['segundos'], 'acertadas_guardadas': acertadas,
                              'fallos_guardados': fallos_test},
                'sesion': self._resumen_sesion(s), 'resumen': self._resumen_progreso()}

    def _calcular_resultado(self, s):
        aciertos = fallos = blanco = dudosas = 0
        penalizacion = 0.0
        por_tema, por_formato, lista = {}, {}, []
        for i, e in enumerate(s['preguntas']):
            p = self.banco.pregunta(e['id']) if e['id'] else None
            r = s['respuestas'].get(i)
            tema = nombre_tema(p['ruta']) if p else '—'
            t = por_tema.setdefault(tema, {'tema': tema, 'aciertos': 0, 'total': 0})
            t['total'] += 1
            formato = nombre_formato(p) if p else '—'
            f = por_formato.setdefault(formato, {'formato': formato, 'aciertos': 0, 'total': 0,
                                                 'clave': clave_formato(p) if p else ''})
            f['total'] += 1
            if r is None:
                blanco += 1
                estado = 'blanco'
            elif r['correcta']:
                aciertos += 1
                t['aciertos'] += 1
                f['aciertos'] += 1
                estado = 'bien'
                dudosas += 1 if r.get('dudosa') else 0
            else:
                fallos += 1
                estado = 'mal'
                penalizacion += 1.0 / max(1, len(p['opciones']) - 1)
            lista.append({'indice': i, 'estado': estado, 'tema': tema, 'formato': formato,
                          'dudosa': bool(r and r.get('dudosa') and r['correcta']),
                          'clave': clave_formato(p) if p else '',
                          'enunciado': (p['enunciado'] if p else 'Pregunta no preparada')[:160]})
        n = max(1, len(s['preguntas']))
        neto = aciertos - (penalizacion if s['config'].get('penalizar') else 0)
        nota = round(max(0.0, neto) * 10.0 / n, 2)
        return {'aciertos': aciertos, 'fallos': fallos, 'blanco': blanco, 'n': n, 'nota': nota, 'dudosas': dudosas,
                'penalizar': bool(s['config'].get('penalizar')), 'por_tema': list(por_tema.values()),
                'por_formato': sorted(por_formato.values(), key=lambda x: -x['total']),
                'preguntas': lista, 'modo': s['config'].get('modo'), 'dificultad': s['config'].get('dificultad'),
                'de_fallos': bool(s['config'].get('fallos')), 'repeticion': bool(s['config'].get('repetir')),
                'simulacro': bool(s['config'].get('simulacro')), 'repaso': bool(s['config'].get('repaso'))}

    # ================================================================ progreso

    def _resumen_progreso(self):
        codigo = self.asignatura['codigo']
        totales = self.banco.totales(codigo)
        historial = self.banco.historial(codigo, 12)
        actividad = self.banco.actividad(codigo)
        mazo = self._estado_mazo(self._mazo(), self.banco.tarjetas(codigo))
        resumen = {**totales, 'precision': round(100 * totales['aciertos'] / totales['respuestas'])
                   if totales['respuestas'] else None,
                   'racha': racha(actividad), 'hoy': actividad.get(date.today().isoformat(), 0),
                   'ultimo_dia': max(actividad) if actividad else '',
                   'repasar': len(self.banco.para_repasar(codigo)),
                   'mis_fallos': len(self.banco.ids_guardadas(codigo)),
                   'tarjetas_pendientes': mazo['pendientes'], 'tarjetas_nuevas': mazo['nuevas'],
                   'tarjetas_total': mazo['total'], 'tarjetas_cajas': mazo['cajas'],
                   'tarjetas_dominadas': mazo['dominadas'],
                   'ultimas_notas': [h['nota'] for h in historial],
                   'ia_nuevas': sum(self.nuevas_ia().values())}
        self._publicar_resumen(codigo, resumen)
        return resumen

    def _publicar_resumen(self, codigo, resumen):
        """Deja el resumen en resumen.json para que la ventana principal lo enseñe."""
        ruta = self.datos / 'resumen.json'
        try:
            todos = json.loads(ruta.read_text(encoding='utf-8')) if ruta.exists() else {}
        except (OSError, ValueError):
            todos = {}
        todos[codigo] = {k: resumen.get(k) for k in (
            'precision', 'respuestas', 'tests', 'racha', 'ultimo_dia', 'mis_fallos',
            'tarjetas_pendientes', 'tarjetas_nuevas', 'tarjetas_total')}
        todos[codigo]['ultima_nota'] = resumen['ultimas_notas'][-1] if resumen['ultimas_notas'] else None
        try:
            temporal = ruta.with_suffix('.tmp')
            temporal.write_text(json.dumps(todos, ensure_ascii=False, indent=1), encoding='utf-8')
            temporal.replace(ruta)
        except OSError:
            pass  # no es imprescindible

    def progreso(self):
        codigo = self.asignatura['codigo']
        temas = []
        for t in self.banco.estadisticas_por_tema(codigo):
            temas.append({**t, 'tema': nombre_tema(t['ruta'])})
        # Por temas y, dentro de cada uno, del apartado más flojo al más fuerte.
        temas = self.con_grupos(temas)
        temas.sort(key=lambda t: (t['grupo_orden'], t['aciertos'] / t['total'] if t['total'] else 1))
        historial = [{'fecha': h['fecha'], 'nota': h['nota'], 'aciertos': h['aciertos'], 'n': h['n'],
                      'modo': h.get('modo')} for h in self.banco.historial(codigo, 20)]
        # Actividad de las últimas 8 semanas, de lunes a domingo, terminando en la semana actual.
        actividad = self.banco.actividad(codigo)
        hoy = date.today()
        inicio = hoy - timedelta(days=hoy.weekday() + 7 * 7)
        dias = [{'fecha': (inicio + timedelta(days=i)).isoformat(),
                 'n': actividad.get((inicio + timedelta(days=i)).isoformat(), 0),
                 'futuro': inicio + timedelta(days=i) > hoy} for i in range(56)]
        return {'resumen': self._resumen_progreso(), 'temas': temas, 'historial': historial, 'actividad': dias}

    # =============================================================== tarjetas

    def _mazo(self):
        """Todas las tarjetas de la asignatura: conceptos del glosario y preguntas del banco.

        Se guarda un minuto en memoria: construirlo exige analizar el temario entero."""
        codigo = self.asignatura['codigo']
        if self._cache_mazo and self._cache_mazo[1] == codigo and time.time() - self._cache_mazo[0] < 60:
            return self._cache_mazo[2]
        generador = Generador(codigo, self.fragmentos)
        mazo, vistos = [], set()

        def anadir(t):
            k = clave(t['frente'])
            if k and k not in vistos and t['reverso'].strip():
                vistos.add(k)
                mazo.append(t)

        # 1) Conceptos: «La curva ROC es…» -> «una gráfica que…»
        for d in generador.definiciones:
            frente = f"{d['articulo']} {d['termino']} {d['verbo']}…"
            anadir({'id': 'g' + identificador(codigo, 'tarjeta', d['termino'], d['resto']),
                    'tipo': 'concepto', 'frente': _mayuscula(frente),
                    'reverso': _mayuscula(d['resto'].rstrip(' .:;,')) + '.',
                    'detalle': '', 'cita': ia.reparar(d['frase']), 'ruta': d['ruta'], 'ubicacion': d['ubicacion']})
        # 2) Preguntas del banco (antes, las de la IA). Si hay pocas, se generan más.
        preguntas = [p for p in self.banco.preguntas(codigo) if p['tipo'] != 'vf']
        if sum(1 for p in preguntas if p['origen'] == 'auto') < 40 and self.fragmentos:
            for p in generador.generar(60, 'Intermedia', 'opcion'):
                self.banco.guardar_pregunta(p)
            preguntas = [p for p in self.banco.preguntas(codigo) if p['tipo'] != 'vf']
        preguntas.sort(key=lambda p: (p['origen'] != 'ia', p['enunciado'].startswith('Completa'), p['creada']))
        for p in preguntas:
            completar = p['enunciado'].startswith('Completa')
            anadir({'id': 'p' + p['id'], 'tipo': 'completar' if completar else 'pregunta',
                    'frente': ia.reparar(p['enunciado']), 'reverso': ia.reparar(p['opciones'][p['respuesta']]),
                    'detalle': ia.reparar(p['explicacion']) if p['origen'] == 'ia' else '', 'cita': ia.reparar(p['cita']),
                    'ruta': p['ruta'], 'ubicacion': p['ubicacion']})
        self._cache_mazo = (time.time(), codigo, mazo)
        return mazo

    @staticmethod
    def _estado_mazo(mazo, estado, ahora=None):
        ahora = ahora or time.time()
        vistas = [estado[t['id']] for t in mazo if t['id'] in estado]
        futuras = [e['proxima'] for e in vistas if e['proxima'] > ahora]
        cajas = [0] * 6  # cuántas tarjetas hay en cada caja de repaso (0 = hoy … 5 = cinco semanas)
        for e in vistas:
            cajas[max(0, min(5, int(e['caja'])))] += 1
        return {'total': len(mazo), 'nuevas': len(mazo) - len(vistas),
                'pendientes': sum(1 for e in vistas if e['proxima'] <= ahora),
                'dominadas': sum(1 for e in vistas if e['caja'] >= 3),
                'cajas': cajas,
                'siguiente': min(futuras) - ahora if futuras else None}

    def tarjetas(self, n=20, temas=None, extra=False):
        """Sesión de tarjetas: primero las que toca repasar, luego nuevas.

        Con `extra` (cuando ya no queda nada para hoy) se repasan las menos sabidas."""
        if not self.asignatura:
            raise ErrorProfesor('Abre primero una asignatura.')
        codigo = self.asignatura['codigo']
        rutas = set(self._rutas({'temas': temas or []}))
        mazo = [t for t in self._mazo() if t['ruta'] in rutas]
        if not mazo:
            raise ErrorProfesor('No he encontrado conceptos claros en estos temas para hacer tarjetas. '
                                'Prueba con otros temas o haz algún test primero.')
        estado = self.banco.tarjetas(codigo)
        ahora = time.time()
        pendientes = sorted((t for t in mazo if t['id'] in estado and estado[t['id']]['proxima'] <= ahora),
                            key=lambda t: (estado[t['id']]['caja'], estado[t['id']]['proxima']))
        nuevas = [t for t in mazo if t['id'] not in estado]
        elegidas = (pendientes + nuevas)[:n]
        if extra and not elegidas:
            vistas = sorted((t for t in mazo if t['id'] in estado),
                            key=lambda t: (estado[t['id']]['caja'], estado[t['id']]['proxima']))
            elegidas = vistas[:n]
        random.shuffle(elegidas)
        salida = []
        for i, t in enumerate(elegidas):
            e = estado.get(t['id'])
            salida.append({**t, 'tema': nombre_tema(t['ruta']), 'caja': e['caja'] if e else 0, 'nueva': e is None,
                           'archivo': str(Path(self.asignatura['temario']) / t['ruta']),
                           'imagen': self._imagen(t) if i == 0 else None})
        # Las figuras del resto se preparan en segundo plano (renderizar un PDF lleva su tiempo).
        self._ronda_tarjetas += 1
        ronda = self._ronda_tarjetas

        def figuras():
            for t in salida[1:]:
                if ronda != self._ronda_tarjetas:
                    return
                try:
                    imagen = self._imagen(t)
                except Exception:
                    imagen = None
                if imagen:
                    self.emitir({'evento': 'imagen_tarjeta', 'tarjeta': t['id'], 'imagen': imagen})

        if len(salida) > 1:
            threading.Thread(target=figuras, daemon=True).start()
        return {'tarjetas': salida, 'mazo': self._estado_mazo(mazo, estado, ahora)}

    def tarjeta(self, ident, nota):
        caja = self.banco.repasar_tarjeta(self.asignatura['codigo'], ident, max(0, min(2, int(nota))))
        return {'caja': caja, 'resumen': self._resumen_progreso()}

    # ============================================================ generación IA

    def iniciar_generacion(self, token):
        self.detener_generacion()
        self.rutas_sesion = Counter()  # temas ya usados en este test: reparte las preguntas entre todos
        self.ronda = 0
        self.cancelar_ia = threading.Event()
        self.hilo_ia = threading.Thread(target=self._generar, args=(token, self.cancelar_ia), daemon=True)
        self.hilo_ia.start()

    def detener_generacion(self):
        if self.hilo_ia and self.hilo_ia.is_alive():
            self.cancelar_ia.set()
            self.hilo_ia.join(timeout=0.5)
        self.hilo_ia = None

    # ------------------------------------------------------- reserva de fondo
    # Mientras el estudiante hace otras cosas, la IA va dejando preparadas preguntas de cada tema
    # (RESERVA sin estrenar por tema) para que el siguiente test empiece ya con preguntas de IA.
    # Solo trabaja cuando la IA está libre y se corta en cuanto el estudiante le pide algo.

    RESERVA = 4

    def nuevas_ia(self, rutas=None):
        """Preguntas de IA del formato actual que aún no han salido en ningún test, por tema."""
        return Counter(p['ruta'] for p in self.banco.preguntas(self.asignatura['codigo'], origen='ia', rutas=rutas)
                       if p.get('formato') and not p['vistas'])

    def reservar(self):
        if os.environ.get('UNIVERSALIS_SIN_RESERVA'):  # pruebas automáticas
            return {'reserva': False}
        if not self.asignatura or not self.ia_estado.get('disponible') or not self.fragmentos:
            return {'reserva': False}
        config = {**CONFIG_INICIAL, **self.banco.ajustes(self.asignatura['codigo'])}
        if not config.get('reserva', True):
            return {'reserva': False}
        hilo = getattr(self, 'hilo_reserva', None)
        if hilo and hilo.is_alive():
            return {'reserva': True}
        self.cancelar_reserva = threading.Event()
        self.hilo_reserva = threading.Thread(target=self._reservar, args=(self.asignatura['codigo'], self.cancelar_reserva),
                                             daemon=True)
        self.hilo_reserva.start()
        return {'reserva': True}

    def poner_reserva(self, activa):
        """Interruptor «Preparar preguntas de IA en segundo plano» (se guarda en los ajustes de la asignatura)."""
        codigo = self.asignatura['codigo']
        self.banco.guardar_ajustes(codigo, {**CONFIG_INICIAL, **self.banco.ajustes(codigo), 'reserva': bool(activa)})
        if activa:
            self.reservar()
        else:
            self.detener_reserva()
        return {'reserva': bool(activa), 'listas': sum(self.nuevas_ia().values())}

    def detener_reserva(self):
        if getattr(self, 'hilo_reserva', None) and self.hilo_reserva.is_alive():
            self.cancelar_reserva.set()
        self.hilo_reserva = None

    def _reservar(self, codigo, cancelar):
        errores = 0
        while not cancelar.is_set():
            # Espera a que la IA lleve un rato libre y no haya un test generando preguntas.
            if not ia.libre(20) or (self.hilo_ia and self.hilo_ia.is_alive()):
                cancelar.wait(3)
                continue
            # Desenchufado, la reserva (la IA a tope durante mucho rato) gasta la batería: espera al cargador.
            if ia.con_bateria():
                if not getattr(self, 'reserva_en_pausa', False):
                    self.reserva_en_pausa = True
                    self.emitir({'evento': 'reserva', 'estado': 'pausada', 'motivo': 'bateria',
                                 'listas': sum(self.nuevas_ia().values())})
                cancelar.wait(30)
                continue
            self.reserva_en_pausa = False
            if not self.asignatura or self.asignatura['codigo'] != codigo:
                return
            rutas = sorted({f['ruta'] for f in self.fragmentos})
            nuevas = self.nuevas_ia(rutas)
            faltan = [r for r in rutas if nuevas.get(r, 0) < self.RESERVA]
            if not faltan:
                self.emitir({'evento': 'reserva', 'estado': 'completa', 'listas': sum(nuevas.values())})
                return
            # Primero los temas con menos reserva, y entre ellos los menos practicados (los que van a salir antes).
            ruta = min(faltan, key=lambda r: (nuevas.get(r, 0), random.random()))
            config = {**CONFIG_INICIAL, **self.banco.ajustes(codigo)}
            modelo = config.get('modelo') or self.ia_estado.get('modelo')
            guardado = getattr(self, 'rutas_sesion', None)
            elegido = self._elegir_pasaje([ruta])
            self.rutas_sesion = guardado
            if not elegido or not modelo:
                return
            _, ventana, _ = elegido
            pasaje = '\n\n'.join(f['texto'] for f in ventana)
            self.emitir({'evento': 'reserva', 'estado': 'trabajando', 'tema': nombre_tema(ruta),
                         'listas': sum(nuevas.values())})
            evitar = [p['enunciado'] for p in self.banco.preguntas(codigo, origen='ia', rutas=[ruta])]
            ronda = getattr(self, 'ronda_reserva', 0)
            self.ronda_reserva = ronda + 1
            try:
                # Cada pregunta se guarda en cuanto está escrita: si la reserva se corta, no se pierde.
                lote, _ = ia.generar_lote(modelo, pasaje, self.asignatura['nombre'], config['dificultad'], n=3,
                                          evitar=evitar, cancelar=cancelar, fondo=True,
                                          al_pregunta=lambda q: self.banco.guardar_pregunta(
                                              self._pregunta_ia(q, ruta, ventana, config, modelo)),
                                          formatos=ia.elegir_formatos(3, config['dificultad'],
                                                                      config.get('formatos'), ronda))
                errores = 0
            except ia.Cancelado:
                self.emitir({'evento': 'reserva', 'estado': 'pausada', 'listas': sum(nuevas.values())})
                continue  # el estudiante ha pedido algo a la IA: se vuelve a esperar
            except ia.ErrorIA:
                errores += 1
                if errores >= 3:
                    self.emitir({'evento': 'reserva', 'estado': 'parada', 'listas': sum(nuevas.values())})
                    return
                continue
            if cancelar.is_set() or self.asignatura['codigo'] != codigo:
                return
            for q in lote:
                self.banco.guardar_pregunta(self._pregunta_ia(q, ruta, ventana, config, modelo))
            self.emitir({'evento': 'reserva', 'estado': 'lote', 'tema': nombre_tema(ruta),
                         'listas': sum(self.nuevas_ia(rutas).values())})

    def _huecos(self, s):
        """Posiciones que la IA puede rellenar: vacías o provisionales aún no vistas."""
        return [i for i, e in enumerate(s['preguntas'])
                if e['id'] is None or (e['provisional'] and not e['mostrada'] and i not in s['respuestas']
                                       and str(i) not in s['config'].get('provisionales', {}))]

    def _elegir_pasaje(self, rutas, mezclar=False):
        """(ruta, fragmentos, mezclado). Con `mezclar` y varios temas, el pasaje junta trozos de dos temas
        distintos para que la IA pregunte cómo se relacionan (tests de todo el temario)."""
        codigo = self.asignatura['codigo']
        hechas = Counter(p['ruta'] for p in self.banco.preguntas(codigo, origen='ia'))
        usadas = getattr(self, 'rutas_sesion', None) or Counter()
        por_ruta = {}
        for f in self.fragmentos:
            if f['ruta'] in rutas:
                por_ruta.setdefault(f['ruta'], []).append(f)
        if not por_ruta:
            return None
        # Temas menos usados en este test y con menos preguntas primero, con algo de azar.
        orden = sorted(por_ruta, key=lambda r: usadas.get(r, 0) * 10 + hechas.get(r, 0) + random.random() * 2)
        elegidas = orden[:2] if mezclar and len(orden) > 1 else orden[:1]
        limite = 1900 // len(elegidas)
        ventana = []
        for ruta in elegidas:
            trozos = por_ruta[ruta]
            # Mejor empezar por páginas con texto explicado que por portadas, índices o listas de nombres:
            # sin explicaciones, la IA rellena con lo que sabe y la pregunta se descarta.
            prosa = [sum(len(x) for x in frases(f['texto'])) for f in trozos]
            inicio = random.choices(range(len(trozos)), weights=[p + 40 for p in prosa])[0]
            total = 0
            for f, p in zip(trozos[inicio:] + trozos[:inicio], prosa[inicio:] + prosa[:inicio]):
                if p < 60 and total and len(trozos) > 3:
                    continue  # página casi sin frases (título, lista): se salta
                if total + len(f['texto']) > limite and total:
                    break
                ventana.append(f if len(f['texto']) <= limite + 200 else {**f, 'texto': f['texto'][:limite + 200]})
                total += len(f['texto'])
            usadas[ruta] += 1
        self.rutas_sesion = usadas
        return elegidas[0], ventana, len(elegidas) > 1

    def _generar(self, token, cancelar):
        errores = 0
        inicio = time.monotonic()
        while not cancelar.is_set():
            s = self.banco.sesion(token)
            if not s or s['terminada']:
                return
            huecos = self._huecos(s)
            if not huecos:
                self.emitir({'evento': 'generacion', 'token': token, 'estado': 'terminada'})
                return
            config = s['config']
            modelo = config.get('modelo') or self.ia_estado.get('modelo')
            permitidos = config.get('formatos') or []
            rutas = config.get('rutas') or self._rutas(config)
            # En los tests de varios temas, uno de cada tres lotes mezcla dos temas (preguntas de relacionar).
            ronda = getattr(self, 'ronda', 0)
            self.ronda = ronda + 1
            mezclar = len(rutas) > 1 and ronda % 3 == 2 and (not permitidos or 'relacion' in permitidos)
            elegido = self._elegir_pasaje(rutas, mezclar)
            if not elegido or not modelo:
                return
            ruta, ventana, mezclado = elegido
            pasaje = '\n\n'.join(f['texto'] for f in ventana)
            hechas = len(s['preguntas']) - len(huecos)
            estado = {'evento': 'generacion', 'token': token, 'estado': 'redactando', 'tema': nombre_tema(ruta),
                      'listas': hechas, 'total': len(s['preguntas']), 'faltan': len(huecos)}
            self.emitir({**estado, 'progreso': 0})
            momento = [0.0]

            def al_recibir(texto):
                ahora = time.monotonic()
                if ahora - momento[0] > 0.7:
                    momento[0] = ahora
                    # ~650 caracteres por pregunta: avance aproximado del lote.
                    self.emitir({**estado, 'progreso': min(99, len(texto) * 100 // 1900),
                                 'segundos': int(ahora - inicio)})

            evitar = [p['enunciado'] for p in self.banco.preguntas(self.asignatura['codigo'], origen='ia', rutas=[ruta])]
            try:
                n = 3 if len(huecos) >= 3 else len(huecos) if len(huecos) > 1 else 2
                formatos = ia.elegir_formatos(n, config['dificultad'], permitidos, ronda)
                if mezclado:
                    formatos = ['relacion'] + [f for f in formatos if f != 'relacion'][:n - 1]
                colocadas = set()

                def entregar(q):
                    # Cada pregunta entra en el test en cuanto la IA la termina (sin la verificación doble,
                    # que necesita el lote entero).
                    if not cancelar.is_set():
                        self._entregar(token, self._pregunta_ia(q, ruta, ventana, config, modelo))
                        colocadas.add(q['enunciado'])

                lote, _ = ia.generar_lote(modelo, pasaje, self.asignatura['nombre'], config['dificultad'],
                                          n=n, evitar=evitar, al_recibir=al_recibir, cancelar=cancelar,
                                          formatos=formatos, al_pregunta=None if config.get('revision') else entregar,
                                          verificar_falsas=not config.get('revision'))
                lote = [q for q in lote if q['enunciado'] not in colocadas]
                if config.get('revision') and lote:
                    self.emitir({**estado, 'estado': 'revisando', 'progreso': 99})
                    lote, _ = ia.revisar_lote(modelo, pasaje, self.asignatura['nombre'], lote, cancelar=cancelar)
                errores = 0 if lote else errores + 1
            except ia.Cancelado:
                return
            except ia.ErrorIA as e:
                errores += 1
                self.emitir({'evento': 'aviso', 'mensaje': str(e)})
                lote = []
            if errores >= 3:
                self.emitir({'evento': 'generacion', 'token': token, 'estado': 'parada',
                             'mensaje': 'La IA no consigue preparar más preguntas ahora mismo.'})
                return
            for q in lote:
                if cancelar.is_set():
                    return
                self._entregar(token, self._pregunta_ia(q, ruta, ventana, config, modelo))

    def _entregar(self, token, pregunta):
        """Guarda una pregunta de IA y la pone en el primer hueco libre del test."""
        self.banco.guardar_pregunta(pregunta)
        indice = self._colocar(token, pregunta)
        if indice is not None:
            self.emitir({'evento': 'pregunta_lista', 'token': token, 'indice': indice,
                         'sesion': self._resumen_sesion(self.banco.sesion(token))})

    def _pregunta_ia(self, q, ruta, ventana, config, modelo):
        ubicacion = ventana[0]['ubicacion']
        for f in ventana:
            if clave(q['cita'])[:60] in clave(f['texto']):
                # En los pasajes de dos temas, la pregunta es del tema de la frase que la respalda.
                ruta, ubicacion = f['ruta'], f['ubicacion']
                break
        return {'id': identificador(self.asignatura['codigo'], 'ia', q['enunciado'], q['opciones'][q['respuesta']]),
                'asignatura': self.asignatura['codigo'], 'ruta': ruta, 'ubicacion': ubicacion, 'origen': 'ia',
                'tipo': 'opcion', 'dificultad': config['dificultad'], 'enunciado': q['enunciado'],
                'opciones': q['opciones'], 'respuesta': q['respuesta'], 'explicacion': q['explicacion'],
                'cita': q['cita'], 'modelo': modelo, 'formato': q.get('formato', ''), 'motivos': q.get('motivos') or []}

    def _colocar(self, token, pregunta):
        with self.candado:
            s = self.banco.sesion(token)
            if not s or s['terminada']:
                return None
            if any(e['id'] == pregunta['id'] for e in s['preguntas']):
                return None
            huecos = self._huecos(s)
            if not huecos:
                return None
            vacios = [i for i in huecos if s['preguntas'][i]['id'] is None]
            indice = vacios[0] if vacios else huecos[-1]
            s['preguntas'][indice] = {'id': pregunta['id'], 'orden': random.sample(range(4), 4),
                                      'provisional': False, 'mostrada': False}
            self.banco.actualizar_preguntas_sesion(token, s['preguntas'])
            return indice

    def rellenar(self, token):
        """Completa con preguntas rápidas los huecos que la IA no ha llegado a preparar."""
        self.detener_generacion()
        with self.candado:
            s = self._sesion(token)
            vacios = [i for i, e in enumerate(s['preguntas']) if e['id'] is None]
            if vacios:
                usados = {e['id'] for e in s['preguntas']}
                generador = Generador(self.asignatura['codigo'],
                                      [f for f in self.fragmentos if f['ruta'] in s['config']['rutas']])
                nuevas = [p for p in generador.generar(len(vacios) * 3, s['config']['dificultad'], 'opcion',
                                                       formatos=s['config'].get('formatos'))
                          if p['id'] not in usados]
                for i, p in zip(vacios, nuevas):
                    self.banco.guardar_pregunta(p)
                    s['preguntas'][i] = {'id': p['id'], 'orden': random.sample(range(len(p['opciones'])),
                                                                               len(p['opciones'])),
                                         'provisional': False, 'mostrada': False}
                self.banco.actualizar_preguntas_sesion(token, s['preguntas'])
        return {'sesion': self._resumen_sesion(self.banco.sesion(token))}

    # ================================================================= dudas

    def _buscar(self, consulta, limite=3, recortar=True):
        """BM25 sencillo sobre los fragmentos del temario. Con `recortar`, de cada fragmento solo lo relevante
        (para pasárselo a la IA); sin él, el texto entero (para el buscador de apuntes)."""
        if self.indice_busqueda is None:
            # El nombre del tema cuenta como parte de cada fragmento («explícame la validación cruzada»).
            docs = [Counter(raices(f['texto']) + raices(nombre_tema(f['ruta'])) * 2) for f in self.fragmentos]
            df = Counter()
            for d in docs:
                df.update(d.keys())
            media = sum(sum(d.values()) for d in docs) / max(1, len(docs))
            self.indice_busqueda = (docs, df, media)
        docs, df, media = self.indice_busqueda
        palabras = list(dict.fromkeys(raices(consulta)))  # cada palabra de la consulta cuenta una vez
        n = len(docs)
        puntos = []
        for i, d in enumerate(docs):
            largo = sum(d.values()) or 1
            total = 0.0
            for w in palabras:
                if w in d:
                    idf = math.log(1 + (n - df[w] + 0.5) / (df[w] + 0.5))
                    total += idf * d[w] * 2.2 / (d[w] + 1.2 * (0.25 + 0.75 * largo / media))
            if total > 0:
                # Una portada, los rótulos de una gráfica o una tabla de números casi no explican nada: cuentan menos.
                t = self.fragmentos[i]['texto']
                total *= min(1.0, len(t) / 300) ** 0.5
                letras = sum(c.isalpha() for c in t) / max(1, len(t))
                if letras < 0.7:
                    total *= max(0.2, letras / 0.7) ** 2
                puntos.append((total, i))
        puntos.sort(reverse=True)
        elegidos, usados = [], set()
        for _, i in puntos:
            if i in usados:
                continue
            f = self.fragmentos[i]
            texto, ubicacion, j = f['texto'], f['ubicacion'], i
            usados.add(i)
            # Página corta: se completa con las dos siguientes del mismo documento (si no se han usado ya).
            while (len(texto) < 700 and j - i < 2 and j + 1 < len(self.fragmentos) and j + 1 not in usados
                   and self.fragmentos[j + 1]['ruta'] == f['ruta'] and es_prosa(self.fragmentos[j + 1]['texto'])):
                j += 1
                usados.add(j)
                texto += '\n' + self.fragmentos[j]['texto']
                ubicacion = f'{f["ubicacion"]}-{self.fragmentos[j]["ubicacion"].split()[-1]}'
            elegidos.append({'titulo': f'{nombre_tema(f["ruta"])} · {ubicacion}', 'texto': _extracto(texto, palabras) if recortar else texto,
                             'ruta': f['ruta'], 'archivo': str(Path(self.asignatura['temario']) / f['ruta'])})
            if len(elegidos) == limite:
                break
        return elegidos

    def duda(self, texto, token=None, indice=None, id_peticion=None, historial=(), pregunta=None, etiqueta=''):
        """Responde una duda con el temario. `pregunta`: id de una pregunta del banco (p. ej. de «Mis fallos»)
        o un texto libre de contexto (una tarjeta) que acompaña a la duda."""
        texto = (texto or '').strip()
        if not texto:
            raise ErrorProfesor('Escribe tu duda y te la explico.')
        contexto = ''
        trozo = False  # el contexto es un texto libre (una tarjeta o un trozo de los apuntes)
        consulta = texto
        if pregunta:
            p = self.banco.pregunta(pregunta) if isinstance(pregunta, str) and len(pregunta) == 20 else None
            if p:
                contexto = p['enunciado'] + '\n' + '\n'.join(f'{"ABCD"[i]}) {o}' for i, o in enumerate(p['opciones']))
                contexto += f'\nRespuesta correcta: {"ABCD"[p["respuesta"]]}. {p["explicacion"]}'
                consulta += ' ' + p['enunciado'] + ' ' + p['opciones'][p['respuesta']]
            else:
                contexto = str(pregunta)[:1600]  # una tarjeta o un trozo de los apuntes
                trozo = True
                consulta += ' ' + contexto
        # Una repregunta («¿y un ejemplo?») se busca junto con la pregunta anterior.
        anteriores = [h.get('pregunta', '') for h in historial or [] if isinstance(h, dict)]
        if anteriores:
            consulta += ' ' + anteriores[-1]
        if token is not None and indice is not None:
            try:
                p = self.pregunta(token, indice)
                if p.get('enunciado'):
                    contexto = p['enunciado'] + '\n' + '\n'.join(
                        f'{"ABCD"[i]}) {o}' for i, o in enumerate(p['opciones']))
                    if p.get('corregida'):
                        contexto += f'\nRespuesta correcta: {"ABCD"[p["correcta"]]}. {p.get("explicacion", "")}'
                    consulta += ' ' + p['enunciado'] + ' ' + ' '.join(p['opciones'])
            except ErrorProfesor:
                pass
        if trozo and not (token is not None and indice is not None):
            # Duda sobre un trozo de los apuntes (o «explícame esta página»): el trozo ya va en el contexto, así
            # que bastan dos fragmentos más y sobran los que repiten ese mismo texto. Menos que leer = antes
            # empieza a escribir.
            muestra = clave(contexto)[:400]
            pasajes = [p for p in self._buscar(consulta, limite=3)
                       if clave(p['texto'])[:120] not in muestra and muestra[:120] not in clave(p['texto'])][:2]
        else:
            # Una repregunta («más sencillo», «un ejemplo») ya lleva la respuesta anterior: le bastan dos fragmentos
            pasajes = self._buscar(consulta, limite=2 if anteriores else 3)
        fuentes = [{'titulo': p['titulo'], 'archivo': p['archivo']} for p in pasajes]
        if not self.ia_estado.get('disponible'):
            self.comprobar_ia()
        if not self.ia_estado.get('disponible'):
            # Sin IA no hay quien explique nada: antes se devolvían trozos del temario tal cual, una
            # respuesta que no respondía a la duda. La ventana no ofrece preguntar sin IA.
            raise ErrorProfesor(f'Para responder dudas hace falta la IA ({ia.nombre_servidor()}), y ahora no está '
                                'disponible. Enciéndela y vuelve a preguntar.')
        if not pasajes and not anteriores and not contexto:
            respuesta = ('No encuentro nada sobre eso en tus apuntes. ¿Me dices de qué tema o concepto? '
                         'Por ejemplo: «' + (self.sugerencias_chat() or ['¿Qué es el sobreajuste?'])[0] + '»')
            return {'respuesta': respuesta, 'fuentes': []}
        self.cancelar_duda = threading.Event()
        momento = [0.0]

        def al_recibir(parcial):
            ahora = time.monotonic()
            if ahora - momento[0] > 0.25:
                momento[0] = ahora
                self.emitir({'evento': 'duda', 'id': id_peticion, 'texto': ia.texto_rico(parcial)})

        try:
            respuesta = ia.responder_duda(self.ia_estado['modelo'], self.asignatura['nombre'], texto, pasajes,
                                          contexto, al_recibir=al_recibir, cancelar=self.cancelar_duda,
                                          historial=[(h.get('pregunta', ''), h.get('respuesta', ''))
                                                     for h in historial or [] if isinstance(h, dict)],
                                          trozo=trozo and not (token is not None and indice is not None))
        except ia.Cancelado:
            return {'respuesta': '(Explicación cancelada)', 'fuentes': fuentes}
        except ia.ErrorIA as e:
            raise ErrorProfesor(str(e))
        respuesta = ia.texto_rico(respuesta)
        self._guardar_turno(texto, respuesta, fuentes, etiqueta)
        return {'respuesta': respuesta, 'fuentes': fuentes}

    def _guardar_turno(self, pregunta, respuesta, fuentes, etiqueta):
        if self.asignatura:
            self.banco.guardar_turno(self.asignatura['codigo'], pregunta, respuesta, fuentes, etiqueta or '')

    # ========================================================== compatibilidad

    def _migrar_banco_antiguo(self):
        """Recupera las preguntas de IA de la versión anterior (datos/profesor/progreso.sqlite)."""
        antiguo = self.datos / 'progreso.sqlite'
        if not antiguo.exists() or self.banco.ajustes('__migracion__'):
            return
        import sqlite3
        importadas = 0
        try:
            db = sqlite3.connect(str(antiguo))
            filas = db.execute('SELECT subject, level, model, payload FROM question_bank').fetchall()
            db.close()
        except sqlite3.Error:
            filas = []
        for asignatura, nivel, modelo, payload in filas:
            try:
                q = json.loads(payload)
                archivo = Path(q['source']['file'])
                partes = archivo.parts
                rel = '/'.join(partes[partes.index('Temario') + 1:]) if 'Temario' in partes else archivo.name
                opciones = [re.sub(r'^\s*[A-D][.)]\s+', '', o) for o in q['options']]
                self.banco.guardar_pregunta({
                    'id': identificador(asignatura, 'ia', q['question'], opciones[q['answer']]),
                    'asignatura': asignatura, 'ruta': rel, 'ubicacion': q['source'].get('location', ''),
                    'origen': 'ia', 'tipo': 'opcion', 'dificultad': nivel or 'Intermedia',
                    'enunciado': q['question'], 'opciones': opciones, 'respuesta': q['answer'],
                    'explicacion': q.get('explanation', ''), 'cita': q.get('quote', ''), 'modelo': modelo or ''})
                importadas += 1
            except (KeyError, ValueError, TypeError, IndexError):
                continue
        self.banco.guardar_ajustes('__migracion__', {'hecha': time.time(), 'importadas': importadas})
