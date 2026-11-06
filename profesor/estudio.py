"""Herramientas de estudio del profesor, además de los tests: plan del día, resúmenes por tema,
preguntas de desarrollo corregidas, análisis del progreso, trucos para las tarjetas y conversación.

Todo funciona sin IA (con resultados más sencillos, construidos con las frases del temario) y mejora
con ella. Las llamadas a la IA son lentas en un portátil, así que:
  * los resultados se guardan (resúmenes, preguntas, trucos, análisis) y se reutilizan;
  * el texto se va enviando a la ventana mientras se escribe (eventos «parcial»);
  * cada tarea se puede cancelar con su id de petición.

`Estudio` es una clase base de `Motor`: usa sus atributos (banco, asignatura, fragmentos, ia_estado...).
"""
import random
import re
import threading
import time
from collections import Counter
from datetime import date, datetime, timedelta

import ia
from generador import Generador, _mayuscula, identificador, nombre_tema
from texto import clave, es_prosa, frases, terminos

REPASO_DIAS = [0, 1, 3, 7, 16, 35]
LIMITE_RESUMEN = 4800      # caracteres del apunte que lee la IA para resumir (≈ 1 min de lectura en CPU)
LIMITE_PASAJE = 2200       # para preguntas de desarrollo y su corrección


class Cancelable:
    """Registro de tareas en curso por id de petición, para poder cancelarlas desde la ventana."""

    def __init__(self):
        self._tareas = {}
        self._candado = threading.Lock()

    def nueva(self, ident):
        evento = threading.Event()
        if ident is not None:
            with self._candado:
                self._tareas[ident] = evento
        return evento

    def fin(self, ident):
        with self._candado:
            self._tareas.pop(ident, None)

    def cancelar(self, ident=None):
        with self._candado:
            for k, e in self._tareas.items():
                if ident is None or k == ident:
                    e.set()


def _cuenta(n, uno, varios):
    """'1 tarjeta pendiente' / '3 tarjetas pendientes'."""
    return f'{n} {uno if n == 1 else varios}'


def _hoy_epoch():
    return datetime.combine(date.today(), datetime.min.time()).timestamp()


def _dias(n):
    return 'hoy' if n == 0 else 'mañana' if n == 1 else f'en {n} días'


class Estudio:
    """Mezcla para Motor (ver motor.py)."""

    def _iniciar_estudio(self):
        self.tareas = Cancelable()
        self.examenes = []
        self.hilo_resumenes = None
        self.cancelar_resumenes = threading.Event()

    # ============================================================== utilidades

    def _codigo(self):
        if not self.asignatura:
            from motor import ErrorProfesor
            raise ErrorProfesor('Abre primero una asignatura.')
        return self.asignatura['codigo']

    def _emisor_parcial(self, ident, tipo, cada=0.3, transformar=ia.texto_rico, **extra):
        """Devuelve un `al_recibir` que manda el texto acumulado a la ventana (limitado en frecuencia)."""
        momento = [0.0]

        def al_recibir(parcial):
            ahora = time.monotonic()
            if ahora - momento[0] > cada:
                momento[0] = ahora
                texto = transformar(parcial) if transformar else ''
                self.emitir({'evento': 'parcial', 'id': ident, 'tipo': tipo, 'texto': texto,
                             'caracteres': len(parcial), **extra})
        return al_recibir

    def _ia_lista(self):
        if not self.ia_estado.get('disponible'):
            self.comprobar_ia()
        return self.ia_estado.get('disponible') and self.ia_estado.get('modelo')

    def _fragmentos_de(self, ruta):
        return [f for f in self.fragmentos if f['ruta'] == ruta]

    def _pasaje_completo(self, ruta, limite):
        """Texto del documento recortado a `limite` caracteres, repartido por todo el documento."""
        trozos = self._fragmentos_de(ruta)
        if not trozos:
            return ''
        # Las páginas que son tablas de números no ayudan a resumir: fuera, si queda texto suficiente
        prosa = [f for f in trozos if es_prosa(f['texto'])]
        if len(prosa) >= max(2, len(trozos) // 3):
            trozos = prosa
        total = sum(len(f['texto']) for f in trozos)
        if total <= limite:
            return '\n'.join(f['texto'] for f in trozos)
        # Se reparte el cupo entre las páginas (las más informativas primero si no caben todas).
        cupo = max(320, limite // len(trozos))
        if cupo * len(trozos) > limite * 1.15:
            cuantos = max(1, limite // 320)
            paso = len(trozos) / cuantos
            trozos = [trozos[int(i * paso)] for i in range(cuantos)]
            cupo = limite // len(trozos)
        partes = []
        for f in trozos:
            t = f['texto']
            if len(t) > cupo:
                corte = t.rfind('. ', 0, cupo)
                t = t[:corte + 1] if corte > cupo // 2 else t[:cupo]
            partes.append(t)
        return '\n'.join(partes)[:limite + 200]

    def _pasaje_alrededor(self, ruta, ubicacion=None, limite=LIMITE_PASAJE):
        """Ventana de páginas seguidas de un documento, empezando en `ubicacion` o al azar."""
        trozos = self._fragmentos_de(ruta)
        if not trozos:
            return '', ''
        inicio = next((i for i, f in enumerate(trozos) if f['ubicacion'] == ubicacion), None)
        if inicio is None:
            inicio = random.randrange(len(trozos))
        ventana, total = [], 0
        for f in trozos[inicio:] + trozos[:inicio]:
            if total + len(f['texto']) > limite and ventana:
                break
            ventana.append(f)
            total += len(f['texto'])
        return '\n'.join(f['texto'] for f in ventana)[:limite + 400], ventana[0]['ubicacion']

    def _rutas_legibles(self):
        vistas = []
        for f in self.fragmentos:
            if f['ruta'] not in vistas:
                vistas.append(f['ruta'])
        return vistas

    # ================================================================ exámenes

    def poner_examenes(self, examenes):
        """La ventana manda las pruebas evaluables del calendario: [{nombre, fecha 'AAAA-MM-DD', peso}]."""
        limpios = []
        for e in examenes or []:
            try:
                dia = date.fromisoformat(str(e.get('fecha', ''))[:10])
            except ValueError:
                continue
            if e.get('calificado'):
                continue
            limpios.append({'nombre': str(e.get('nombre', 'Prueba')), 'fecha': dia.isoformat(),
                            'peso': float(e.get('peso') or 0)})
        limpios.sort(key=lambda e: e['fecha'])
        self.examenes = limpios

    def proximo_examen(self):
        hoy = date.today()
        for e in self.examenes:
            dias = (date.fromisoformat(e['fecha']) - hoy).days
            if dias >= 0:
                return {**e, 'dias': dias}
        return None

    # ================================================================== inicio

    def _estadisticas_temas(self):
        """[{ruta, tema, total, aciertos, precision}] de todos los documentos legibles."""
        codigo = self._codigo()
        hechas = {t['ruta']: t for t in self.banco.estadisticas_por_tema(codigo)}
        lista = []
        for ruta in self._rutas_legibles():
            t = hechas.get(ruta, {'total': 0, 'aciertos': 0})
            lista.append({'ruta': ruta, 'tema': nombre_tema(ruta), 'total': t['total'], 'aciertos': t['aciertos'],
                          'precision': round(100 * t['aciertos'] / t['total']) if t['total'] else None})
        return lista

    def plan(self):
        """Tareas concretas para hoy, de más a menos urgente. Cada una sabe si ya está hecha."""
        codigo = self._codigo()
        hoy = _hoy_epoch()
        examen = self.proximo_examen()
        mazo = self._estado_mazo(self._mazo(), self.banco.tarjetas(codigo))
        repasos_hoy = self.banco.repasos_desde(codigo, hoy)
        por_tema_hoy = self.banco.hoy_por_tema(codigo, hoy)
        tests_hoy = self.banco.sesiones_desde(codigo, hoy)
        intentos_hoy = [i for i in self.banco.intentos(codigo, 50) if i['momento'] >= hoy]
        temas = self._estadisticas_temas()
        resumenes = self.banco.resumenes(codigo)
        tareas = []

        # 1) Tarjetas: las que toca repasar, o unas cuantas nuevas.
        if mazo['pendientes'] > 0:
            tareas.append({'tipo': 'tarjetas', 'titulo': 'Repasa ' + _cuenta(mazo['pendientes'], 'tarjeta pendiente', 'tarjetas pendientes'),
                           'detalle': 'Repetición espaciada: hoy es el mejor día para no olvidarlas.', 'hecha': False})
        elif mazo['nuevas'] > 0:
            hecho = repasos_hoy >= 10
            tareas.append({'tipo': 'tarjetas', 'titulo': 'Aprende 10 tarjetas nuevas',
                           'detalle': f"{repasos_hoy} {'repasada' if repasos_hoy == 1 else 'repasadas'} hoy" if repasos_hoy else
                           f"Te quedan {mazo['nuevas']} conceptos por ver.", 'hecha': hecho})
        elif mazo['total']:
            tareas.append({'tipo': 'tarjetas', 'titulo': 'Tarjetas al día', 'detalle': 'No te toca repasar ninguna.',
                           'hecha': True})

        # 2) Preguntas falladas cuyo repaso toca hoy.
        repasar = len(self.banco.para_repasar(codigo))
        if repasar:
            tareas.append({'tipo': 'repaso', 'titulo': 'Repasa ' + _cuenta(min(repasar, 10), 'pregunta que fallaste', 'preguntas que fallaste'),
                           'detalle': 'Vuelven ahora para que se te queden.', 'hecha': False})

        # 3) El tema más flojo (o uno sin practicar).
        practicados = [t for t in temas if t['total'] >= 2]
        flojo = min(practicados, key=lambda t: t['aciertos'] / t['total'], default=None)
        nuevo = next((t for t in temas if t['total'] == 0), None)
        if flojo and flojo['precision'] < 75:
            hecho = por_tema_hoy.get(flojo['ruta'], 0) >= 5
            tareas.append({'tipo': 'tema', 'ruta': flojo['ruta'], 'titulo': f"Practica «{flojo['tema']}»",
                           'detalle': f"Llevas un {flojo['precision']} % de aciertos: es tu tema más flojo.",
                           'hecha': hecho})
        if nuevo:
            leido = nuevo['ruta'] in resumenes and resumenes[nuevo['ruta']]['creado'] >= hoy
            tareas.append({'tipo': 'resumen', 'ruta': nuevo['ruta'], 'titulo': f"Empieza «{nuevo['tema']}»",
                           'detalle': 'Aún no lo has practicado: lee su resumen y luego haz un test.',
                           'hecha': leido or por_tema_hoy.get(nuevo['ruta'], 0) >= 5})

        # 4) Cerca del examen, un simulacro; si no, una pregunta de desarrollo.
        if examen and examen['dias'] <= 5:
            hecho = any(t['config'].get('simulacro') for t in tests_hoy)
            tareas.append({'tipo': 'simulacro', 'titulo': f"Haz un simulacro de {examen['nombre']}",
                           'detalle': f"Es {_dias(examen['dias'])}: entrénalo en condiciones de examen.",
                           'hecha': hecho})
        else:
            tareas.append({'tipo': 'desarrollo', 'titulo': 'Responde una pregunta de desarrollo',
                           'detalle': 'Explicar con tus palabras es la mejor forma de comprobar que lo sabes.',
                           'hecha': bool(intentos_hoy)})
        if not tareas:
            tareas.append({'tipo': 'test', 'titulo': 'Haz un test de 10 preguntas',
                           'detalle': 'Para empezar a medir tu progreso.', 'hecha': bool(tests_hoy)})
        return tareas[:5]

    def inicio(self):
        """Todo lo que necesita la pantalla de inicio: plan, próximo examen y cifras de cada herramienta."""
        codigo = self._codigo()
        resumenes = self.banco.resumenes(codigo)
        rutas = self._rutas_legibles()
        intentos = self.banco.intentos(codigo, 200)
        return {'plan': self.plan(), 'examen': self.proximo_examen(), 'resumen': self._resumen_progreso(),
                'herramientas': {
                    'resumenes': sum(1 for r in rutas if r in resumenes), 'resumenes_ia':
                        sum(1 for r in rutas if resumenes.get(r, {}).get('origen') == 'ia'),
                    'temas': len(rutas), 'desarrollos': len(intentos),
                    'nota_desarrollo': round(sum(i['nota'] for i in intentos) / len(intentos), 1) if intentos else None,
                    'conversaciones': len(self.banco.conversacion(codigo, 100))},
                'sugerencias': self.sugerencias_chat(), 'prediccion': self.prediccion(),
                'calendario': self.calendario(), 'nivel': self._nivel_compacto()}

    def _nivel_compacto(self):
        l = self.logros()
        return {k: l[k] for k in ('nivel', 'xp', 'xp_nivel', 'xp_siguiente', 'conseguidos', 'minutos_hoy')} | {
            'total_logros': len(l['logros']),
            'ultimo': next((x['nombre'] for x in reversed(l['logros']) if x['conseguido']), '')}

    # =============================================================== resúmenes

    def resumenes(self):
        """Lista de temas con el estado de su resumen."""
        codigo = self._codigo()
        hechos = self.banco.resumenes(codigo)
        temas = {t['ruta']: t for t in self._estadisticas_temas()}
        archivos = self.banco.archivos(codigo)
        lista = []
        for ruta in self._rutas_legibles():
            r = hechos.get(ruta)
            actual = bool(r) and r['firma'] == archivos.get(ruta, {}).get('firma')
            lista.append({'ruta': ruta, 'tema': nombre_tema(ruta), 'paginas': archivos.get(ruta, {}).get('paginas', 0),
                          'estado': (r['origen'] if actual else 'antiguo') if r else 'no',
                          'precision': temas.get(ruta, {}).get('precision')})
        return {'temas': self.con_grupos(lista), 'trabajando': bool(self.hilo_resumenes and self.hilo_resumenes.is_alive())}

    def resumen(self, ruta, rehacer=False, id_peticion=None, usar_ia=True, cancelar=None):
        """Resumen de un documento del Temario. Se guarda; con `rehacer` se vuelve a escribir."""
        from motor import ErrorProfesor
        codigo = self._codigo()
        if ruta not in self._rutas_legibles():
            raise ErrorProfesor('Ese documento no tiene texto legible.')
        firma = self.banco.archivos(codigo).get(ruta, {}).get('firma', '')
        guardado = self.banco.resumenes(codigo).get(ruta)
        con_ia = usar_ia and self._ia_lista()
        if guardado and guardado['firma'] == firma and not rehacer and (guardado['origen'] == 'ia' or not con_ia):
            return self._salida_resumen(ruta, guardado)
        if not con_ia:
            texto = self._resumen_sin_ia(ruta)
            self.banco.guardar_resumen(codigo, ruta, firma, texto, 'auto')
            return {**self._salida_resumen(ruta, self.banco.resumenes(codigo)[ruta]), 'sin_ia': True}
        cancelar = cancelar or self.tareas.nueva(id_peticion)
        try:
            texto = ia.resumir(self.ia_estado['modelo'], self.asignatura['nombre'], nombre_tema(ruta),
                               self._pasaje_completo(ruta, LIMITE_RESUMEN),
                               al_recibir=self._emisor_parcial(id_peticion, 'resumen', ruta=ruta), cancelar=cancelar)
        except ia.Cancelado:
            if guardado:
                return {**self._salida_resumen(ruta, guardado), 'cancelado': True}
            raise ErrorProfesor('Resumen cancelado.')
        except ia.ErrorIA as e:
            raise ErrorProfesor(str(e))
        finally:
            self.tareas.fin(id_peticion)
        self.banco.guardar_resumen(codigo, ruta, firma, texto, 'ia', self.ia_estado['modelo'])
        return self._salida_resumen(ruta, self.banco.resumenes(codigo)[ruta])

    def _salida_resumen(self, ruta, r):
        return {'ruta': ruta, 'tema': nombre_tema(ruta), 'texto': ia.texto_rico(r['texto']), 'origen': r['origen'],
                'modelo': r['modelo'], 'creado': r['creado'],
                'archivo': str(self._ruta_archivo(ruta))}

    def _ruta_archivo(self, ruta):
        from pathlib import Path
        return Path(self.asignatura['temario']) / ruta

    def _resumen_sin_ia(self, ruta):
        """Resumen extractivo: las frases más representativas del documento y su glosario."""
        trozos = self._fragmentos_de(ruta)
        generador = Generador(self.asignatura['codigo'], trozos)
        todas = [f for f, _, _ in generador.frases]
        # Importancia de una frase: palabras del documento que aparecen en muchas páginas.
        frecuencia = Counter()
        for f in trozos:
            frecuencia.update(set(terminos(f['texto'])))
        puntuadas = []
        for i, f in enumerate(todas):
            palabras = set(terminos(f))
            if len(palabras) < 4:
                continue
            puntos = sum(min(frecuencia[w], 6) for w in palabras) / (len(palabras) ** 0.6)
            puntuadas.append((puntos, i, f))
        elegidas = sorted(sorted(puntuadas, reverse=True)[:6], key=lambda x: x[1])
        conceptos = []
        vistos = set()
        for d in generador.definiciones:
            if clave(d['termino']) in vistos:
                continue
            vistos.add(clave(d['termino']))
            conceptos.append(d)
            if len(conceptos) == 7:
                break
        partes = ['## Ideas clave'] + [f'- {f}' for _, _, f in elegidas]
        if conceptos:
            partes += ['', '## Conceptos']
            for d in conceptos:
                definicion = d['resto'] if d['verbo'] in ('es', 'son') else f"{d['verbo']} {d['resto']}"
                partes.append(f"- **{_mayuscula(d['termino'])}**: {definicion.rstrip(' .')}.")
            partes += ['', '## Para el examen']
            for d in conceptos[:3]:
                verbo = 'son' if d['termino'].split()[0].lower().endswith('s') else 'es'
                partes.append(f"- ¿Qué {verbo} {d['articulo'].lower()} {d['termino']}?")
        if len(partes) <= 1:
            return 'No he encontrado frases claras en este documento para resumirlo sin IA.'
        return '\n'.join(partes)

    def resumir_todos(self):
        """Prepara en segundo plano los resúmenes con IA de todos los temas que no lo tienen."""
        from motor import ErrorProfesor
        if not self._ia_lista():
            raise ErrorProfesor(f'Para resumir todos los temas hace falta la IA ({ia.nombre_servidor()}).')
        if self.hilo_resumenes and self.hilo_resumenes.is_alive():
            return {'trabajando': True}
        codigo = self._codigo()
        hechos = self.banco.resumenes(codigo)
        archivos = self.banco.archivos(codigo)
        pendientes = [r for r in self._rutas_legibles()
                      if not (r in hechos and hechos[r]['origen'] == 'ia'
                              and hechos[r]['firma'] == archivos.get(r, {}).get('firma'))]
        if not pendientes:
            return {'trabajando': False, 'pendientes': 0}
        self.cancelar_resumenes = threading.Event()
        cancelar = self.cancelar_resumenes

        def trabajar():
            for i, ruta in enumerate(pendientes):
                if cancelar.is_set() or (self.asignatura or {}).get('codigo') != codigo:
                    break
                self.emitir({'evento': 'resumenes', 'estado': 'trabajando', 'ruta': ruta, 'tema': nombre_tema(ruta),
                             'hechos': i, 'total': len(pendientes)})
                try:
                    self.resumen(ruta, rehacer=True, id_peticion=f'todos:{ruta}', cancelar=cancelar)
                    self.emitir({'evento': 'resumen_listo', 'ruta': ruta})
                except Exception as e:  # noqa: BLE001 - un tema que falla no para a los demás
                    self.emitir({'evento': 'aviso', 'mensaje': f'No he podido resumir «{nombre_tema(ruta)}»: {e}'})
            self.emitir({'evento': 'resumenes', 'estado': 'terminado'})

        self.hilo_resumenes = threading.Thread(target=trabajar, daemon=True)
        self.hilo_resumenes.start()
        return {'trabajando': True, 'pendientes': len(pendientes)}

    def detener_resumenes(self):
        self.cancelar_resumenes.set()
        return {}

    # ============================================================== desarrollo

    def desarrollo(self):
        """Pantalla de preguntas de desarrollo: temas y últimos intentos."""
        codigo = self._codigo()
        intentos = [{**i, 'tema': nombre_tema(i['ruta'])} for i in self.banco.intentos(codigo, 12)]
        return {'temas': self.con_grupos(self._estadisticas_temas()), 'intentos': intentos}

    def _elegir_ruta_desarrollo(self, ruta):
        if ruta in self._rutas_legibles():
            return ruta
        temas = self._estadisticas_temas()
        if not temas:
            from motor import ErrorProfesor
            raise ErrorProfesor('No hay apuntes legibles en el Temario de esta asignatura.')
        hechas = Counter(a['ruta'] for a in self.banco.abiertas(self._codigo()))
        # Temas flojos y poco trabajados primero, con algo de azar.
        return min(temas, key=lambda t: ((t['precision'] if t['precision'] is not None else 60) / 100
                                         + hechas.get(t['ruta'], 0) * 0.15 + random.random() * 0.5))['ruta']

    def abierta(self, ruta=None, dificultad='Intermedia', usar_ia=True, id_peticion=None, nueva=False):
        """Una pregunta de desarrollo del tema `ruta` (o de uno flojo). Reutiliza las que aún no has hecho."""
        from motor import ErrorProfesor
        codigo = self._codigo()
        automatico = ruta not in self._rutas_legibles()
        ruta = self._elegir_ruta_desarrollo(ruta)
        hechas = self.banco.abiertas(codigo, [ruta])
        con_ia = usar_ia and self._ia_lista()
        if not nueva:
            sin_hacer = [a for a in hechas if not a['intentos'] and (a['origen'] == 'ia' or not con_ia)]
            if sin_hacer:
                return {'pregunta': self._salida_abierta(random.choice(sin_hacer))}
        if not con_ia:
            # Sin IA solo valen los temas con definiciones claras: en automático se prueban todos.
            candidatas = [ruta] + ([r for r in self._rutas_legibles() if r != ruta] if automatico else [])
            for r in candidatas:
                a = self._abierta_sin_ia(r, self.banco.abiertas(codigo, [r]))
                if a:
                    return {'pregunta': self._salida_abierta(a), 'sin_ia': True}
            raise ErrorProfesor('No encuentro conceptos claros en ' + ('tu temario' if automatico else 'este tema') +
                                ' para una pregunta sin IA. ' + (f'Enciende la IA ({ia.nombre_servidor()}).' if automatico
                                                                 else f'Elige otro tema o enciende la IA ({ia.nombre_servidor()}).'))
        pasaje, ubicacion = self._pasaje_alrededor(ruta)
        cancelar = self.tareas.nueva(id_peticion)
        try:
            q = None
            for _ in range(2):
                try:
                    q = ia.pregunta_abierta(self.ia_estado['modelo'], self.asignatura['nombre'], nombre_tema(ruta),
                                            pasaje, dificultad, evitar=[a['enunciado'] for a in hechas],
                                            al_recibir=self._emisor_parcial(id_peticion, 'abierta', 0.5, None),
                                            cancelar=cancelar)
                    break
                except ia.ErrorIA:
                    if cancelar.is_set():
                        raise
                    pasaje, ubicacion = self._pasaje_alrededor(ruta)
            if not q:
                raise ErrorProfesor('La IA no ha conseguido preparar una pregunta válida. Prueba otra vez.')
        except ia.Cancelado:
            raise ErrorProfesor('Pregunta cancelada.')
        except ia.ErrorIA as e:
            raise ErrorProfesor(str(e))
        finally:
            self.tareas.fin(id_peticion)
        a = {'id': identificador(codigo, 'abierta', q['enunciado'], ''), 'asignatura': codigo, 'ruta': ruta,
             'ubicacion': ubicacion, 'origen': 'ia', 'enunciado': q['enunciado'], 'puntos': q['puntos'],
             'respuesta': q['respuesta'], 'cita': self._cita(q['respuesta'], pasaje), 'modelo': self.ia_estado['modelo']}
        self.banco.guardar_abierta(a)
        return {'pregunta': self._salida_abierta(self.banco.abierta(a['id']))}

    @staticmethod
    def _cita(texto, pasaje):
        objetivo = set(terminos(texto))
        mejor, puntos = '', 0
        for f in frases(pasaje):
            p = len(objetivo & set(terminos(f)))
            if p > puntos:
                mejor, puntos = f, p
        return mejor

    def _abierta_sin_ia(self, ruta, hechas):
        """«¿Qué es X? Explícalo con tus palabras» a partir de una definición del apunte."""
        generador = Generador(self._codigo(), self._fragmentos_de(ruta))
        hechos = {clave(a['enunciado']) for a in hechas if a['intentos']}
        candidatas = [d for d in generador.definiciones if len(d['resto']) >= 40]
        random.shuffle(candidatas)
        for d in candidatas:
            plural = d['termino'].split()[0].lower().endswith('s')
            enunciado = (f"¿Qué {'son' if plural else 'es'} {d['articulo'].lower()} {d['termino']}? "
                         'Explícalo con tus palabras y pon un ejemplo si puedes.')
            if clave(enunciado) in hechos:
                continue
            definicion = (d['resto'] if d['verbo'] in ('es', 'son') else f"{d['verbo']} {d['resto']}").rstrip(' .')
            # Puntos clave: la definición troceada por comas o «y» en ideas cortas.
            trozos = [t.strip(' ,;') for t in re.split(r',\s+|;\s+|\s+y\s+(?=[a-záéíóú]{4,})', definicion)]
            puntos = [t for t in trozos if len(terminos(t)) >= 2][:4] or [definicion]
            a = {'id': identificador(self._codigo(), 'abierta', enunciado, ''), 'asignatura': self._codigo(),
                 'ruta': ruta, 'ubicacion': d['ubicacion'], 'origen': 'auto', 'enunciado': enunciado,
                 'puntos': [_mayuscula(p) for p in puntos],
                 'respuesta': f"{_mayuscula(d['articulo'].lower())} {d['termino']} {d['verbo']} {d['resto'].rstrip('.')}.",
                 'cita': d['frase']}
            self.banco.guardar_abierta(a)
            return self.banco.abierta(a['id'])
        return None

    def _salida_abierta(self, a, con_solucion=False):
        salida = {'id': a['id'], 'enunciado': a['enunciado'], 'tema': nombre_tema(a['ruta']), 'ruta': a['ruta'],
                  'ubicacion': a['ubicacion'], 'origen': a['origen'], 'n_puntos': len(a['puntos']),
                  'archivo': str(self._ruta_archivo(a['ruta']))}
        if con_solucion:
            salida.update({'puntos': a['puntos'], 'respuesta': a['respuesta'], 'cita': a['cita']})
        return salida

    def corregir(self, ident, respuesta, usar_ia=True, id_peticion=None):
        """Corrige una respuesta de desarrollo. Devuelve nota, puntos cubiertos, errores y la solución."""
        from motor import ErrorProfesor
        codigo = self._codigo()
        a = self.banco.abierta(ident)
        if not a or a['asignatura'] != codigo:
            raise ErrorProfesor('No encuentro esa pregunta.')
        respuesta = (respuesta or '').strip()
        rendirse = not respuesta
        if rendirse or len(terminos(respuesta)) < 3:
            correccion = {'nota': 0.0, 'puntos': [False] * len(a['puntos']), 'errores': [],
                          'comentario': 'No has escrito una respuesta. Lee la respuesta modelo y vuelve a intentarlo '
                                        'más tarde sin mirarla.' if rendirse else
                                        'Tu respuesta es demasiado corta para poder valorarla. Desarrolla la idea.',
                          'origen': 'auto'}
        elif usar_ia and self._ia_lista():
            pasaje, _ = self._pasaje_alrededor(a['ruta'], a['ubicacion'])
            cancelar = self.tareas.nueva(id_peticion)
            try:
                correccion = ia.corregir_abierta(self.ia_estado['modelo'], self.asignatura['nombre'],
                                                 nombre_tema(a['ruta']), pasaje, a['enunciado'], a['puntos'],
                                                 respuesta, al_recibir=self._emisor_parcial(id_peticion, 'corregir',
                                                                                           0.5, None),
                                                 cancelar=cancelar)
                correccion['origen'] = 'ia'
            except ia.Cancelado:
                raise ErrorProfesor('Corrección cancelada.')
            except ia.ErrorIA as e:
                raise ErrorProfesor(str(e))
            finally:
                self.tareas.fin(id_peticion)
        else:
            correccion = self._corregir_sin_ia(a, respuesta)
        if not rendirse:
            self.banco.guardar_intento(ident, codigo, respuesta, correccion['nota'], correccion)
        anteriores = [i['nota'] for i in self.banco.intentos(codigo, 200) if i['id'] == ident]
        return {'correccion': {**correccion, 'puntos_texto': a['puntos'], 'respuesta_modelo': a['respuesta'],
                               'cita': a['cita'], 'intentos': len(anteriores),
                               'mejor': max(anteriores) if anteriores else None},
                'pregunta': self._salida_abierta(a, con_solucion=True)}

    @staticmethod
    def _corregir_sin_ia(a, respuesta):
        """Corrección aproximada: cuántas palabras clave de cada punto aparecen en la respuesta."""
        def raiz(w):
            return w[:6] if len(w) > 6 else w
        escritas = {raiz(w) for w in terminos(respuesta)}
        cubiertos = []
        for p in a['puntos']:
            clave_p = {raiz(w) for w in terminos(p)}
            cubiertos.append(bool(clave_p) and len(clave_p & escritas) / len(clave_p) >= 0.5)
        nota = round(10 * sum(cubiertos) / max(1, len(cubiertos)), 1)
        if nota >= 8:
            comentario = 'Recoges las ideas clave del apunte. Compara con la respuesta modelo por si falta algún matiz.'
        elif nota >= 5:
            comentario = 'Vas bien, pero te faltan algunas ideas clave: repásalas en la respuesta modelo.'
        else:
            comentario = 'Faltan la mayoría de las ideas clave. Relee el apunte y vuelve a intentarlo.'
        return {'nota': nota, 'puntos': cubiertos, 'errores': [], 'comentario': comentario, 'origen': 'auto'}

    def descartar_abierta(self, ident):
        self.banco.descartar_abierta(ident)
        return {'mensaje': 'De acuerdo, no volveré a hacerte esa pregunta.'}

    # ================================================================ análisis

    def _datos_analisis(self):
        codigo = self._codigo()
        resumen = self._resumen_progreso()
        examen = self.proximo_examen()
        temas = self._estadisticas_temas()
        fallos = self.banco.guardadas(codigo)[:6]
        intentos = self.banco.intentos(codigo, 30)
        lineas = [f'Hoy es {date.today().strftime("%d/%m/%Y")}.']
        if examen:
            lineas.append(f"Próxima prueba: {examen['nombre']} {_dias(examen['dias'])} "
                          f"({examen['fecha']}, {examen['peso']:.0f} % de la nota).")
        else:
            lineas.append('No hay pruebas evaluables próximas en el calendario.')
        lineas.append(f"Precisión global en tests: {resumen['precision'] if resumen['precision'] is not None else 'sin datos'}"
                      f"{' %' if resumen['precision'] is not None else ''} en {resumen['respuestas']} respuestas y "
                      f"{resumen['tests']} tests. Racha: {resumen['racha']} días. Notas de los últimos tests: "
                      f"{', '.join(f'{n:.1f}' for n in resumen['ultimas_notas'][-6:]) or 'ninguna'}.")
        lineas.append(f"Tarjetas: {resumen['tarjetas_pendientes']} pendientes de repaso, "
                      f"{resumen['tarjetas_nuevas']} sin ver de {resumen['tarjetas_total']}.")
        if intentos:
            lineas.append(f'Preguntas de desarrollo: {len(intentos)} respondidas, nota media '
                          f"{sum(i['nota'] for i in intentos) / len(intentos):.1f}.")
        lineas.append('Temas (aciertos en tests):')
        for t in temas:
            lineas.append(f"- {t['tema']}: " + (f"{t['precision']} % de {t['total']} preguntas" if t['total']
                                                 else 'sin practicar'))
        if fallos:
            lineas.append('Algunas preguntas que ha fallado:')
            lineas += [f"- ({nombre_tema(p['ruta'])}) {p['enunciado'][:140]}" for p in fallos]
        return '\n'.join(lineas), resumen, examen, temas

    def analisis(self, usar_ia=True, id_peticion=None, rehacer=False, generar=True):
        """Diagnóstico del progreso y plan hasta el examen. Se guarda el último.
        Con `generar=False` solo devuelve el guardado (o un texto vacío si no hay ninguno)."""
        from motor import ErrorProfesor
        codigo = self._codigo()
        guardado, momento = self.banco.recuerdo(codigo, 'analisis')
        if guardado and not rehacer:
            return {**guardado, 'texto': ia.texto_rico(guardado.get('texto', '')), 'momento': momento}
        if not generar:
            return {'texto': '', 'origen': '', 'momento': 0}
        datos, resumen, examen, temas = self._datos_analisis()
        if usar_ia and self._ia_lista() and resumen['respuestas'] + resumen['tarjetas_total'] > 0:
            cancelar = self.tareas.nueva(id_peticion)
            try:
                texto = ia.analizar(self.ia_estado['modelo'], self.asignatura['nombre'], datos,
                                    al_recibir=self._emisor_parcial(id_peticion, 'analisis'), cancelar=cancelar)
                origen = 'ia'
            except ia.Cancelado:
                raise ErrorProfesor('Análisis cancelado.')
            except ia.ErrorIA as e:
                raise ErrorProfesor(str(e))
            finally:
                self.tareas.fin(id_peticion)
        else:
            texto, origen = self._analisis_sin_ia(resumen, examen, temas), 'auto'
        salida = {'texto': texto, 'origen': origen}
        self.banco.recordar(codigo, 'analisis', salida)
        return {**salida, 'momento': time.time()}

    def _analisis_sin_ia(self, resumen, examen, temas):
        partes = ['## Cómo vas']
        if resumen['respuestas'] == 0:
            partes.append('Aún no has hecho ningún test, así que no puedo medir tu nivel. Empieza por uno de '
                          '10 preguntas y repasa unas cuantas tarjetas.')
        else:
            p = resumen['precision']
            valoracion = ('muy bien' if p >= 80 else 'bien encaminado' if p >= 65 else
                          'a medio camino' if p >= 50 else 'todavía flojo')
            partes.append(f"Llevas un **{p} %** de aciertos en {resumen['respuestas']} preguntas: vas {valoracion}. "
                          f"Tu racha es de {resumen['racha']} {'día' if resumen['racha'] == 1 else 'días'}.")
        practicados = sorted((t for t in temas if t['total'] >= 2), key=lambda t: t['precision'])
        sin = [t for t in temas if t['total'] == 0]
        partes += ['', '## Puntos débiles']
        debiles = [f"- **{t['tema']}**: {t['precision']} % de aciertos." for t in practicados if t['precision'] < 70][:3]
        if sin:
            debiles.append(f"- **{len(sin)} temas sin practicar**, como «{sin[0]['tema']}».")
        partes += debiles or ['- No tienes temas flojos claros. ¡Sigue así!']
        partes += ['', '## Plan hasta el examen' if examen else '## Plan para esta semana']
        dias = examen['dias'] if examen else 7
        plan = []
        if resumen['tarjetas_pendientes']:
            plan.append(f"- **Hoy**: repasa tus {resumen['tarjetas_pendientes']} tarjetas pendientes.")
        for t in practicados[:2]:
            if t['precision'] < 70:
                plan.append(f"- **{'Hoy' if not plan else 'Mañana' if dias > 1 else 'Hoy'}**: test de «{t['tema']}» "
                            'y repasa los fallos.')
        if sin:
            plan.append(f"- **Esta semana**: lee el resumen de «{sin[0]['tema']}» y haz un test de ese tema.")
        if examen:
            plan.append(f"- **{'Mañana' if dias > 1 else 'Hoy'}**: simulacro de {examen['nombre']} en modo examen.")
        plan.append('- **Cada día**: responde una pregunta de desarrollo para practicar la redacción.')
        partes += plan[:5]
        return '\n'.join(partes)

    # ================================================================= trucos

    def truco(self, tarjeta, frente='', reverso='', id_peticion=None, rehacer=False):
        """Regla mnemotécnica o analogía para recordar una tarjeta (necesita IA). Se guarda por tarjeta."""
        from motor import ErrorProfesor
        codigo = self._codigo()
        guardado, _ = self.banco.recuerdo(codigo, 'truco:' + tarjeta)
        if guardado and not rehacer:
            return {'tarjeta': tarjeta, 'texto': ia.texto_rico(guardado)}
        if not self._ia_lista():
            raise ErrorProfesor('Los trucos para recordar los escribe la IA, y ahora no está disponible.')
        cancelar = self.tareas.nueva(id_peticion)
        try:
            texto = ia.truco(self.ia_estado['modelo'], self.asignatura['nombre'], frente, reverso,
                             al_recibir=self._emisor_parcial(id_peticion, 'truco', 0.25), cancelar=cancelar)
        except ia.Cancelado:
            raise ErrorProfesor('Truco cancelado.')
        except ia.ErrorIA as e:
            raise ErrorProfesor(str(e))
        finally:
            self.tareas.fin(id_peticion)
        self.banco.recordar(codigo, 'truco:' + tarjeta, texto)
        return {'tarjeta': tarjeta, 'texto': texto}

    # =========================================================== conversación

    def sugerencias_chat(self):
        """Preguntas de ejemplo para el chat, sacadas del propio temario: temas flojos y conceptos."""
        temas = self._estadisticas_temas()
        if not temas:
            return []
        flojos = sorted(temas, key=lambda t: (t['precision'] if t['precision'] is not None else 55, random.random()))
        sugerencias = [f"Explícame lo más importante de «{flojos[0]['tema']}»"]
        rutas = [t['ruta'] for t in flojos[:4]]
        definiciones = Generador(self._codigo(), [f for f in self.fragmentos if f['ruta'] in rutas]).definiciones
        vistos = set()
        for d in sorted(definiciones, key=lambda d: rutas.index(d['ruta'])):
            if clave(d['termino']) in vistos or len(sugerencias) >= 3:
                continue
            vistos.add(clave(d['termino']))
            verbo = 'son' if d['termino'].split()[0].lower().endswith('s') else 'es'
            sugerencias.append(f"¿Qué {verbo} {d['articulo'].lower()} {d['termino']}? Ponme un ejemplo")
        examen = self.proximo_examen()
        if len(flojos) > 1:
            sugerencias.append(f"¿Qué es lo que más cae en el examen de «{flojos[1]['tema']}»?" if not examen else
                               f"¿Qué debería repasar de «{flojos[1]['tema']}» para {examen['nombre']}?")
        return sugerencias[:4]

    def conversacion(self):
        return {'chat': [{**t, 'respuesta': ia.texto_rico(t['respuesta'])} for t in self.banco.conversacion(self._codigo())]}

    def nueva_conversacion(self):
        self.banco.archivar_conversacion(self._codigo())
        return {'chat': []}

    def cancelar(self, ident=None):
        self.tareas.cancelar(ident)
        return {}
