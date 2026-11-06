"""Más herramientas del profesor: buscador y glosario de los apuntes, mapas mentales, calendario hasta
el examen, nota estimada, tiempo de estudio (pomodoro), niveles y logros, y exportación (dossier HTML
para imprimir y tarjetas para Anki).

`Herramientas` es otra clase base de `Motor` (como `Estudio`): usa banco, asignatura, fragmentos...
"""
import csv
import html
import math
import random
import re
import time
from collections import Counter
from datetime import date, datetime, timedelta
from pathlib import Path

import ia
from generador import Generador, _mayuscula, nombre_tema
from texto import clave, frases, raiz, terminos

DIAS_SEMANA = ['lun', 'mar', 'mié', 'jue', 'vie', 'sáb', 'dom']

LOGROS = [
    # id, nombre, descripción, objetivo
    ('primer_test', 'Primer paso', 'Termina tu primer test', 1),
    ('racha3', 'Constancia', 'Estudia 3 días seguidos', 3),
    ('racha7', 'Imparable', 'Estudia 7 días seguidos', 7),
    ('preguntas100', 'Centenario', 'Responde 100 preguntas', 100),
    ('preguntas500', 'Veterano', 'Responde 500 preguntas', 500),
    ('precision80', 'Francotirador', '80 % de aciertos con al menos 50 respuestas', 80),
    ('tarjetas50', 'Memoria de elefante', 'Repasa 50 tarjetas', 50),
    ('dominadas20', 'Lo tengo grabado', 'Domina 20 tarjetas (repasadas con éxito varias veces)', 20),
    ('desarrollo10', 'Pluma de oro', 'Responde 10 preguntas de desarrollo', 10),
    ('matricula', 'Matrícula', 'Saca un 9 o más en una pregunta de desarrollo', 1),
    ('simulacro', 'Listo para el examen', 'Aprueba un simulacro de examen', 1),
    ('bibliotecario', 'Bibliotecario', 'Ten resumidos todos los temas', 1),
    ('maraton', 'Maratón', 'Estudia 5 horas con el temporizador', 300),
    ('sin_fallos', 'Sin cuentas pendientes', 'Vacía «Mis fallos» después de haber fallado', 1),
]


def _xp_para(nivel):
    """Experiencia total necesaria para llegar a `nivel` (1, 2, 3...)."""
    return 120 * (nivel - 1) * nivel // 2


class Herramientas:
    """Mezcla para Motor (ver motor.py)."""

    def _iniciar_herramientas(self):
        self._cache_glosario = None   # (codigo, nº de fragmentos, lista)

    # ============================================================ buscador

    def glosario(self):
        """Todos los conceptos definidos en el temario, por orden alfabético."""
        codigo = self._codigo()
        resumenes = self.banco.resumenes(codigo)
        firma = (codigo, len(self.fragmentos), sum(int(r['creado']) for r in resumenes.values()))
        if self._cache_glosario and self._cache_glosario[0] == firma:
            return {'glosario': self._cache_glosario[1]}
        vistos, lista = set(), []
        # 1) Los conceptos de los resúmenes («- **Término**: definición»), los de la IA primero.
        for ruta, r in sorted(resumenes.items(), key=lambda x: x[1]['origen'] != 'ia'):
            if ruta not in self._rutas_legibles():
                continue
            for m in re.finditer(r'^- \*\*(.+?)\*\*:?\s*(.+)$', ia.texto_rico(r['texto']), re.M):
                termino, definicion = m.group(1).strip(' :'), m.group(2).strip()
                if clave(termino) in vistos or len(termino) > 60 or len(definicion) < 10:
                    continue
                vistos.add(clave(termino))
                lista.append({'termino': _mayuscula(termino), 'definicion': _mayuscula(definicion.rstrip(' .')) + '.',
                              'tema': nombre_tema(ruta), 'ruta': ruta, 'ubicacion': 'resumen',
                              'archivo': str(self._ruta_archivo(ruta))})
        # 2) Las definiciones que se detectan en el propio apunte.
        for d in Generador(codigo, self.fragmentos).definiciones:
            k = clave(d['termino'])
            if k in vistos:
                continue
            vistos.add(k)
            definicion = d['resto'] if d['verbo'] in ('es', 'son') else f"{d['verbo']} {d['resto']}"
            lista.append({'termino': _mayuscula(d['termino']), 'definicion': _mayuscula(definicion.rstrip(' .')) + '.',
                          'tema': nombre_tema(d['ruta']), 'ruta': d['ruta'], 'ubicacion': d['ubicacion'],
                          'archivo': str(self._ruta_archivo(d['ruta']))})
        lista.sort(key=lambda t: clave(t['termino']))
        self._cache_glosario = (firma, lista)
        return {'glosario': lista}

    def buscar(self, texto, limite=10):
        """Busca en los apuntes: fragmentos más relevantes (con las palabras resaltadas) y conceptos."""
        texto = (texto or '').strip()
        if len(texto) < 2:
            return {'consulta': texto, 'resultados': [], 'conceptos': []}
        buscadas = {raiz(t) for t in terminos(texto)} or {raiz(texto)}
        resultados, vistos = [], set()
        for p in self._buscar(texto, limite=limite * 3, recortar=False):
            # Solo fragmentos con una frase de verdad que contenga lo buscado (no portadas ni rótulos).
            mejor, puntos = '', 0
            for f in frases(p['texto']):
                n = sum(1 for t in terminos(f) if raiz(t) in buscadas)
                if n > puntos:
                    mejor, puntos = f, n
            if not mejor or clave(mejor) in vistos:
                continue
            vistos.add(clave(mejor))
            extracto = ' '.join(mejor.split())
            extracto = extracto if len(extracto) <= 320 else extracto[:317] + '…'
            # Palabras buscadas en negrita (la ventana dibuja **…**).
            extracto = re.sub(r'[A-Za-zÁÉÍÓÚÜÑáéíóúüñ]{3,}',
                              lambda m: f'**{m.group()}**' if raiz(m.group()) in buscadas
                              and m.group().lower() not in ('las', 'los', 'del', 'que') else m.group(), extracto)
            titulo = p['titulo']
            resultados.append({'tema': titulo.split(' · ')[0], 'ubicacion': titulo.split(' · ')[-1],
                               'extracto': extracto, 'archivo': p['archivo'], 'ruta': p['ruta']})
            if len(resultados) == limite:
                break
        consulta = clave(texto)
        conceptos = [g for g in self.glosario()['glosario']
                     if consulta in clave(g['termino']) or (len(consulta) > 3 and consulta in clave(g['definicion']))]
        return {'consulta': texto, 'resultados': resultados, 'conceptos': conceptos[:8]}

    # ========================================================= mapa mental

    def mapa(self, ruta, rehacer=False, usar_ia=True, id_peticion=None):
        """Mapa mental de un tema: {centro, ramas:[{titulo, ideas}]}. Se guarda por documento."""
        from motor import ErrorProfesor
        codigo = self._codigo()
        if ruta not in self._rutas_legibles():
            raise ErrorProfesor('Ese documento no tiene texto legible.')
        firma = self.banco.archivos(codigo).get(ruta, {}).get('firma', '')
        guardado, _ = self.banco.recuerdo(codigo, 'mapa:' + ruta)
        con_ia = usar_ia and self._ia_lista()
        if guardado and guardado.get('firma') == firma and not rehacer and (guardado['origen'] == 'ia' or not con_ia):
            return {**guardado, 'ruta': ruta, 'tema': nombre_tema(ruta)}
        if con_ia:
            cancelar = self.tareas.nueva(id_peticion)
            try:
                m = ia.mapa(self.ia_estado['modelo'], self.asignatura['nombre'], nombre_tema(ruta),
                            self._pasaje_completo(ruta, 4200),
                            al_recibir=self._emisor_parcial(id_peticion, 'mapa', 0.5, None), cancelar=cancelar)
                origen = 'ia'
            except ia.Cancelado:
                raise ErrorProfesor('Mapa cancelado.')
            except ia.ErrorIA as e:
                raise ErrorProfesor(str(e))
            finally:
                self.tareas.fin(id_peticion)
        else:
            m, origen = self._mapa_sin_ia(ruta), 'auto'
        salida = {**m, 'origen': origen, 'firma': firma}
        self.banco.recordar(codigo, 'mapa:' + ruta, salida)
        return {**salida, 'ruta': ruta, 'tema': nombre_tema(ruta)}

    def _mapa_sin_ia(self, ruta):
        """Ramas = conceptos definidos en el apunte; si hay pocos, las palabras más características."""
        trozos = self._fragmentos_de(ruta)
        generador = Generador(self._codigo(), trozos)
        ramas, vistos = [], set()
        for d in generador.definiciones:
            k = clave(d['termino'])
            if k in vistos:
                continue
            vistos.add(k)
            trozos_def = [t.strip(' ,;.') for t in re.split(r',\s+|;\s+|\s+y\s+(?=[a-záéíóú]{4,})', d['resto'])]
            ideas = [ia._corto(t, 8) for t in trozos_def if len(terminos(t)) >= 2][:3] or [ia._corto(d['resto'], 8)]
            ramas.append({'titulo': ia._corto(_mayuscula(d['termino']), 5), 'ideas': ideas})
            if len(ramas) == 6:
                break
        if len(ramas) < 3:
            # Palabras que se repiten en el documento, con la frase donde mejor aparecen.
            cuenta = Counter(t for f in trozos for t in set(terminos(f['texto'])) if len(t) > 5)
            todas = [f for t in trozos for f in frases(t['texto'])]
            for palabra, _ in cuenta.most_common(30):
                if len(ramas) == 5 or palabra[:6] in vistos:
                    continue
                ejemplos = [f for f in todas if palabra in f.lower()][:2]
                if not ejemplos:
                    continue
                vistos.add(palabra[:6])
                ramas.append({'titulo': _mayuscula(palabra), 'ideas': [ia._corto(e, 8) for e in ejemplos]})
        return {'centro': ia._corto(nombre_tema(ruta), 5), 'ramas': ramas}

    # ======================================================= examen y plan

    def prediccion(self):
        """Nota estimada si el examen fuera hoy, a partir de tus aciertos en cada tema (ponderados por
        páginas). Los temas sin practicar cuentan como un 30 %. None si aún no hay respuestas."""
        temas = self._estadisticas_temas()
        archivos = self.banco.archivos(self._codigo())
        if not temas or not any(t['total'] for t in temas):
            return None
        suma = peso = cubierto = 0.0
        for t in temas:
            w = max(1, archivos.get(t['ruta'], {}).get('paginas', 1))
            # Con pocas respuestas la precisión se suaviza hacia el 50 %.
            p = (t['aciertos'] + 1) / (t['total'] + 2) if t['total'] else 0.3
            suma += w * p
            peso += w
            if t['total'] >= 3:
                cubierto += w
        nota = round(10 * suma / peso, 1)
        cubierto = round(100 * cubierto / peso)
        flojos = [t['tema'] for t in sorted(temas, key=lambda t: (t['aciertos'] + 1) / (t['total'] + 2)
                                            if t['total'] else 0.3)[:2]]
        if cubierto < 40:
            consejo = 'Aún has practicado poco temario: la estimación es orientativa.'
        elif nota >= 7:
            consejo = 'Vas muy bien. Mantén los repasos para no olvidar.'
        elif nota >= 5:
            consejo = f'Aprobarías justo. Lo que más sube la nota: «{flojos[0]}».'
        else:
            consejo = f'Ahora mismo no llegas. Empieza por «{flojos[0]}» y «{flojos[-1]}».'
        return {'nota': nota, 'cubierto': cubierto, 'consejo': consejo}

    def calendario(self):
        """Reparto de los temas por días hasta el próximo examen (los flojos primero, repasos al final)."""
        examen = self.proximo_examen()
        temas = self._estadisticas_temas()
        if not examen or not temas:
            return None
        hoy = date.today()
        dias = examen['dias']
        orden = sorted(temas, key=lambda t: ((t['aciertos'] + 1) / (t['total'] + 2) if t['total'] else 0.25,
                                             t['tema']))
        salida = []
        if dias == 0:
            return {'examen': examen, 'dias': [{'fecha': hoy.isoformat(), 'etiqueta': 'Hoy', 'tipo': 'examen',
                                                'tareas': ['Repaso ligero de tus fallos y de los conceptos clave',
                                                           f"Examen: {examen['nombre']}"], 'hoy': True}]}
        estudio = list(range(dias))  # días antes del examen (0 = hoy)
        ultimo = estudio[-1] if dias >= 2 else None
        huecos = [d for d in estudio if d != ultimo] or [0]
        por_dia = max(1, math.ceil(len(orden) / len(huecos)))
        por_dia = min(por_dia, 4)
        for i, d in enumerate(estudio):
            dia = hoy + timedelta(days=d)
            etiqueta = 'Hoy' if d == 0 else 'Mañana' if d == 1 else f'{DIAS_SEMANA[dia.weekday()]} {dia.day}'
            if d == ultimo:
                tareas = [f"Simulacro de {examen['nombre']}", 'Repasa «Mis fallos» y tus tarjetas pendientes']
                tipo = 'simulacro'
            else:
                k = huecos.index(d)
                bloque = orden[k * por_dia:(k + 1) * por_dia]
                if not bloque:  # sobran días: segunda vuelta a los más flojos
                    bloque = orden[(k * por_dia) % len(orden):(k * por_dia) % len(orden) + 1]
                tareas = [f"{'Repasa' if t['total'] else 'Estudia'} «{t['tema']}»"
                          + (f" ({t['precision']} %)" if t['precision'] is not None else '') for t in bloque]  # «67 %» sin partir
                tipo = 'temas'
            salida.append({'fecha': dia.isoformat(), 'etiqueta': etiqueta, 'tipo': tipo, 'tareas': tareas,
                           'hoy': d == 0, 'rutas': [] if tipo != 'temas' else [t['ruta'] for t in bloque]})
            if len(salida) == 14:
                break
        dia_examen = hoy + timedelta(days=dias)
        if dias <= 14:
            salida.append({'fecha': dia_examen.isoformat(), 'tipo': 'examen', 'hoy': False, 'rutas': [],
                           'etiqueta': f'{DIAS_SEMANA[dia_examen.weekday()]} {dia_examen.day}',
                           'tareas': [f"Examen: {examen['nombre']} ({examen['peso']:.0f} %)"]})
        return {'examen': examen, 'dias': salida}

    # ============================================================== temario

    CLAVE_LEIDOS = 'temario_leidos'
    CLAVE_POSICIONES = 'temario_posiciones'  # {ruta: fracción leída}
    CLAVE_ULTIMO = 'temario_ultimo'
    TAM_LECTURA = ('*', 'lectura_tam')  # tamaño de letra del lector, igual en todas las asignaturas

    def _leidos(self):
        valor, _ = self.banco.recuerdo(self._codigo(), self.CLAVE_LEIDOS)
        return set(valor or [])

    def _posiciones(self):
        valor, _ = self.banco.recuerdo(self._codigo(), self.CLAVE_POSICIONES)
        return dict(valor or {})

    def _tam_lectura(self):
        valor, _ = self.banco.recuerdo(*self.TAM_LECTURA)
        return int(valor) if valor else 17

    def temario(self):
        """Los documentos del temario, por temas, con si ya los has leído entero alguna vez."""
        leidos = self._leidos()
        posiciones = self._posiciones()
        paginas = Counter(f['ruta'] for f in self.fragmentos)
        lista = []
        for rel, info in sorted(self.banco.archivos(self._codigo()).items()):
            visual = self.imagenes.visual(rel) and info['paginas'] > 0
            lista.append({'ruta': rel, 'tema': nombre_tema(rel), 'paginas': info['paginas'],
                          'legibles': paginas.get(rel, 0), 'visual': visual, 'leido': rel in leidos,
                          'error': info['error'], 'posicion': posiciones.get(rel, 0)})
        lista = self.con_grupos(lista)
        ultimo, _ = self.banco.recuerdo(self._codigo(), self.CLAVE_ULTIMO)
        return {'temas': lista, 'leidos': sum(1 for t in lista if t['leido']),
                'total': sum(1 for t in lista if t['legibles'] or t['visual']), 'ultimo': ultimo or '',
                'tam': self._tam_lectura()}

    def leer(self, ruta):
        """Un documento para leerlo dentro del profesor: su texto página a página y, si es un PDF, el
        tamaño de cada página para verlo tal cual (las páginas se piden luego una a una con `pagina`)."""
        paginas = [{'ubicacion': f['ubicacion'], 'texto': f['texto']} for f in self.fragmentos if f['ruta'] == ruta]
        palabras = sum(len(p['texto'].split()) for p in paginas)
        grupo = self.con_grupos([{'ruta': ruta}])[0]
        archivo = Path(self.asignatura['temario']) / ruta
        try:
            hojas = self.imagenes.hojas(archivo)
        except Exception:  # PDF dañado: queda el texto
            hojas = []
        return {'ruta': ruta, 'tema': nombre_tema(ruta), 'grupo': grupo.get('grupo', ''),
                'archivo': str(archivo), 'paginas': paginas, 'hojas': hojas, 'visual': bool(hojas),
                'palabras': palabras, 'minutos': max(1, round(palabras / 200)) if palabras else 0,
                'leido': ruta in self._leidos(), 'posicion': self._posiciones().get(ruta, 0)}

    def pagina(self, ruta, indice, ancho):
        """Una página del documento pintada como imagen (para verlo como en el PDF)."""
        archivo = Path(self.asignatura['temario']) / ruta
        try:
            jpg = self.imagenes.pagina(archivo, indice, ancho, getattr(self, 'firmas', {}).get(ruta, ''))
        except Exception as e:
            raise ValueError(f'No he podido pintar la página {indice + 1}: {e}') from e
        return {'ruta': ruta, 'indice': indice, 'ancho': ancho, 'imagen': jpg}

    def posicion_lectura(self, ruta, posicion):
        """Guarda por dónde vas leyendo un documento (0 = principio, 1 = final)."""
        posiciones = self._posiciones()
        posiciones[ruta] = round(max(0.0, min(1.0, posicion)), 3)
        self.banco.recordar(self._codigo(), self.CLAVE_POSICIONES, posiciones)
        self.banco.recordar(self._codigo(), self.CLAVE_ULTIMO, ruta)
        return {'ruta': ruta, 'posicion': posiciones[ruta]}

    def texto_region(self, ruta, indice, caja, hasta=None):
        """Texto de un recuadro del PDF (`caja`: izquierda, arriba, derecha, abajo en fracciones de la
        página), para preguntar al profesor por un trozo seleccionado. Con `hasta`, el recuadro baja
        desde la página `indice` hasta esa: ver `Imagenes.texto_region`."""
        archivo = Path(self.asignatura['temario']) / ruta
        hasta = indice if hasta is None else max(indice, hasta)
        try:
            texto = self.imagenes.texto_region(archivo, indice, caja, hasta)
        except Exception as e:
            raise ValueError(f'No he podido leer el texto de esa zona: {e}') from e
        ubicacion = f'página {indice + 1}' if hasta == indice else f'páginas {indice + 1}–{hasta + 1}'
        return {'ruta': ruta, 'indice': indice, 'texto': texto, 'ubicacion': ubicacion}

    def tam_lectura(self, tam):
        tam = max(14, min(34, int(tam)))
        self.banco.recordar(*self.TAM_LECTURA, tam)
        return {'tam': tam}

    def marcar_leido(self, ruta, leido=True):
        leidos = self._leidos()
        if leido:
            leidos.add(ruta)
        else:
            leidos.discard(ruta)
        self.banco.recordar(self._codigo(), self.CLAVE_LEIDOS, sorted(leidos))
        return {'ruta': ruta, 'leido': leido, 'leidos': len(leidos)}

    # ===================================================== tiempo y logros

    def registrar_estudio(self, minutos, tipo='pomodoro'):
        minutos = max(1, min(180, int(minutos)))
        self.banco.registrar_estudio(self._codigo(), minutos, tipo)
        return {'logros': self.logros()}

    def _hoy_epoch(self):
        return datetime.combine(date.today(), datetime.min.time()).timestamp()

    def logros(self):
        """Nivel, experiencia y logros de la asignatura."""
        codigo = self._codigo()
        b = self.banco
        resumen = self._resumen_progreso()
        respuestas = resumen['respuestas']
        aciertos = resumen['aciertos']
        repasos = b.contar('SELECT COUNT(*) FROM repasos WHERE asignatura=?', (codigo,))
        dominadas = b.contar('SELECT COUNT(*) FROM tarjetas WHERE asignatura=? AND caja>=3', (codigo,))
        intentos = b.contar('SELECT COUNT(*) FROM intentos WHERE asignatura=?', (codigo,))
        matricula = b.contar('SELECT COUNT(*) FROM intentos WHERE asignatura=? AND nota>=9', (codigo,))
        minutos = b.minutos_estudio(codigo)
        simulacros = sum(1 for s in b.sesiones_desde(codigo, 0) if s['config'].get('simulacro') and s['nota'] >= 5)
        resumidos = len([r for r in self._rutas_legibles() if b.resumenes(codigo).get(r, {}).get('origen') == 'ia'])
        temas = len(self._rutas_legibles())
        fallos_alguna_vez = b.contar('SELECT COUNT(*) FROM preguntas WHERE asignatura=? AND fallos>0', (codigo,))
        maxima_racha = self._racha_maxima()
        valores = {
            'primer_test': resumen['tests'], 'racha3': maxima_racha, 'racha7': maxima_racha,
            'preguntas100': respuestas, 'preguntas500': respuestas,
            'precision80': (resumen['precision'] or 0) if respuestas >= 50 else min(79, resumen['precision'] or 0),
            'tarjetas50': repasos, 'dominadas20': dominadas, 'desarrollo10': intentos, 'matricula': matricula,
            'simulacro': simulacros, 'bibliotecario': 1 if temas and resumidos == temas else 0,
            'maraton': minutos,
            'sin_fallos': 1 if fallos_alguna_vez and not resumen['mis_fallos'] else 0}
        lista = []
        for ident, nombre, descripcion, objetivo in LOGROS:
            v = valores.get(ident, 0)
            lista.append({'id': ident, 'nombre': nombre, 'descripcion': descripcion, 'valor': v, 'objetivo': objetivo,
                          'conseguido': v >= objetivo, 'progreso': round(min(1.0, v / objetivo), 3)})
        xp = respuestas * 2 + aciertos * 3 + repasos * 2 + intentos * 12 + minutos + resumen['tests'] * 15
        xp += sum(25 for l in lista if l['conseguido'])
        nivel = 1
        while _xp_para(nivel + 1) <= xp:
            nivel += 1
        return {'logros': lista, 'nivel': nivel, 'xp': xp, 'xp_nivel': _xp_para(nivel), 'xp_siguiente': _xp_para(nivel + 1),
                'conseguidos': sum(1 for l in lista if l['conseguido']), 'minutos': minutos,
                'minutos_hoy': b.minutos_estudio(codigo, self._hoy_epoch()),
                'minutos_semana': b.minutos_estudio(codigo, self._hoy_epoch() - 6 * 86400),
                'minutos_dias': self._minutos_dias(14)}

    def _minutos_dias(self, n):
        """Minutos con el temporizador de los ultimos 
 dias, del mas antiguo a hoy."""
        hoy = date.today()
        por_dia = self.banco.minutos_por_dia(self._codigo(), self._hoy_epoch() - (n - 1) * 86400)
        return [{'fecha': (hoy - timedelta(days=n - 1 - i)).isoformat(),
                 'minutos': por_dia.get((hoy - timedelta(days=n - 1 - i)).isoformat(), 0)} for i in range(n)]

    def _racha_maxima(self):
        actividad = sorted(self.banco.actividad(self._codigo()))
        mejor = actual = 0
        anterior = None
        for dia in actividad:
            d = date.fromisoformat(dia)
            actual = actual + 1 if anterior and (d - anterior).days == 1 else 1
            mejor = max(mejor, actual)
            anterior = d
        return mejor

    # ============================================================ exportar

    def _carpeta_exportes(self):
        carpeta = self.datos.parent / 'exportes'
        carpeta.mkdir(parents=True, exist_ok=True)
        return carpeta

    def exportar(self, tipo='dossier'):
        """'dossier': HTML para leer o imprimir con resúmenes, glosario, fallos y calendario.
        'anki': CSV (separado por tabuladores) con las tarjetas, para importarlo en Anki."""
        from motor import ErrorProfesor
        codigo = self._codigo()
        nombre = re.sub(r'[^\w\-]+', '_', codigo)
        if tipo == 'anki':
            ruta = self._carpeta_exportes() / f'{nombre}-tarjetas-anki.txt'
            mazo = self._mazo()
            if not mazo:
                raise ErrorProfesor('No hay tarjetas que exportar en esta asignatura.')
            with open(ruta, 'w', encoding='utf-8', newline='') as f:
                f.write('#separator:tab\n#html:false\n#tags column:3\n')
                escritor = csv.writer(f, delimiter='\t', quoting=csv.QUOTE_MINIMAL)
                for t in mazo:
                    etiqueta = re.sub(r'\W+', '_', nombre_tema(t['ruta'])).strip('_')
                    escritor.writerow([t['frente'].replace('\n', ' '), t['reverso'].replace('\n', ' '),
                                       f'{nombre} {etiqueta}'])
            return {'archivo': str(ruta), 'cantidad': len(mazo)}
        if tipo != 'dossier':
            raise ErrorProfesor('Formato de exportación desconocido.')
        ruta = self._carpeta_exportes() / f'{nombre}-dossier.html'
        ruta.write_text(self._dossier_html(), encoding='utf-8')
        return {'archivo': str(ruta)}

    def _dossier_html(self):
        codigo = self._codigo()
        e = html.escape
        resumenes = self.banco.resumenes(codigo)
        examen = self.proximo_examen()
        cal = self.calendario()
        pred = self.prediccion()
        partes = [f'<h1>{e(self.asignatura["nombre"])}</h1>',
                  f'<p class="sub">Dossier de estudio · {date.today().strftime("%d/%m/%Y")}'
                  + (f' · {e(examen["nombre"])} en {examen["dias"]} días' if examen else '') + '</p>']
        if pred:
            partes.append(f'<div class="caja">Nota estimada si el examen fuera hoy: <b>{pred["nota"]:.1f}</b> '
                          f'({pred["cubierto"]} % del temario practicado). {e(pred["consejo"])}</div>')
        if cal:
            partes.append('<h2>Calendario hasta el examen</h2><table>')
            for d in cal['dias']:
                partes.append(f'<tr><td class="dia">{e(d["etiqueta"])}</td><td>'
                              + '<br>'.join(e(t) for t in d['tareas']) + '</td></tr>')
            partes.append('</table>')
        partes.append('<h2>Resúmenes por tema</h2>')
        hay = False
        for ruta in self._rutas_legibles():
            r = resumenes.get(ruta)
            if not r:
                continue
            hay = True
            partes.append(f'<section><h3>{e(nombre_tema(ruta))}</h3>{_markdown_html(ia.texto_rico(r["texto"]))}</section>')
        if not hay:
            partes.append('<p class="sub">Aún no hay resúmenes: créalos en la pantalla «Resúmenes» del profesor.</p>')
        glosario = self.glosario()['glosario']
        if glosario:
            partes.append('<h2>Glosario</h2><dl>')
            for g in glosario:
                partes.append(f'<dt>{e(g["termino"])} <span>{e(g["tema"])}</span></dt><dd>{e(g["definicion"])}</dd>')
            partes.append('</dl>')
        fallos = self.banco.guardadas(codigo)
        if fallos:
            partes.append('<h2>Mis fallos</h2><ol>')
            for p in fallos:
                partes.append(f'<li>{e(ia.reparar(p["enunciado"]))}<br><b>Respuesta:</b> '
                              f'{e(ia.reparar(p["opciones"][p["respuesta"]]))}</li>')
            partes.append('</ol>')
        estilo = ('body{font-family:"Segoe UI",system-ui,sans-serif;max-width:860px;margin:40px auto;padding:0 24px;'
                  'color:#1d2330;line-height:1.55}h1{margin-bottom:0}h2{border-bottom:2px solid #5b9dff;'
                  'padding-bottom:4px;margin-top:40px}h3{color:#2a6ee0}.sub{color:#6b7385}'
                  '.caja{background:#eef4ff;border-radius:10px;padding:12px 16px;margin:16px 0}'
                  'table{border-collapse:collapse;width:100%}td{border-bottom:1px solid #e3e6ee;padding:6px 8px;'
                  'vertical-align:top}td.dia{font-weight:600;width:110px}dt{font-weight:600;margin-top:10px}'
                  'dt span{font-weight:400;color:#6b7385;font-size:.85em}dd{margin:2px 0 0 0}'
                  'section{break-inside:avoid}@media print{body{margin:0}h2{break-after:avoid}}')
        return ('<!doctype html><html lang="es"><head><meta charset="utf-8"><title>'
                f'{e(self.asignatura["nombre"])} · dossier</title><style>{estilo}</style></head><body>'
                + '\n'.join(partes) + '</body></html>')


def _markdown_html(md):
    """El Markdown ligero de `ia.texto_rico` a HTML."""
    e = html.escape
    salida, en_lista = [], False
    for linea in md.split('\n'):
        texto = re.sub(r'\*\*(.+?)\*\*', r'<b>\1</b>', e(linea.lstrip('- ').strip() if linea.startswith(('- ', '  - '))
                                                          else linea.strip()))
        if linea.startswith(('- ', '  - ')):
            if not en_lista:
                salida.append('<ul>')
                en_lista = True
            salida.append(f'<li>{texto}</li>')
            continue
        if en_lista:
            salida.append('</ul>')
            en_lista = False
        if linea.startswith('## '):
            salida.append(f'<h4>{texto[3:]}</h4>')
        elif texto:
            salida.append(f'<p>{texto}</p>')
    if en_lista:
        salida.append('</ul>')
    return '\n'.join(salida)
