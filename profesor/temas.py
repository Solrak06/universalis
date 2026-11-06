"""Temas del Temario: agrupa los apartados (los archivos) en temas con su título.

  '1_1 Concepto IA.pdf', '1_2 IA como tendencia.pdf'  ->  «Tema 1 · Introducción a la IA»
  '2_5. Aprendizaje supervisado.pdf'                  ->  «Tema 2 · Aprendizaje automático»

El número del tema sale del nombre del archivo (o de su carpeta). Los que no llevan número se
juntan por carpeta o por un prefijo común. Si la IA está disponible, en segundo plano pone nombre
a cada tema y coloca los apartados sueltos; su propuesta se guarda mientras no cambien los archivos.

Cada lista que la ventana enseña por temas lleva en cada elemento:
  grupo        «Tema 1 · Introducción a la IA» (el subtítulo)
  grupo_orden  orden del tema
  orden        orden del apartado en toda la lista
"""
import hashlib
import json
import re
import threading
from pathlib import PurePath

import ia
from generador import nombre_tema

_NUMERO = re.compile(r'^\s*(?:tema|unidad|ud|u|bloque|cap[ií]tulo|cap|t)?[\s._\-]*(\d{1,2})(?=[\s._\-]|$)', re.I)
_PREFIJO = re.compile(r'^([^\W\d_]+\d*)(?=[\s._\-])')
_PALABRAS_VACIAS = {'de', 'del', 'la', 'las', 'el', 'los', 'y', 'e', 'en', 'a', 'con', 'por', 'para', 'un', 'una'}

CLAVE_MEMORIA = 'temas_ia'
SIN_TEMA = 'Otros apartados'


def numero_tema(nombre):
    """'1_1 Concepto IA' -> 1, 'Tema1.1_ESQUEMA' -> 1, 'Gestión de memoria' -> None."""
    m = _NUMERO.match(nombre)
    if not m:
        return None
    n = int(m.group(1))
    return n if 0 < n < 60 else None


def clave_natural(ruta):
    """Orden «humano»: '2_4' antes que '10_1' y '7_19' antes que '7_20'."""
    return [(0, int(t), '') if t.isdigit() else (1, 0, t.lower()) for t in re.split(r'(\d+)', ruta)]


def firma(rutas):
    return hashlib.sha1('\n'.join(sorted(rutas)).encode('utf-8')).hexdigest()[:16]


def _prefijo_comun(nombres):
    """Palabras iniciales comunes: ['Funciones de Perdida y Metricas_Concepto', ...] -> 'Funciones de Perdida y Metricas'."""
    partes = [re.split(r'[\s_]+', n) for n in nombres]
    comun = []
    for palabras in zip(*partes):
        if len({p.lower() for p in palabras}) != 1:
            break
        comun.append(palabras[0])
    while comun and comun[-1].lower() in _PALABRAS_VACIAS:
        comun.pop()
    texto = ' '.join(comun).strip(' .-_')
    return texto if len(texto) >= 5 else ''


def limpiar_titulo(t):
    t = re.sub(r'\s+', ' ', str(t or '')).strip(' "\'«»*.:-')
    t = re.sub(r'^(tema|unidad|bloque)\s*\d+\s*[:.\-·]?\s*', '', t, flags=re.I).strip(' "\'«»*.:-')
    if len(t) > 52:
        t = t[:52].rsplit(' ', 1)[0]
    return t[:1].upper() + t[1:]


def agrupar(rutas, propuesta=None):
    """{ruta: {grupo, grupo_orden, orden, numero}}. `propuesta`: lo que devolvió la IA (ver `propuesta_ia`)."""
    propuesta = propuesta or {}
    titulos_ia = {int(k): v for k, v in (propuesta.get('titulos') or {}).items()}
    sueltos_ia = propuesta.get('sueltos') or {}       # ruta -> clave de grupo de la IA
    grupos_ia = propuesta.get('grupos') or []           # [{clave, titulo}] en orden

    claves = {}   # ruta -> ('n', numero) | ('ia', i) | ('c', carpeta) | ('p', prefijo) | ('o',)
    for ruta in rutas:
        p = PurePath(ruta)
        carpetas = p.parts[:-1]
        n = numero_tema(carpetas[0]) if carpetas else None
        if n is None:
            n = numero_tema(p.stem)
        if n is not None:
            claves[ruta] = ('n', n)
        elif ruta in sueltos_ia:  # la IA lo ha puesto en un tema numerado (int) o en uno suyo ('g2')
            destino = sueltos_ia[ruta]
            claves[ruta] = ('n', destino) if isinstance(destino, int) else ('ia', destino)
        elif carpetas:
            claves[ruta] = ('c', carpetas[0])
    # Prefijos compartidos por dos o más sueltos ('Ej1.Enero2013', 'Ej1.Julio2014').
    prefijos = {}
    for ruta in rutas:
        if ruta not in claves:
            m = _PREFIJO.match(PurePath(ruta).stem)
            if m:
                prefijos.setdefault(m.group(1).lower(), []).append(ruta)
    for pre, miembros in prefijos.items():
        if len(miembros) >= 2:
            for r in miembros:
                claves[r] = ('p', pre)
    for ruta in rutas:
        claves.setdefault(ruta, ('o',))

    miembros = {}
    for ruta, k in claves.items():
        miembros.setdefault(k, []).append(ruta)
    hay_numeros = any(k[0] == 'n' for k in miembros)
    orden_ia = {g['clave']: i for i, g in enumerate(grupos_ia)}

    def orden_grupo(k):
        if k[0] == 'n':
            return (0, k[1], '')
        if k[0] == 'ia':
            return (1, orden_ia.get(k[1], 99), '')
        if k[0] in ('c', 'p'):
            return (2, 0, k[1].lower())
        return (3, 0, '')

    salida = {}
    contador = 0
    sin_numero = 0
    for g, k in enumerate(sorted(miembros, key=orden_grupo), start=1):
        rutas_g = sorted(miembros[k], key=clave_natural)
        nombres = [nombre_tema(r) for r in rutas_g]
        if k[0] == 'n':
            titulo = titulos_ia.get(k[1]) or (nombres[0] if len(nombres) == 1 else _prefijo_comun(nombres))
            grupo = f'Tema {k[1]}' + (f' · {titulo}' if titulo else '')
            numero = k[1]
        elif k[0] == 'ia':
            titulo = next((x['titulo'] for x in grupos_ia if x['clave'] == k[1]), '') or SIN_TEMA
            numero = None
            if not hay_numeros:  # nada venía numerado: los temas de la IA se numeran en su orden
                sin_numero += 1
                numero = sin_numero
                grupo = f'Tema {numero} · {titulo}'
            else:
                grupo = titulo
        elif k[0] == 'c':
            grupo, numero = limpiar_titulo(k[1].replace('_', ' ')) or k[1], None
        elif k[0] == 'p':
            grupo, numero = _prefijo_comun(nombres) or limpiar_titulo(k[1]) or k[1], None
            grupo = grupo[:1].upper() + grupo[1:]
        else:
            grupo, numero = SIN_TEMA, None
        for r in rutas_g:
            contador += 1
            salida[r] = {'grupo': grupo, 'grupo_orden': g, 'orden': contador, 'numero': numero}
    return salida


# ---------------------------------------------------------------------- IA

ESQUEMA = {
    'type': 'object',
    'properties': {'temas': {'type': 'array', 'items': {
        'type': 'object',
        'properties': {'titulo': {'type': 'string'}, 'apartados': {'type': 'array', 'items': {'type': 'integer'}}},
        'required': ['titulo', 'apartados']}}},
    'required': ['temas']}

PEDIR = (
    'Estos son los apartados (archivos) del temario de la asignatura «{asignatura}». Agrúpalos en temas: '
    'los que dicen [Tema N] van juntos en su tema; los demás ponlos en el tema que les corresponda o en temas '
    'nuevos (por ejemplo «Exámenes de otros años» o «Prácticas»). Pon a cada tema un título corto de 2 a 6 '
    'palabras, en español, que diga de qué trata (sin «Tema N»). Escribe los títulos en minúscula salvo la primera letra, las siglas y los nombres propios. Ordena los temas como se estudiarían.\n\n'
    '{lista}\n\nResponde en JSON: {{"temas": [{{"titulo": "...", "apartados": [números]}}]}}')


def _linea(i, ruta, extracto):
    p = PurePath(ruta)
    n = numero_tema(p.parts[0]) if len(p.parts) > 1 else None
    n = n if n is not None else numero_tema(p.stem)
    texto = f'{i}. {nombre_tema(ruta)}' + (f'  [Tema {n}]' if n is not None else '')
    if n is None and extracto:
        texto += f'  — «{extracto}»'
    return texto


def propuesta_ia(modelo, asignatura, rutas, extractos, cancelar=None):
    """Pide a la IA títulos de los temas y dónde van los sueltos. Devuelve la `propuesta` de `agrupar`."""
    lista = '\n'.join(_linea(i, r, extractos.get(r, '')) for i, r in enumerate(rutas, start=1))
    texto = ia.chat(modelo, [{'role': 'user', 'content': PEDIR.format(asignatura=asignatura, lista=lista)}],
                    formato=ESQUEMA, opciones={'num_predict': 520, 'temperature': 0.2},
                    cancelar=cancelar, limite=420)
    datos = json.loads(texto)
    base = agrupar(rutas)
    titulos, sueltos, grupos = {}, {}, []
    for j, t in enumerate(datos.get('temas') or []):
        titulo = limpiar_titulo(t.get('titulo'))
        miembros = [rutas[i - 1] for i in t.get('apartados') or [] if isinstance(i, int) and 1 <= i <= len(rutas)]
        if not titulo or not miembros:
            continue
        numeros = [base[r]['numero'] for r in miembros if _numerado(r)]
        if numeros:
            n = max(set(numeros), key=numeros.count)
            titulos.setdefault(n, titulo)
            destino = n
        else:
            destino = f'g{j}'
            grupos.append({'clave': destino, 'titulo': titulo})
        for r in miembros:
            if not _numerado(r):
                sueltos.setdefault(r, destino)
    return {'titulos': {str(k): v for k, v in titulos.items()}, 'sueltos': sueltos, 'grupos': grupos}


def _numerado(ruta):
    p = PurePath(ruta)
    return numero_tema(p.stem) is not None or (len(p.parts) > 1 and numero_tema(p.parts[0]) is not None)


# ------------------------------------------------------------- mezcla de Motor

class Temas:
    """Mezcla para Motor (ver motor.py)."""

    def _iniciar_temas(self):
        self.hilo_temas = None
        self.cancelar_temas = threading.Event()
        self._cache_grupos = None   # (codigo, firma, grupos)

    def _rutas_temario(self):
        return sorted(self.banco.archivos(self.asignatura['codigo']))

    def grupos(self):
        """{ruta: {grupo, grupo_orden, orden, numero}} de todos los archivos del Temario."""
        codigo = self.asignatura['codigo']
        rutas = self._rutas_temario()
        f = firma(rutas)
        if self._cache_grupos and self._cache_grupos[:2] == (codigo, f):
            return self._cache_grupos[2]
        guardado, _ = self.banco.recuerdo(codigo, CLAVE_MEMORIA)
        propuesta = guardado['propuesta'] if guardado and guardado.get('firma') == f else None
        grupos = agrupar(rutas, propuesta)
        self._cache_grupos = (codigo, f, grupos)
        return grupos

    def con_grupos(self, lista):
        """Añade a cada elemento (con 'ruta') su tema y ordena la lista por temas."""
        if not self.asignatura:
            return lista
        g = self.grupos()
        salida = []
        for item in lista:
            info = g.get(item.get('ruta')) or {'grupo': SIN_TEMA, 'grupo_orden': 999, 'orden': 9999, 'numero': None}
            salida.append({**item, **info})
        salida.sort(key=lambda x: (x['grupo_orden'], x['orden']))
        return salida

    def temas_ia_pendiente(self):
        """True si la IA aún no ha organizado los temas de los archivos actuales."""
        codigo = self.asignatura['codigo']
        rutas = self._rutas_temario()
        if len(rutas) < 2:
            return False
        guardado, _ = self.banco.recuerdo(codigo, CLAVE_MEMORIA)
        return not (guardado and guardado.get('firma') == firma(rutas))

    def organizar_temas(self, forzar=False):
        """Lanza en segundo plano la IA para nombrar los temas. Avisa con el evento 'temas'."""
        if not self.asignatura or not (forzar or self.temas_ia_pendiente()):
            return {'trabajando': False}
        if self.hilo_temas and self.hilo_temas.is_alive():
            return {'trabajando': True}
        if not self._ia_lista():
            return {'trabajando': False}
        codigo = self.asignatura['codigo']
        nombre = self.asignatura['nombre']
        rutas = self._rutas_temario()
        extractos = {}
        for f in self.fragmentos:
            if f['ruta'] not in extractos and not _numerado(f['ruta']):
                extractos[f['ruta']] = re.sub(r'\s+', ' ', f['texto'])[:110]
        modelo = self.ia_estado.get('modelo')
        self.cancelar_temas = threading.Event()
        cancelar = self.cancelar_temas

        def trabajar():
            self.emitir({'evento': 'temas', 'estado': 'trabajando'})
            try:
                propuesta = propuesta_ia(modelo, nombre, rutas, extractos, cancelar)
                self.banco.recordar(codigo, CLAVE_MEMORIA, {'firma': firma(rutas), 'propuesta': propuesta,
                                                            'modelo': modelo})
                self._cache_grupos = None
                if (self.asignatura or {}).get('codigo') == codigo:
                    self.emitir({'evento': 'temas', 'estado': 'listo', 'grupos': self.grupos()})
            except ia.Cancelado:
                self.emitir({'evento': 'temas', 'estado': 'parado'})
            except Exception as e:  # noqa: BLE001 - sin IA los temas se quedan con el agrupado automático
                self.emitir({'evento': 'temas', 'estado': 'error', 'mensaje': str(e)})

        self.hilo_temas = threading.Thread(target=trabajar, daemon=True)
        self.hilo_temas.start()
        return {'trabajando': True}

    def detener_temas(self):
        self.cancelar_temas.set()
