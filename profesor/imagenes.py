"""Imagen para acompañar cada pregunta, sacada del propio apunte.

Orden de preferencia:
  1. la figura (gráfica, diagrama, foto) de la misma página de la que sale la pregunta;
  2. la figura más cercana del mismo documento;
  3. la propia página o diapositiva renderizada.
Las figuras se recortan de la página renderizada (no se extrae la imagen suelta), porque en
las diapositivas suele haber ejes, flechas y textos dibujados encima de la imagen.
Las imágenes se guardan en caché (JPEG) en datos/profesor/imagenes.
"""
import hashlib
import io
import re
import threading
import zipfile
from collections import Counter
from pathlib import Path

LADO_MAXIMO = 900


def _pagina(ubicacion):
    m = re.search(r'(\d+)', ubicacion or '')
    return int(m.group(1)) - 1 if m else 0


class Imagenes:
    def __init__(self, carpeta):
        self.carpeta = Path(carpeta)
        self.carpeta.mkdir(parents=True, exist_ok=True)
        self.figuras = {}  # (archivo, firma) -> {pagina: caja}
        self.candado = threading.Lock()

    def para(self, archivo, ubicacion, firma=''):
        """Devuelve {'archivo': ruta_jpg, 'pie': texto} o None si no hay imagen posible."""
        archivo = Path(archivo)
        if not archivo.is_file():
            return None
        sufijo = archivo.suffix.lower()
        try:
            with self.candado:
                if sufijo == '.pdf':
                    return self._pdf(archivo, _pagina(ubicacion), firma)
                if sufijo in ('.pptx', '.docx'):
                    return self._office(archivo, _pagina(ubicacion), firma)
        except Exception:  # una imagen nunca debe impedir hacer la pregunta
            return None
        return None

    def _destino(self, archivo, clave, firma):
        nombre = hashlib.sha1(f'{archivo}|{clave}|{firma}'.encode('utf-8')).hexdigest()[:20] + '.jpg'
        return self.carpeta / nombre

    # ----------------------------------------------------------------- PDF

    def _cajas(self, doc, clave):
        """Zona de figuras de cada página: unión de las imágenes grandes que no son logos repetidos."""
        if clave in self.figuras:
            return self.figuras[clave]
        import pypdfium2.raw as pdfium_c
        objetos = []
        for i in range(len(doc)):
            pagina = doc[i]
            ancho, alto = pagina.get_size()
            for obj in pagina.get_objects(filter=(pdfium_c.FPDF_PAGEOBJ_IMAGE,), max_depth=3):
                izq, abajo, der, arriba = obj.get_bounds()
                objetos.append((i, (izq, abajo, der, arriba), obj.get_px_size(), ancho * alto))
        repetidas = Counter(tam for _, _, tam, _ in objetos)
        cajas = {}
        for i, (izq, abajo, der, arriba), tam, area in objetos:
            superficie = (der - izq) * (arriba - abajo)
            if repetidas[tam] >= 3 and superficie < 0.05 * area:
                continue  # logotipo o icono que se repite en todas las diapositivas
            if superficie < 0.06 * area or der - izq < 60 or arriba - abajo < 60:
                continue
            c = cajas.get(i)
            cajas[i] = (min(c[0], izq), min(c[1], abajo), max(c[2], der), max(c[3], arriba)) if c else \
                (izq, abajo, der, arriba)
        self.figuras[clave] = cajas
        return cajas

    def _pdf(self, archivo, pagina, firma):
        import pypdfium2 as pdfium
        doc = pdfium.PdfDocument(str(archivo))
        try:
            pagina = max(0, min(pagina, len(doc) - 1))
            cajas = self._cajas(doc, (str(archivo), firma))
            if cajas:
                elegida = min(cajas, key=lambda p: (abs(p - pagina), p))
                destino = self._destino(archivo, f'fig{elegida}', firma)
                pie = f'Figura del apunte · página {elegida + 1}'
                if not destino.exists():
                    self._recortar(doc[elegida], cajas[elegida], destino)
            else:
                destino = self._destino(archivo, f'pag{pagina}', firma)
                pie = f'Página {pagina + 1} del apunte'
                if not destino.exists():
                    self._recortar(doc[pagina], None, destino)
            return {'archivo': str(destino), 'pie': pie}
        finally:
            doc.close()

    def _recortar(self, pagina, caja, destino):
        escala = 2.0
        imagen = pagina.render(scale=escala).to_pil().convert('RGB')
        if caja:
            ancho, alto = pagina.get_size()
            margen = 18
            izq, abajo, der, arriba = caja
            recorte = (max(0, izq - margen) * escala, max(0, alto - arriba - margen) * escala,
                       min(ancho, der + margen) * escala, min(alto, alto - abajo + margen) * escala)
            imagen = imagen.crop(tuple(int(v) for v in recorte))
        imagen.thumbnail((LADO_MAXIMO, LADO_MAXIMO))
        imagen.save(destino, quality=86)

    # ------------------------------------------- páginas enteras para el lector

    @staticmethod
    def visual(archivo):
        """True si el documento se puede ver tal cual (páginas renderizadas) en el lector."""
        return Path(archivo).suffix.lower() == '.pdf'

    def hojas(self, archivo):
        """Tamaño (ancho, alto en puntos) de cada página, para colocarlas antes de tenerlas pintadas."""
        if not self.visual(archivo) or not Path(archivo).is_file():
            return []
        import pypdfium2 as pdfium
        with self.candado:
            doc = pdfium.PdfDocument(str(archivo))
            try:
                return [[round(v, 1) for v in doc.get_page_size(i)] for i in range(len(doc))]
            finally:
                doc.close()

    def pagina(self, archivo, indice, ancho, firma=''):
        """Una página del PDF pintada a `ancho` píxeles (JPEG en caché). Devuelve la ruta del JPEG."""
        archivo = Path(archivo)
        if not self.visual(archivo) or not archivo.is_file():
            raise ValueError('Este documento no se puede ver como página.')
        ancho = max(400, min(2400, int(ancho)))
        destino = self.carpeta / 'paginas' / (
            hashlib.sha1(f'{archivo}|{firma}|{indice}|{ancho}'.encode('utf-8')).hexdigest()[:24] + '.jpg')
        if destino.exists():
            return str(destino)
        import pypdfium2 as pdfium
        with self.candado:
            doc = pdfium.PdfDocument(str(archivo))
            try:
                if not 0 <= indice < len(doc):
                    raise ValueError('Esa página no existe.')
                pagina = doc[indice]
                imagen = pagina.render(scale=ancho / pagina.get_width()).to_pil().convert('RGB')
            finally:
                doc.close()
        destino.parent.mkdir(parents=True, exist_ok=True)
        temporal = destino.with_suffix('.tmp')
        imagen.save(temporal, format='JPEG', quality=90, subsampling=0)  # 4:4:4: letras nítidas
        temporal.replace(destino)
        self._podar(destino.parent)
        return str(destino)

    MAX_PAGINAS_REGION = 30  # una selección más larga ya no es «un trozo»

    def texto_region(self, archivo, indice, caja, hasta=None):
        """Texto que hay dentro de `caja` (izquierda, arriba, derecha, abajo; fracciones de 0 a 1 con el
        origen arriba a la izquierda, como se ve en pantalla) en la página `indice` del PDF.

        Con `hasta` > `indice` el recuadro abarca varias páginas: en la primera va desde `arriba` hasta el
        final de la página, las de en medio entran enteras y en la última va desde el principio hasta
        `abajo`, siempre entre `izquierda` y `derecha`."""
        archivo = Path(archivo)
        if not self.visual(archivo) or not archivo.is_file():
            raise ValueError('Este documento no es un PDF.')
        izq, arr, der, aba = (max(0.0, min(1.0, float(v))) for v in caja)
        izq, der = min(izq, der), max(izq, der)
        hasta = indice if hasta is None else max(indice, min(hasta, indice + self.MAX_PAGINAS_REGION - 1))
        if hasta == indice:
            arr, aba = min(arr, aba), max(arr, aba)
        import pypdfium2 as pdfium
        trozos = []
        with self.candado:
            doc = pdfium.PdfDocument(str(archivo))
            try:
                if not 0 <= indice < len(doc):
                    raise ValueError('Esa página no existe.')
                for i in range(indice, min(hasta, len(doc) - 1) + 1):
                    pagina = doc[i]
                    ancho, alto = pagina.get_size()
                    textos = pagina.get_textpage()
                    sup = arr if i == indice else 0.0
                    inf = aba if i == hasta else 1.0
                    # pdfium mide desde abajo a la izquierda
                    trozos.append(textos.get_text_bounded(left=izq * ancho, bottom=(1 - inf) * alto,
                                                          right=der * ancho, top=(1 - sup) * alto) or '')
            finally:
                doc.close()
        texto = '\n'.join(t.strip() for t in trozos if t and t.strip())
        texto = re.sub(r'[ \t]+', ' ', (texto or '').replace('\r', '\n'))
        texto = re.sub(r'(\w)-\n(\w)', r'\1\2', texto)  # palabras cortadas al final de la línea
        return re.sub(r'\n{3,}', '\n\n', texto).strip()

    def _podar(self, carpeta, maximo=400 * 1024 * 1024):
        """Mantiene la caché de páginas por debajo de `maximo` bytes (borra las más antiguas)."""
        self._pintadas = getattr(self, '_pintadas', 0) + 1
        if self._pintadas % 50:
            return
        ficheros = sorted(carpeta.glob('*.jpg'), key=lambda f: f.stat().st_mtime, reverse=True)
        total = 0
        for f in ficheros:
            total += f.stat().st_size
            if total > maximo:
                try:
                    f.unlink()
                except OSError:
                    pass

    # ------------------------------------------------------- Word y PowerPoint

    def _office(self, archivo, pagina, firma):
        with zipfile.ZipFile(archivo) as z:
            medios = [n for n in z.namelist() if re.search(r'/media/[^/]+\.(png|jpe?g|gif|bmp)$', n, re.I)]
            if archivo.suffix.lower() == '.pptx':
                # Imágenes de la diapositiva (o de la más cercana que tenga alguna).
                por_diapositiva = {}
                for n in z.namelist():
                    m = re.fullmatch(r'ppt/slides/_rels/slide(\d+)\.xml\.rels', n)
                    if m:
                        refs = re.findall(r'Target="\.\./media/([^"]+)"', z.read(n).decode('utf-8', 'ignore'))
                        if refs:
                            por_diapositiva[int(m.group(1)) - 1] = ['ppt/media/' + r for r in refs]
                if por_diapositiva:
                    cercana = min(por_diapositiva, key=lambda p: (abs(p - pagina), p))
                    medios = por_diapositiva[cercana]
            medios = [n for n in medios if z.getinfo(n).file_size > 15000]  # fuera iconos
            if not medios:
                return None
            nombre = max(medios, key=lambda n: z.getinfo(n).file_size)
            destino = self._destino(archivo, nombre, firma)
            if not destino.exists():
                from PIL import Image
                imagen = Image.open(io.BytesIO(z.read(nombre)))
                if imagen.mode in ('RGBA', 'LA', 'P'):
                    imagen = imagen.convert('RGBA')
                    fondo = Image.new('RGBA', imagen.size, (255, 255, 255, 255))
                    fondo.alpha_composite(imagen)
                    imagen = fondo
                imagen = imagen.convert('RGB')
                imagen.thumbnail((LADO_MAXIMO, LADO_MAXIMO))
                imagen.save(destino, quality=86)
            return {'archivo': str(destino), 'pie': 'Imagen del apunte'}
