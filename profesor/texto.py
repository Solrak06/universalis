"""Lectura del Temario: extrae el texto de cada documento y lo limpia.

Formatos: PDF, DOCX, PPTX, TXT, MD y TEX. Para PDF se usa pypdfium2 (rápido y
respeta los espacios entre palabras) y, si no está, pypdf.
"""
import re
import unicodedata
import zipfile
from pathlib import Path
from xml.etree import ElementTree as ET

FORMATOS = {'.pdf', '.docx', '.pptx', '.txt', '.md', '.tex'}

PALABRAS_VACIAS = set('''
a al algo algunos ante antes aquel aquella aquellas aquellos aqui así asi aun aunque bajo bien
cada casi como con contra cual cuales cualquier cuando cuanto de del desde donde dos durante e el
ella ellas ello ellos en entre era eran es esa esas ese eso esos esta estas este esto estos está
están estar etc fue fueron ha han hasta hay la las le les lo los mas más me mediante mi mientras
mismo mucho muchos muy nada ni no nos nuestra nuestro o otra otras otro otros para pero poco por
porque puede pueden que qué quien se sea sean según segun ser si sí sido siempre sin sino sobre
son su sus tal también tambien tan tanto te tiene tienen todo todos tras tu un una unas uno unos
usted y ya podría podrían debería deberían tendría tendrían sería serían habría cuya cuyo dicho dicha ver vez veces hacer hace hacen tener debe deben forma tipo tipos
caso casos ejemplo ejemplos parte partes manera modo través traves dentro fuera respecto general
alguna alguno algunas primera primero segunda segundo tercera tercero cuarta próxima próximo proxima
proximo última último ultima ultimo cierta cierto ciertas ciertos cualquiera demás demas mismas mismos
'''.split())


# ---------------------------------------------------------------- extracción

def _pdf(ruta):
    try:
        import pypdfium2 as pdfium
        doc = pdfium.PdfDocument(str(ruta))
        try:
            return [(f'página {i + 1}', doc[i].get_textpage().get_text_range())
                    for i in range(len(doc))]
        finally:
            doc.close()
    except ImportError:
        pass
    import logging
    logging.getLogger('pypdf').setLevel(logging.ERROR)
    from pypdf import PdfReader
    return [(f'página {i + 1}', p.extract_text() or '')
            for i, p in enumerate(PdfReader(str(ruta)).pages)]


def _office(ruta):
    with zipfile.ZipFile(ruta) as zipf:
        if ruta.suffix.lower() == '.docx':
            nombres = ['word/document.xml']
        else:
            nombres = sorted((n for n in zipf.namelist()
                              if re.fullmatch(r'ppt/slides/slide\d+\.xml', n)),
                             key=lambda n: int(re.search(r'(\d+)', n.rsplit('/', 1)[1]).group(1)))
        paginas = []
        for i, nombre in enumerate(nombres):
            raiz = ET.fromstring(zipf.read(nombre))
            # Cada párrafo (<w:p> o <a:p>) en su propia línea.
            parrafos = []
            for p in raiz.iter():
                if p.tag.endswith('}p'):
                    texto = ''.join(t.text or '' for t in p.iter() if t.tag.endswith('}t'))
                    if texto.strip():
                        parrafos.append(texto)
            ubicacion = 'documento' if ruta.suffix.lower() == '.docx' else f'diapositiva {i + 1}'
            paginas.append((ubicacion, '\n'.join(parrafos)))
        return paginas


def _plano(ruta):
    crudo = ruta.read_bytes()
    for codificacion in ('utf-8-sig', 'cp1252', 'latin-1'):
        try:
            texto = crudo.decode(codificacion)
            break
        except UnicodeDecodeError:
            continue
    if ruta.suffix.lower() == '.tex':
        texto = _limpiar_latex(texto)
    return [('texto', texto)]


def _limpiar_latex(texto):
    texto = re.sub(r'(?<!\\)%.*', '', texto)
    texto = re.sub(r'\\begin\{(?:figure|table|tikzpicture|lstlisting|verbatim)\}.*?'
                   r'\\end\{(?:figure|table|tikzpicture|lstlisting|verbatim)\}', '', texto, flags=re.S)
    texto = re.sub(r'\\(?:section|subsection|subsubsection|chapter|paragraph)\*?\{([^}]*)\}', r'\n\1.\n', texto)
    texto = re.sub(r'\\item\s*', '\n• ', texto)
    texto = re.sub(r'\\(?:textbf|textit|emph|underline|texttt)\{([^}]*)\}', r'\1', texto)
    texto = re.sub(r'\\[a-zA-Z]+\*?(?:\[[^\]]*\])?(?:\{[^}]*\})?', ' ', texto)
    return texto.replace('{', '').replace('}', '').replace('~', ' ')


def extraer(ruta):
    """Devuelve [(ubicación, texto)] de un documento. Lanza excepción si no puede."""
    ruta = Path(ruta)
    sufijo = ruta.suffix.lower()
    if sufijo == '.pdf':
        return _pdf(ruta)
    if sufijo in ('.docx', '.pptx'):
        return _office(ruta)
    return _plano(ruta)


# ------------------------------------------------------------------ limpieza

_VINETA = re.compile(r'^\s*(?:[•●▪■◦○\-–—*►➢✓]|\d{1,2}[.)])\s+')


def limpiar(texto):
    """Une las líneas cortadas de las diapositivas y separa las viñetas en párrafos."""
    texto = unicodedata.normalize('NFC', texto).replace('\r\n', '\n').replace('\r', '\n')
    texto = texto.replace('\x02', '')
    texto = texto.replace('\ufffe', '').replace('\u00ad', '').replace('\x0c', '\n')
    lineas = [l.strip() for l in texto.split('\n')]
    parrafos, actual = [], ''
    titulo_posible = False  # `actual` es una única línea corta que no viene de una viñeta
    for linea in lineas:
        if not linea or re.fullmatch(r'\d{1,3}', linea):  # vacía o número de página
            if actual:
                parrafos.append(actual)
                actual = ''
            continue
        if _VINETA.match(linea):
            if actual:
                parrafos.append(actual)
            actual = _VINETA.sub('', linea)
            titulo_posible = False
            continue
        if actual.endswith('-') and not actual.endswith(' -'):
            actual = actual[:-1] + linea  # palabra partida con guion
            titulo_posible = False
        elif actual and linea[:1].isupper() and (actual[-1] in '.:;!?' or (titulo_posible and len(actual) < 60)):
            parrafos.append(actual)  # fin de frase, o un título corto de una sola línea
            actual = linea
            titulo_posible = True
        else:
            titulo_posible = not actual
            actual = (actual + ' ' + linea).strip()
    if actual:
        parrafos.append(actual)
    return '\n'.join(re.sub(r'\s+', ' ', p).strip() for p in parrafos if p.strip())


def frases(texto):
    """Frases legibles de un texto ya limpio (una por párrafo o por punto)."""
    resultado = []
    for parrafo in texto.split('\n'):
        for trozo in re.split(r'(?<=[.!?])\s+(?=[A-ZÁÉÍÓÚÑ¿¡])', parrafo):
            trozo = trozo.strip()
            # Solo frases completas: empiezan en mayúscula y no acaban a medias.
            if es_legible(trozo) and (trozo[0].isupper() or trozo[0] in '¿¡«"“') \
                    and not trozo.endswith((',', ' y', ' o', ' de', ' que', ' la', ' el')):
                resultado.append(trozo)
    return resultado


def es_legible(frase, minimo=45, maximo=420):
    if not minimo <= len(frase) <= maximo:
        return False
    if 'http' in frase or 'www.' in frase:
        return False
    letras = sum(c.isalpha() for c in frase)
    if letras / len(frase) < 0.72:
        return False
    palabras = frase.split()
    if len(palabras) < 7:
        return False
    # Texto extraído con las palabras pegadas: demasiadas palabras larguísimas.
    if sum(len(p) > 22 for p in palabras) > 0:
        return False
    # Fórmulas o etiquetas sueltas de un diagrama, no una frase.
    if any(c in frase for c in '=≈≤≥∑∫→←') or any(0x1D400 <= ord(c) <= 0x1D7FF for c in frase):
        return False
    if sum(p[:1].isupper() for p in palabras[1:]) / len(palabras) > 0.3:
        return False
    return True


# ------------------------------------------------------------- vocabulario

def sin_tildes(texto):
    return ''.join(c for c in unicodedata.normalize('NFD', texto) if unicodedata.category(c) != 'Mn')


def clave(texto):
    """Forma canónica para comparar textos: minúsculas, sin tildes ni signos."""
    return re.sub(r'\W+', ' ', sin_tildes(texto).casefold()).strip()


def palabras(texto):
    return re.findall(r'[A-Za-zÁÉÍÓÚÜÑáéíóúüñ]+', texto)


def terminos(texto):
    """Palabras con contenido (sin palabras vacías), en minúsculas."""
    return [p.lower() for p in palabras(texto) if len(p) > 2 and p.lower() not in PALABRAS_VACIAS]


_SUFIJOS = ('aciones', 'amientos', 'imientos', 'amiento', 'imiento', 'idades', 'mente', 'acion', 'ables', 'ibles',
            'idad', 'able', 'ible', 'ivos', 'ivas', 'ivo', 'iva', 'ces', 'es', 'os', 'as', 's', 'o', 'a', 'e')


def raiz(palabra):
    """Raíz aproximada para buscar sin depender de tildes, plurales ni derivados: «Validaciones»,
    «validación» y «validar» comparten raíz."""
    p = sin_tildes(palabra.lower())
    for s in _SUFIJOS:
        if p.endswith(s) and len(p) - len(s) >= 4:
            p = p[:-len(s)]
            break
    return p[:8]


def raices(texto):
    return [raiz(t) for t in terminos(texto)]


def es_prosa(texto):
    """False para una página que es casi todo números o fórmulas (una tabla, los ejes de una gráfica)."""
    return sum(c.isalpha() for c in texto) / max(1, len(texto)) >= 0.6
