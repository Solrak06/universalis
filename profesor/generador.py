"""Preguntas instantáneas sin IA, construidas a partir de las frases del Temario.

Todos llevan la frase original como cita:
  * definición -> «¿Qué concepto corresponde a esta descripción?»
  * inversa    -> «¿Qué describe mejor X?», con las definiciones de conceptos vecinos como distractores
  * falsa      -> «¿Cuál de estas afirmaciones es FALSA?»: tres frases del apunte y una con el concepto cambiado
  * completar  -> hueco en una frase con un término clave (los distractores concuerdan en género y número)
  * verdadero/falso -> la frase original o la misma con un concepto cambiado
"""
import hashlib
import math
import random
import re
from collections import Counter, defaultdict
from pathlib import PurePath

from texto import PALABRAS_VACIAS, clave, frases, sin_tildes

VERBOS = (r'se define como|se denomina|se conoce como|se llama|consiste en|consisten en|'
          r'se basa en|se basan en|se utiliza para|se utilizan para|se encarga de|se encargan de|'
          r'tiene como objetivo|tienen como objetivo|permite|permiten|representa|representan|'
          r'mide|miden|es|son|son también|es también')
DEFINICION = re.compile(
    r'^(?:(?:En|Para) [^,]{3,40}, )?(El|La|Los|Las|Un|Una|Unos|Unas)\s+'
    r'([A-Za-zÁÉÍÓÚÜÑáéíóúüñ][\wÁÉÍÓÚÜÑáéíóúüñ\-]*(?:\s+[\wÁÉÍÓÚÜÑáéíóúüñ\-()]+){0,5}?)\s+'
    r'(' + VERBOS + r')\s+(.{25,})$')
ARRANQUES_MALOS = {'resto', 'mismo', 'misma', 'siguiente', 'primero', 'primera', 'segundo', 'segunda',
                   'tercera', 'tercero', 'cuarta', 'objetivo', 'idea', 'problema', 'principal', 'mayoría',
                   'mayoria', 'último', 'ultimo', 'caso', 'resultado', 'uso', 'hecho', 'tema', 'figura',
                   'tabla', 'imagen', 'cual', 'alternativa', 'posibilidad', 'ventaja', 'desventaja',
                   'término', 'termino', 'propósito', 'proposito', 'poder', 'razón', 'razon', 'motivo',
                   'diferencia', 'clave', 'pregunta', 'respuesta', 'ejemplo', 'solución', 'solucion',
                   'opción', 'opcion', 'definición', 'definicion', 'paso', 'punto', 'parte', 'cosa', 'mismos', 'mismas', 'demás'}
# Lo que sigue a «es/son» y no es una definición («es igual», «es que…», «es muy…»).
RESTOS_MALOS = ('el mismo', 'la misma', 'lo mismo', 'igual', 'correct', 'confus', 'especialmente', 'algo ',
                'denominad', 'que ', 'muy ', 'más ', 'mas ', 'menos ', 'esencial', 'importante', 'posible',
                'necesari', 'fundamental', 'mejor', 'peor', 'así', 'asi ', 'decir', 'entrenad', '“', '"',
                'capaz', 'difícil', 'dificil', 'fácil', 'facil', 'útil', 'util', 'cierto', 'falso', 'mayor',
                'menor', 'suficiente', 'distint', 'diferente', 'similar', 'parecid', 'complej', 'sencill')
# Terminaciones típicas de sustantivos y adjetivos técnicos: buenos huecos para completar.
SUFIJOS_TECNICOS = ('ción', 'sión', 'dad', 'miento', 'ismo', 'aje', 'encia', 'ancia', 'ico', 'ica', 'icos',
                    'icas', 'ivo', 'iva', 'ivos', 'ivas', 'ura', 'ista', 'logía', 'metro', 'grama', 'ador',
                    'adora', 'ores', 'ciones', 'siones', 'dades', 'mientos')
# Terminaciones verbales (sin tildes): se evitan como hueco, dan preguntas de gramática.
SUFIJOS_VERBALES = ('ar', 'er', 'ir', 'ando', 'iendo', 'amos', 'emos', 'imos', 'aron', 'ieron', 'aba',
                    'aban', 'ian', 'ara', 'era', 'ira', 'aran', 'eran', 'iran', 'ase', 'iese', 'ados',
                    'adas', 'ado', 'ada', 'ido', 'ida', 'idos', 'idas', 'an', 'en', 'aria', 'eria', 'iria',
                    'nde', 'ere', 'ire')
# Para ocultar una sola palabra hace falta saber su clase: si la respuesta es un sustantivo y los
# distractores adjetivos o participios («puntos / vistos / ocultos»), la gramática delata la respuesta.
SUSTANTIVOS = ('ción', 'sión', 'ciones', 'siones', 'dad', 'dades', 'tad', 'miento', 'mientos', 'ismo', 'ismos',
               'aje', 'ajes', 'encia', 'encias', 'ancia', 'ancias', 'ura', 'uras', 'logía', 'logías', 'metro',
               'metros', 'grama', 'gramas', 'ador', 'adores', 'ista', 'istas', 'ización', 'izaciones')
ADJETIVOS = ('ico', 'ica', 'icos', 'icas', 'ivo', 'iva', 'ivos', 'ivas', 'able', 'ables', 'ible', 'ibles', 'al', 'ales')


def _clase(palabra):
    p = palabra.lower()
    if p.endswith(SUSTANTIVOS):
        return 'sustantivo'
    if p.endswith(ADJETIVOS):
        # «-ico/-ica» sin tilde suele ser un verbo («significa», «implica»); «práctica», «lógica» la llevan.
        if p.endswith(('ico', 'ica', 'icos', 'icas')) and not re.search('[áéíóú]', p):
            return ''
        return 'adjetivo'
    return ''


# Los nombres de archivo suelen venir sin tildes («Validacion Cruzada.pdf»): al mostrarlos se ponen
# las de las terminaciones que siempre la llevan y las de palabras habituales en los apuntes.
_TILDES = {'metrica': 'métrica', 'metricas': 'métricas', 'perdida': 'pérdida', 'perdidas': 'pérdidas',
           'analisis': 'análisis', 'logica': 'lógica', 'logicas': 'lógicas', 'tecnica': 'técnica',
           'tecnicas': 'técnicas', 'teoria': 'teoría', 'matematica': 'matemática', 'matematicas': 'matemáticas',
           'matematicos': 'matemáticos', 'estadistica': 'estadística', 'numero': 'número', 'numeros': 'números',
           'metodo': 'método', 'metodos': 'métodos', 'algebra': 'álgebra', 'calculo': 'cálculo',
           'introduccion': 'introducción', 'busqueda': 'búsqueda', 'arbol': 'árbol', 'arboles': 'árboles',
           'grafico': 'gráfico', 'graficos': 'gráficos', 'basico': 'básico', 'basicos': 'básicos',
           'caracteristicas': 'características', 'heuristica': 'heurística', 'heuristicas': 'heurísticas',
           'automatico': 'automático', 'automatica': 'automática', 'semantica': 'semántica',
           'sintactico': 'sintáctico', 'lexico': 'léxico', 'tema': 'tema', 'modulo': 'módulo', 'capitulo': 'capítulo',
           'genetico': 'genético', 'geneticos': 'genéticos', 'optimo': 'óptimo', 'optima': 'óptima',
           'dinamica': 'dinámica', 'dinamico': 'dinámico', 'electronica': 'electrónica', 'fisica': 'física',
           'quimica': 'química', 'economia': 'economía', 'informatica': 'informática', 'ingenieria': 'ingeniería',
           'practica': 'práctica', 'practicas': 'prácticas', 'teorico': 'teórico', 'teorica': 'teórica'}


def _con_tildes(palabra):
    minus = palabra.lower()
    nueva = _TILDES.get(minus)
    if nueva is None:
        nueva = re.sub(r'([cs])ion(es)?$', lambda m: m.group(1) + ('iones' if m.group(2) else 'ión'), minus) \
            if len(minus) > 5 else minus
    if nueva == minus:
        return palabra
    if palabra.isupper():
        return nueva.upper()
    return nueva[:1].upper() + nueva[1:] if palabra[:1].isupper() else nueva


def nombre_tema(ruta):
    """'2_5. Aprendizaje supervisado.pdf' -> 'Aprendizaje supervisado'."""
    nombre = PurePath(ruta).stem
    nombre = re.sub(r'^[\d\s._\-]+', '', nombre).replace('_', ' ').strip()
    if not nombre:
        return PurePath(ruta).stem
    nombre = re.sub(r'[A-Za-z]+', lambda m: _con_tildes(m.group()), nombre)
    return nombre[:1].upper() + nombre[1:]


def identificador(asignatura, tipo, enunciado, correcta):
    texto = '|'.join((asignatura, tipo, clave(enunciado), clave(correcta)))
    return hashlib.sha1(texto.encode('utf-8')).hexdigest()[:20]


def _mayuscula(texto):
    return texto[:1].upper() + texto[1:]


def _plural(termino):
    primera = termino.split()[0].lower()
    return primera.endswith('s') and not primera.endswith(('is', 'us'))


# Artículo -> (género, número). Sirve para que los distractores de un hueco concuerden con lo que lo
# precede: «la _____» no admite «sobreajuste», y eso delataba la respuesta.
ARTICULOS = {'el': ('m', 's'), 'del': ('m', 's'), 'al': ('m', 's'), 'un': ('m', 's'), 'la': ('f', 's'),
             'una': ('f', 's'), 'los': ('m', 'p'), 'unos': ('m', 'p'), 'las': ('f', 'p'), 'unas': ('f', 'p')}
FEMENINAS = ('ción', 'sión', 'dad', 'tad', 'tud', 'umbre', 'eza', 'ía', 'ura', 'encia', 'ancia', 'ciones',
             'siones', 'dades', 'a', 'as')
MASCULINAS = ('aje', 'ajes', 'miento', 'mientos', 'ismo', 'ismos', 'ema', 'emas', 'ama', 'or', 'ores', 'o', 'os')


def _genero(palabra):
    """Género probable de un sustantivo por su terminación ('' si no se sabe)."""
    p = palabra.lower().split()[0] if palabra.split() else ''
    for sufijo in sorted(FEMENINAS + MASCULINAS, key=len, reverse=True):
        if p.endswith(sufijo):
            return 'f' if sufijo in FEMENINAS else 'm'
    return ''


def _articulo_previo(frase, inicio):
    """(género, número) del artículo justo antes de la posición `inicio`, o None."""
    m = re.search(r'\b(el|la|los|las|un|una|unos|unas|del|al)\s+$', frase[:inicio], re.I)
    return ARTICULOS[m.group(1).lower()] if m else None


def _recortar(texto, maximo=150):
    if len(texto) <= maximo:
        return texto
    return texto[:maximo].rsplit(' ', 1)[0].rstrip(',;:') + '…'


class Generador:
    def __init__(self, asignatura, fragmentos, semilla=None):
        self.asignatura = asignatura
        self.rng = random.Random(semilla)
        self.frases = []        # (frase, ruta, ubicacion)
        for f in fragmentos:
            for frase in frases(f['texto']):
                self.frases.append((frase, f['ruta'], f['ubicacion']))
        self._glosario()
        self._vocabulario(fragmentos)

    # ----------------------------------------------------------- análisis

    def _glosario(self):
        self.definiciones = []
        vistos = set()
        for frase, ruta, ubicacion in self.frases:
            m = DEFINICION.match(frase)
            if not m:
                continue
            articulo, termino, verbo, resto = m.groups()
            termino = termino.strip(' ,:;')
            primera = termino.split()[0].lower()
            if (primera in PALABRAS_VACIAS or primera in ARRANQUES_MALOS or len(termino) < 4
                    or len(termino.split()) > 4
                    or any(w.lower() in ('esto', 'esta', 'este', 'eso', 'esa', 'ese') for w in termino.split())
                    or len(termino) > 55 or re.search(r'\d', termino)
                    or any(w.isupper() and len(w) > 1 and w.lower() in PALABRAS_VACIAS for w in termino.split())):
                continue
            resto = re.sub(r'^también\s+', '', resto)
            if resto.lower().startswith(RESTOS_MALOS) or len(resto) < 25:
                continue
            if clave(termino) in clave(resto):  # la definición revela el término
                continue
            k = clave(termino)
            if (k, clave(resto)) in vistos:
                continue
            vistos.add((k, clave(resto)))
            self.definiciones.append({'articulo': articulo, 'termino': termino, 'verbo': verbo,
                                      'resto': resto.rstrip('.'), 'frase': frase,
                                      'ruta': ruta, 'ubicacion': ubicacion})
        self.terminos = {}
        for d in self.definiciones:
            self.terminos.setdefault(clave(d['termino']), d)

    def _vocabulario(self, fragmentos):
        """Frecuencia documental de cada palabra con contenido, para elegir huecos y distractores."""
        df = Counter()
        self.forma = {}
        for f in fragmentos:
            vistas = set()
            for p in re.findall(r'[A-Za-zÁÉÍÓÚÜÑáéíóúüñ]{5,}', f['texto']):
                b = p.lower()
                if b in PALABRAS_VACIAS:
                    continue
                vistas.add(b)
                self.forma.setdefault(b, p if not p.isupper() else b)
            df.update(vistas)
        self.df = df
        self.n_fragmentos = max(1, len(fragmentos))
        # Con poco temario no se puede exigir que una palabra se repita en varias páginas.
        self.minimo_df = 2 if self.n_fragmentos >= 6 else 1
        # Palabras "de contenido": aparecen en el temario pero no en todas partes.
        limite = max(3, self.n_fragmentos * 0.25)
        self.vocabulario = [w for w, n in df.items() if 1 <= n <= limite and len(w) >= 6]

    # --------------------------------------------------------- distractores

    def _distractores_termino(self, correcto, ruta, dificultad, n=3, concordancia=None):
        """Otros conceptos del glosario. Con `concordancia` (género, número), solo los que encajan con
        el artículo que precede al hueco."""
        k = clave(correcto['termino'])
        plural = _plural(correcto['termino'])
        candidatos = []
        for kt, d in self.terminos.items():
            if kt == k or kt in k or k in kt:
                continue
            if concordancia and ARTICULOS.get(d['articulo'].lower()) != concordancia:
                continue
            puntos = self.rng.random()
            if _plural(d['termino']) == plural:
                puntos += 1.0
            if dificultad == 'Avanzada' and d['ruta'] == ruta:
                puntos += 1.5
            elif dificultad == 'Básica' and d['ruta'] != ruta:
                puntos += 0.8
            candidatos.append((puntos, d['termino']))
        candidatos.sort(reverse=True)
        elegidos, claves = [], {k}
        for _, t in candidatos:
            if clave(t) not in claves:
                elegidos.append(t)
                claves.add(clave(t))
            if len(elegidos) == n:
                break
        return elegidos

    def _distractores_palabra(self, palabra, frase, dificultad, n=3, concordancia=None):
        base = palabra.lower()
        sufijo = sin_tildes(base)[-3:]
        en_frase = {w.lower() for w in re.findall(r'\w+', frase)}
        candidatos = []
        for w in self.vocabulario:
            if w == base or w in en_frase or sin_tildes(w)[:5] == sin_tildes(base)[:5]:
                continue
            if self.df[w] < self.minimo_df or (sin_tildes(w).endswith(SUFIJOS_VERBALES) and not w.endswith(SUFIJOS_TECNICOS)):
                continue
            if _plural(w) != _plural(base):
                continue
            if concordancia and _genero(w) != concordancia[0]:
                continue
            if not concordancia and _genero(base) and _genero(w) != _genero(base):
                continue  # sin artículo delante, concuerdan con la propia respuesta («poder predictivo»)
            if _clase(w) != _clase(base):
                continue
            puntos = self.rng.random()
            if sin_tildes(w)[-3:] == sufijo:
                puntos += 2.0 if dificultad != 'Básica' else 1.0
            elif sin_tildes(w)[-1:] == sin_tildes(base)[-1:]:
                puntos += 0.7
            puntos -= abs(len(w) - len(base)) * 0.08
            candidatos.append((puntos, w))
        candidatos.sort(reverse=True)
        elegidos = [self._igualar_forma(w, palabra) for _, w in candidatos[:n]]
        return elegidos if len(elegidos) == n else []

    def _igualar_forma(self, palabra, modelo):
        forma = self.forma.get(palabra, palabra)
        if modelo[:1].isupper():
            return _mayuscula(forma)
        return forma.lower() if forma[:1].isupper() and not forma.isupper() else forma

    # ------------------------------------------------------------ preguntas

    def _crear(self, tipo, enunciado, opciones, correcta, explicacion, cita, ruta, ubicacion, dificultad,
               formato='definicion', motivos=None):
        """`motivos`: por qué es correcta o incorrecta cada opción (en el orden de `opciones`)."""
        orden = list(range(len(opciones)))
        if tipo != 'vf':
            self.rng.shuffle(orden)
        mezcladas = [opciones[i] for i in orden]
        motivos = [motivos[i] for i in orden] if motivos and len(motivos) == len(opciones) else []
        return {'id': identificador(self.asignatura, tipo, enunciado, opciones[correcta]),
                'asignatura': self.asignatura, 'ruta': ruta, 'ubicacion': ubicacion,
                'origen': 'auto', 'tipo': tipo, 'dificultad': dificultad,
                'enunciado': enunciado, 'opciones': mezcladas, 'respuesta': orden.index(correcta),
                'explicacion': explicacion, 'cita': cita, 'formato': 'vf' if tipo == 'vf' else formato,
                'motivos': motivos}

    def _que_es(self, termino):
        """«El sobreajuste es un fenómeno en el que…»: la definición del temario de otro concepto, para
        explicar por qué esa opción no era la buena."""
        d = self.terminos.get(clave(termino))
        if not d:
            return f'«{_mayuscula(termino)}» es otro concepto del temario.'
        return f'«{_mayuscula(termino)}» {d["verbo"]} {_recortar(d["resto"], 110)}.'

    def definicion(self, d, dificultad):
        if d['verbo'] in ('se denomina', 'se conoce como', 'se llama'):
            return None  # «X se denomina Y»: lo que sigue es un nombre, no una descripción
        distractores = self._distractores_termino(d, d['ruta'], dificultad)
        if len(distractores) < 3:
            return None
        descripcion = f"{'Son' if _plural(d['termino']) else 'Es'} " if d['verbo'] in ('es', 'son') else ''
        descripcion += (d['verbo'] + ' ' if d['verbo'] not in ('es', 'son') else '') + d['resto']
        enunciado = ('¿Qué concepto corresponde a esta descripción?\n«' + _mayuscula(descripcion.strip()) + '.»')
        opciones = [_mayuscula(d['termino'])] + [_mayuscula(t) for t in distractores]
        explicacion = (f"La descripción corresponde a {d['articulo'].lower()} {d['termino']}. "
                       f"Las otras opciones son conceptos del temario que se definen de otra forma.")
        motivos = [f"Correcta: así define el temario {d['articulo'].lower()} {d['termino']}."] + \
            [self._que_es(t) for t in distractores]
        return self._crear('opcion', enunciado, opciones, 0, explicacion, d['frase'],
                           d['ruta'], d['ubicacion'], dificultad, motivos=motivos)

    @staticmethod
    def _descripcion(d):
        texto = d['resto'] if d['verbo'] in ('es', 'son') else d['verbo'] + ' ' + d['resto']
        return _mayuscula(_recortar(texto.strip()))

    def inversa(self, d, dificultad):
        """«¿Qué describe mejor X?»: hay que reconocer su definición entre las de conceptos vecinos."""
        nombres = ('se denomina', 'se conoce como', 'se llama')
        if d['verbo'] in nombres:
            return None
        vecinos = [self.terminos.get(clave(t)) for t in self._distractores_termino(d, d['ruta'], dificultad, n=6)]
        vecinos = [v for v in vecinos if v and v['verbo'] not in nombres][:3]
        if len(vecinos) < 3:
            return None
        opciones = [self._descripcion(d)] + [self._descripcion(v) for v in vecinos]
        if len({clave(o) for o in opciones}) < 4:
            return None
        enunciado = f"¿Qué describe mejor {d['articulo'].lower()} {d['termino']}?"
        explicacion = (f"{_mayuscula(d['articulo'].lower())} {d['termino']} {d['verbo']} {d['resto']}. "
                       f"Las otras descripciones corresponden a: {', '.join(v['termino'] for v in vecinos)}.")
        motivos = [f"Correcta: es la definición de {d['termino']} del temario."] + \
            [f"Esa descripción es la de «{v['termino']}», no la de {d['termino']}." for v in vecinos]
        return self._crear('opcion', enunciado, opciones, 0, explicacion, d['frase'], d['ruta'], d['ubicacion'],
                           dificultad, motivos=motivos)

    def falsa(self, d, dificultad):
        """«¿Cuál es falsa?»: la frase de `d` con su concepto cambiado entre tres definiciones verdaderas."""
        if len(d['frase']) > 230:
            return None
        otro = self._distractores_termino(d, d['ruta'], dificultad, n=1,
                                          concordancia=ARTICULOS.get(d['articulo'].lower()))
        if not otro:
            return None
        cambiada = re.compile(re.escape(d['termino']), re.I).sub(otro[0], d['frase'], count=1)
        if cambiada == d['frase']:
            return None
        ciertas = [x for x in self.definiciones
                   if clave(x['termino']) not in (clave(d['termino']), clave(otro[0])) and len(x['frase']) <= 230]
        self.rng.shuffle(ciertas)
        ciertas.sort(key=lambda x: x['ruta'] != d['ruta'])  # las del mismo tema primero
        elegidas, vistos = [], set()
        for x in ciertas:
            if clave(x['termino']) not in vistos:
                elegidas.append(x['frase'])
                vistos.add(clave(x['termino']))
            if len(elegidas) == 3:
                break
        if len(elegidas) < 3:
            return None
        explicacion = (f'Es falsa porque eso no lo hace {otro[0]}, sino {d["termino"]}. '
                       'Las otras tres afirmaciones las recoge el temario.')
        motivos = [f'Es la falsa: eso no es {otro[0]}, sino {d["termino"]}.'] + \
            ['Es cierta: el temario lo dice así.'] * 3
        return self._crear('opcion', '¿Cuál de estas afirmaciones es FALSA?', [cambiada] + elegidas, 0, explicacion,
                           d['frase'], d['ruta'], d['ubicacion'], dificultad, 'falsa', motivos)

    def completar(self, frase, ruta, ubicacion, dificultad):
        # 1) Si la frase menciona un término compuesto del glosario, se oculta ese término.
        for kt, d in self.terminos.items():
            if len(d['termino'].split()) < 2:
                continue
            patron = re.compile(r'\b' + re.escape(d['termino']) + r'\b', re.I)
            m = patron.search(frase)
            if m and d['frase'] != frase and len(self.terminos) >= 4:
                distractores = self._distractores_termino(d, ruta, dificultad,
                                                          concordancia=_articulo_previo(frase, m.start()))
                if len(distractores) == 3:
                    hueco = frase[:m.start()] + '_____' + frase[m.end():]
                    motivos = [f'Correcta: la frase habla de {d["termino"]}.'] + [self._que_es(t) for t in distractores]
                    return self._crear('opcion', 'Completa la frase:\n«' + hueco + '»',
                                       [m.group()] + distractores, 0,
                                       f'El concepto que falta es «{m.group()}».',
                                       frase, ruta, ubicacion, dificultad, 'completar', motivos)
        # 2) Si no, se oculta la palabra más característica de la frase.
        candidatas = []
        for m in re.finditer(r'[A-Za-zÁÉÍÓÚÜÑáéíóúüñ]{6,}', frase):
            w = m.group().lower()
            if w in PALABRAS_VACIAS or w not in self.df or m.start() == 0:
                continue
            n = self.df[w]
            if n > max(3, self.n_fragmentos * 0.25) or n < self.minimo_df:
                continue  # demasiado común, o tan rara que no es un concepto del tema
            if not _clase(w):
                continue  # solo sustantivos y adjetivos técnicos, cuya clase se reconoce
            puntos = math.log(self.n_fragmentos / n) + self.rng.random() * 0.3
            if w.endswith(SUFIJOS_TECNICOS):
                puntos += 1.5
            candidatas.append((puntos, m))
        for _, m in sorted(candidatas, key=lambda c: c[0], reverse=True)[:3]:
            concordancia = _articulo_previo(frase, m.start())
            if concordancia and _genero(m.group()) != concordancia[0]:
                continue  # el género de la palabra no se puede deducir: los distractores podrían no encajar
            distractores = self._distractores_palabra(m.group(), frase, dificultad, concordancia=concordancia)
            if len(distractores) == 3:
                hueco = frase[:m.start()] + '_____' + frase[m.end():]
                return self._crear('opcion', 'Completa la frase:\n«' + hueco + '»',
                                   [m.group()] + distractores, 0,
                                   f'La palabra que falta es «{m.group()}».',
                                   frase, ruta, ubicacion, dificultad, 'completar')
        return None

    def verdadero_falso(self, d, dificultad):
        verdadera = self.rng.random() < 0.5
        if verdadera:
            afirmacion = d['frase']
            explicacion = 'Es verdadero: el temario lo afirma tal cual.'
        else:
            otros = self._distractores_termino(d, d['ruta'], dificultad, n=1,
                                               concordancia=ARTICULOS.get(d['articulo'].lower()))
            if not otros:
                return None
            patron = re.compile(re.escape(d['termino']), re.I)
            afirmacion = patron.sub(otros[0], d['frase'], count=1)
            if afirmacion == d['frase']:
                return None
            explicacion = f'Es falso: lo que se describe no es {otros[0]}, sino {d["termino"]}.'
        return self._crear('vf', '¿Verdadero o falso?\n«' + afirmacion + '»', ['Verdadero', 'Falso'],
                           0 if verdadera else 1, explicacion, d['frase'], d['ruta'], d['ubicacion'],
                           dificultad)

    # Peso de cada forma de preguntar una definición según la dificultad.
    PESOS = {'Básica': {'definicion': 3, 'inversa': 2, 'falsa': 1, 'completar': 1},
             'Intermedia': {'definicion': 2, 'inversa': 2, 'falsa': 2, 'completar': 1},
             'Avanzada': {'definicion': 1, 'inversa': 2, 'falsa': 3, 'completar': 1}}

    def _pregunta_de_definicion(self, d, dificultad, formatos):
        pesos = dict(self.PESOS.get(dificultad, self.PESOS['Intermedia']))
        if formatos:
            pesos = {k: v for k, v in pesos.items()
                     if (k if k != 'inversa' else 'definicion') in formatos} or pesos
        orden = []
        while pesos:  # orden aleatorio ponderado, sin repetir
            k = self.rng.choices(list(pesos), weights=list(pesos.values()))[0]
            orden.append(k)
            del pesos[k]
        for k in orden:
            if k == 'completar':
                p = self.completar(d['frase'], d['ruta'], d['ubicacion'], dificultad)
            else:
                p = getattr(self, k)(d, dificultad)
            if p:
                yield p

    def generar(self, cantidad, dificultad='Intermedia', tipo='variado', excluir=(), formatos=None):
        """Hasta `cantidad` preguntas distintas, repartidas entre temas y tipos. `formatos` limita las
        formas de preguntar (definicion, completar, falsa); los formatos que solo sabe hacer la IA se ignoran."""
        formatos = {f for f in (formatos or []) if f in ('definicion', 'completar', 'falsa')}
        excluir = set(excluir)
        resultado, enunciados = [], set()

        def anadir(p):
            if p and p['id'] not in excluir and clave(p['enunciado']) not in enunciados:
                resultado.append(p)
                enunciados.add(clave(p['enunciado']))
                return True
            return False

        # Turnos por tema para que un PDF largo no acapare el test.
        por_tema = defaultdict(list)
        for d in self.definiciones:
            por_tema[d['ruta']].append(('def', d))
        for frase, ruta, ubicacion in self.frases:
            por_tema[ruta].append(('frase', (frase, ruta, ubicacion)))
        for lista in por_tema.values():
            # Las definiciones dan las mejores preguntas: se sacan primero (pop toma del final).
            self.rng.shuffle(lista)
            lista.sort(key=lambda e: e[0] == 'def')
        temas = list(por_tema)
        self.rng.shuffle(temas)
        intentos = 0
        while len(resultado) < cantidad and any(por_tema.values()) and intentos < cantidad * 40:
            for tema in temas:
                if len(resultado) >= cantidad or not por_tema[tema]:
                    continue
                intentos += 1
                clase, dato = por_tema[tema].pop()
                quiero_vf = tipo == 'vf' or (tipo == 'variado' and self.rng.random() < 0.3)
                if tipo == 'vf' and clase != 'def':
                    continue
                if clase == 'def':
                    if quiero_vf:
                        anadir(self.verdadero_falso(dato, dificultad))
                    else:
                        any(anadir(p) for p in self._pregunta_de_definicion(dato, dificultad, formatos))
                elif not formatos or 'completar' in formatos:
                    anadir(self.completar(*dato, dificultad))
        return resultado
