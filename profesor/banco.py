"""Persistencia del profesor en SQLite: temario indexado, banco de preguntas,
tests y respuestas.

Una única conexión compartida por los hilos, protegida con un candado.
"""
import json
import sqlite3
import threading
import time
from contextlib import contextmanager

ESQUEMA = '''
CREATE TABLE IF NOT EXISTS archivos(
    asignatura TEXT, ruta TEXT, firma TEXT, paginas INTEGER DEFAULT 0, error TEXT DEFAULT '',
    PRIMARY KEY(asignatura, ruta));
CREATE TABLE IF NOT EXISTS fragmentos(
    id INTEGER PRIMARY KEY AUTOINCREMENT, asignatura TEXT, ruta TEXT, ubicacion TEXT, texto TEXT);
CREATE INDEX IF NOT EXISTS fragmentos_ruta ON fragmentos(asignatura, ruta);
CREATE TABLE IF NOT EXISTS preguntas(
    id TEXT PRIMARY KEY, asignatura TEXT, ruta TEXT, ubicacion TEXT,
    origen TEXT, tipo TEXT, dificultad TEXT, enunciado TEXT, opciones TEXT, respuesta INTEGER,
    explicacion TEXT, cita TEXT, modelo TEXT DEFAULT '', creada REAL,
    vistas INTEGER DEFAULT 0, aciertos INTEGER DEFAULT 0, fallos INTEGER DEFAULT 0,
    caja INTEGER DEFAULT 0, ultima REAL DEFAULT 0, descartada INTEGER DEFAULT 0);
CREATE INDEX IF NOT EXISTS preguntas_asig ON preguntas(asignatura, origen);
CREATE TABLE IF NOT EXISTS sesiones(
    token TEXT PRIMARY KEY, asignatura TEXT, config TEXT, preguntas TEXT,
    creada REAL, terminada REAL DEFAULT 0, segundos INTEGER DEFAULT 0, resultado TEXT DEFAULT '');
CREATE TABLE IF NOT EXISTS respuestas(
    token TEXT, indice INTEGER, pregunta TEXT, eleccion INTEGER, correcta INTEGER, momento REAL,
    PRIMARY KEY(token, indice));
CREATE TABLE IF NOT EXISTS ajustes(asignatura TEXT PRIMARY KEY, config TEXT);
CREATE TABLE IF NOT EXISTS guardadas(
    asignatura TEXT, pregunta TEXT, creada REAL, aciertos INTEGER DEFAULT 0,
    PRIMARY KEY(asignatura, pregunta));
CREATE TABLE IF NOT EXISTS tarjetas(
    asignatura TEXT, id TEXT, caja INTEGER DEFAULT 0, proxima REAL DEFAULT 0, vistas INTEGER DEFAULT 0,
    PRIMARY KEY(asignatura, id));
CREATE TABLE IF NOT EXISTS repasos(asignatura TEXT, tarjeta TEXT, momento REAL, nota INTEGER);
CREATE TABLE IF NOT EXISTS resumenes(
    asignatura TEXT, ruta TEXT, firma TEXT, texto TEXT, origen TEXT, modelo TEXT DEFAULT '', creado REAL,
    PRIMARY KEY(asignatura, ruta));
CREATE TABLE IF NOT EXISTS abiertas(
    id TEXT PRIMARY KEY, asignatura TEXT, ruta TEXT, ubicacion TEXT, origen TEXT, enunciado TEXT,
    puntos TEXT, respuesta TEXT, cita TEXT, modelo TEXT DEFAULT '', creada REAL, descartada INTEGER DEFAULT 0);
CREATE TABLE IF NOT EXISTS intentos(
    abierta TEXT, asignatura TEXT, momento REAL, respuesta TEXT, nota REAL, correccion TEXT);
CREATE TABLE IF NOT EXISTS conversacion(
    asignatura TEXT, momento REAL, pregunta TEXT, respuesta TEXT, fuentes TEXT, contexto TEXT DEFAULT '',
    archivada INTEGER DEFAULT 0);
CREATE TABLE IF NOT EXISTS memoria(asignatura TEXT, clave TEXT, valor TEXT, momento REAL,
    PRIMARY KEY(asignatura, clave));
CREATE TABLE IF NOT EXISTS estudio(asignatura TEXT, momento REAL, minutos INTEGER, tipo TEXT DEFAULT 'pomodoro');
'''

# Caja de Leitner -> días hasta volver a preguntar.
REPASO_DIAS = [0, 1, 3, 7, 16, 35]


class Banco:
    def __init__(self, ruta):
        self.ruta = str(ruta)
        self.db = sqlite3.connect(self.ruta, check_same_thread=False, timeout=10)
        self.db.row_factory = sqlite3.Row
        self.candado = threading.RLock()
        with self.transaccion() as db:
            db.executescript(ESQUEMA)
            # Columnas nuevas en bancos de versiones anteriores
            columnas = {c[1] for c in db.execute('PRAGMA table_info(preguntas)')}
            if 'formato' not in columnas:
                db.execute("ALTER TABLE preguntas ADD COLUMN formato TEXT DEFAULT ''")
            if 'motivos' not in columnas:  # por qué es correcta o incorrecta cada opción (JSON)
                db.execute("ALTER TABLE preguntas ADD COLUMN motivos TEXT DEFAULT ''")
            if 'dudas' not in columnas:  # veces acertada sin estar seguro
                db.execute('ALTER TABLE preguntas ADD COLUMN dudas INTEGER DEFAULT 0')
            if 'dudosa' not in {c[1] for c in db.execute('PRAGMA table_info(respuestas)')}:
                db.execute('ALTER TABLE respuestas ADD COLUMN dudosa INTEGER DEFAULT 0')

    @contextmanager
    def transaccion(self):
        with self.candado:
            try:
                yield self.db
                self.db.commit()
            except BaseException:
                self.db.rollback()
                raise

    def cerrar(self):
        with self.candado:
            self.db.close()

    # ------------------------------------------------------------- temario

    def archivos(self, asignatura):
        with self.transaccion() as db:
            return {r['ruta']: dict(r) for r in db.execute(
                'SELECT * FROM archivos WHERE asignatura=?', (asignatura,))}

    def guardar_archivo(self, asignatura, ruta, firma, paginas, fragmentos, error=''):
        with self.transaccion() as db:
            db.execute('DELETE FROM fragmentos WHERE asignatura=? AND ruta=?', (asignatura, ruta))
            db.executemany('INSERT INTO fragmentos(asignatura,ruta,ubicacion,texto) VALUES(?,?,?,?)',
                           [(asignatura, ruta, u, t) for u, t in fragmentos])
            db.execute('INSERT OR REPLACE INTO archivos(asignatura,ruta,firma,paginas,error) '
                       'VALUES(?,?,?,?,?)', (asignatura, ruta, firma, paginas, error))

    def olvidar_archivo(self, asignatura, ruta):
        with self.transaccion() as db:
            db.execute('DELETE FROM fragmentos WHERE asignatura=? AND ruta=?', (asignatura, ruta))
            db.execute('DELETE FROM archivos WHERE asignatura=? AND ruta=?', (asignatura, ruta))

    def fragmentos(self, asignatura, rutas=None):
        with self.transaccion() as db:
            filas = [dict(r) for r in db.execute(
                'SELECT * FROM fragmentos WHERE asignatura=? ORDER BY ruta, id', (asignatura,))]
        if rutas is not None:
            rutas = set(rutas)
            filas = [f for f in filas if f['ruta'] in rutas]
        return filas

    # ------------------------------------------------------------ preguntas

    def guardar_pregunta(self, p):
        with self.transaccion() as db:
            db.execute('''INSERT OR IGNORE INTO preguntas(id,asignatura,ruta,ubicacion,origen,tipo,
                dificultad,enunciado,opciones,respuesta,explicacion,cita,modelo,creada,formato,motivos)
                VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)''',
                       (p['id'], p['asignatura'], p['ruta'], p['ubicacion'], p['origen'], p['tipo'],
                        p['dificultad'], p['enunciado'], json.dumps(p['opciones'], ensure_ascii=False),
                        p['respuesta'], p['explicacion'], p['cita'], p.get('modelo', ''), time.time(),
                        p.get('formato', ''), json.dumps(p.get('motivos') or [], ensure_ascii=False)))
            if p.get('motivos'):  # preguntas guardadas antes de que hubiera motivos: se completan
                db.execute("UPDATE preguntas SET motivos=? WHERE id=? AND (motivos IS NULL OR motivos IN ('', '[]'))",
                           (json.dumps(p['motivos'], ensure_ascii=False), p['id']))

    def pregunta(self, ident):
        with self.transaccion() as db:
            fila = db.execute('SELECT * FROM preguntas WHERE id=?', (ident,)).fetchone()
        return _pregunta(fila) if fila else None

    def preguntas(self, asignatura, origen=None, dificultad=None, rutas=None):
        consulta = 'SELECT * FROM preguntas WHERE asignatura=? AND descartada=0'
        args = [asignatura]
        if origen:
            consulta += ' AND origen=?'
            args.append(origen)
        if dificultad:
            consulta += ' AND dificultad=?'
            args.append(dificultad)
        with self.transaccion() as db:
            filas = [_pregunta(r) for r in db.execute(consulta, args)]
        if rutas is not None:
            rutas = set(rutas)
            filas = [p for p in filas if p['ruta'] in rutas]
        return filas

    def para_repasar(self, asignatura, rutas=None, ahora=None):
        """Preguntas falladas cuyo repaso toca ya (repetición espaciada)."""
        ahora = ahora or time.time()
        resultado = []
        for p in self.preguntas(asignatura, rutas=rutas):
            if p['fallos'] == 0 and not p.get('dudas'):
                continue
            espera = REPASO_DIAS[min(p['caja'], len(REPASO_DIAS) - 1)] * 86400
            if p['caja'] == 0 or p['ultima'] + espera <= ahora:
                resultado.append(p)
        resultado.sort(key=lambda p: (p['caja'], p['ultima']))
        return resultado

    def descartar(self, ident):
        with self.transaccion() as db:
            db.execute('UPDATE preguntas SET descartada=1 WHERE id=?', (ident,))

    # -------------------------------------------------------------- sesiones

    def crear_sesion(self, token, asignatura, config, preguntas):
        with self.transaccion() as db:
            db.execute('INSERT INTO sesiones(token,asignatura,config,preguntas,creada) VALUES(?,?,?,?,?)',
                       (token, asignatura, json.dumps(config, ensure_ascii=False),
                        json.dumps(preguntas, ensure_ascii=False), time.time()))

    def sesion(self, token):
        with self.transaccion() as db:
            fila = db.execute('SELECT * FROM sesiones WHERE token=?', (token,)).fetchone()
            if not fila:
                return None
            s = dict(fila)
            s['config'] = json.loads(s['config'])
            s['preguntas'] = json.loads(s['preguntas'])
            s['resultado'] = json.loads(s['resultado']) if s['resultado'] else None
            s['respuestas'] = {r['indice']: dict(r) for r in db.execute(
                'SELECT * FROM respuestas WHERE token=?', (token,))}
        return s

    def actualizar_preguntas_sesion(self, token, preguntas):
        with self.transaccion() as db:
            db.execute('UPDATE sesiones SET preguntas=? WHERE token=?',
                       (json.dumps(preguntas, ensure_ascii=False), token))

    def responder(self, token, indice, ident, eleccion, correcta, dudosa=False):
        """Guarda la respuesta y actualiza la caja de repaso de la pregunta. Una acertada sin estar seguro
        cuenta como acierto, pero vuelve a repaso pronto (caja 1) como si fuera un fallo leve."""
        ahora = time.time()
        with self.transaccion() as db:
            if db.execute('SELECT 1 FROM respuestas WHERE token=? AND indice=?', (token, indice)).fetchone():
                return False
            db.execute('INSERT INTO respuestas(token,indice,pregunta,eleccion,correcta,momento,dudosa) '
                       'VALUES(?,?,?,?,?,?,?)', (token, indice, ident, eleccion, int(correcta), ahora, int(dudosa)))
            if correcta and dudosa:
                db.execute('UPDATE preguntas SET vistas=vistas+1, aciertos=aciertos+1, dudas=dudas+1, '
                           'caja=MIN(caja,1), ultima=? WHERE id=?', (ahora, ident))
            elif correcta:
                db.execute('UPDATE preguntas SET vistas=vistas+1, aciertos=aciertos+1, '
                           'caja=MIN(caja+1,5), ultima=? WHERE id=?', (ahora, ident))
            else:
                db.execute('UPDATE preguntas SET vistas=vistas+1, fallos=fallos+1, caja=0, ultima=? '
                           'WHERE id=?', (ahora, ident))
        return True

    def terminar_sesion(self, token, resultado, segundos):
        with self.transaccion() as db:
            db.execute('UPDATE sesiones SET terminada=?, resultado=?, segundos=? WHERE token=?',
                       (time.time(), json.dumps(resultado, ensure_ascii=False), segundos, token))

    def sesion_pendiente(self, asignatura):
        with self.transaccion() as db:
            fila = db.execute('SELECT token FROM sesiones WHERE asignatura=? AND terminada=0 '
                              'ORDER BY creada DESC LIMIT 1', (asignatura,)).fetchone()
        return self.sesion(fila['token']) if fila else None

    def abandonar_pendientes(self, asignatura, salvo=None):
        """Un test nuevo sustituye al que estuviera a medias (queda en el historial sin nota)."""
        with self.transaccion() as db:
            db.execute("UPDATE sesiones SET terminada=?, resultado='' WHERE asignatura=? AND terminada=0 "
                       "AND token<>?", (time.time(), asignatura, salvo or ''))

    def historial(self, asignatura, limite=20):
        with self.transaccion() as db:
            filas = db.execute("SELECT token, creada, terminada, segundos, resultado FROM sesiones "
                               "WHERE asignatura=? AND terminada>0 AND resultado<>'' "
                               "ORDER BY terminada DESC LIMIT ?", (asignatura, limite)).fetchall()
        return [{'token': f['token'], 'fecha': f['terminada'], 'segundos': f['segundos'],
                 **json.loads(f['resultado'])} for f in filas][::-1]

    def estadisticas_por_tema(self, asignatura):
        with self.transaccion() as db:
            filas = db.execute('''SELECT p.ruta, COUNT(*) total, SUM(r.correcta) bien
                FROM respuestas r JOIN preguntas p ON p.id = r.pregunta
                WHERE p.asignatura=? GROUP BY p.ruta ORDER BY p.ruta''', (asignatura,)).fetchall()
        return [{'ruta': f['ruta'], 'total': f['total'], 'aciertos': f['bien'] or 0} for f in filas]

    def totales(self, asignatura):
        with self.transaccion() as db:
            r = db.execute('''SELECT COUNT(*) n, COALESCE(SUM(r.correcta),0) bien FROM respuestas r
                JOIN preguntas p ON p.id=r.pregunta WHERE p.asignatura=?''', (asignatura,)).fetchone()
            tests = db.execute("SELECT COUNT(*) FROM sesiones WHERE asignatura=? AND terminada>0 "
                               "AND resultado<>''", (asignatura,)).fetchone()[0]
            ia = db.execute("SELECT COUNT(*) FROM preguntas WHERE asignatura=? AND origen='ia' "
                            "AND descartada=0", (asignatura,)).fetchone()[0]
        return {'respuestas': r['n'], 'aciertos': r['bien'], 'tests': tests, 'banco_ia': ia}

    def actividad(self, asignatura):
        """Preguntas respondidas y tarjetas repasadas por día (fecha local 'AAAA-MM-DD' -> número)."""
        with self.transaccion() as db:
            filas = db.execute('''SELECT dia, SUM(n) n FROM (
                    SELECT date(r.momento, 'unixepoch', 'localtime') dia, COUNT(*) n
                    FROM respuestas r JOIN preguntas p ON p.id = r.pregunta
                    WHERE p.asignatura=? GROUP BY dia
                  UNION ALL
                    SELECT date(momento, 'unixepoch', 'localtime') dia, COUNT(*) n
                    FROM repasos WHERE asignatura=? GROUP BY dia
                  UNION ALL
                    SELECT date(momento, 'unixepoch', 'localtime') dia, COUNT(*) * 3 n
                    FROM intentos WHERE asignatura=? GROUP BY dia)
                GROUP BY dia''', (asignatura, asignatura, asignatura)).fetchall()
        return {f['dia']: f['n'] for f in filas}

    def hoy_por_tema(self, asignatura, desde):
        """Preguntas respondidas desde `desde` (epoch) agrupadas por documento: ruta -> número."""
        with self.transaccion() as db:
            filas = db.execute('''SELECT p.ruta, COUNT(*) n FROM respuestas r JOIN preguntas p ON p.id = r.pregunta
                WHERE p.asignatura=? AND r.momento>=? GROUP BY p.ruta''', (asignatura, desde)).fetchall()
        return {f['ruta']: f['n'] for f in filas}

    def repasos_desde(self, asignatura, desde):
        with self.transaccion() as db:
            return db.execute('SELECT COUNT(*) FROM repasos WHERE asignatura=? AND momento>=?',
                              (asignatura, desde)).fetchone()[0]

    def sesiones_desde(self, asignatura, desde):
        """Tests terminados (con nota) desde `desde`, con su configuración."""
        with self.transaccion() as db:
            filas = db.execute("SELECT config, resultado FROM sesiones WHERE asignatura=? AND terminada>=? "
                               "AND resultado<>''", (asignatura, desde)).fetchall()
        return [{'config': json.loads(f['config']), **json.loads(f['resultado'])} for f in filas]

    # ------------------------------------------------------------ mis fallos

    def guardar_fallo(self, asignatura, ident):
        """Añade la pregunta a «Mis fallos» (o reinicia su racha de aciertos si ya estaba)."""
        with self.transaccion() as db:
            db.execute('INSERT INTO guardadas(asignatura,pregunta,creada) VALUES(?,?,?) '
                       'ON CONFLICT(asignatura,pregunta) DO UPDATE SET aciertos=0', (asignatura, ident, time.time()))

    def quitar_fallo(self, asignatura, ident):
        with self.transaccion() as db:
            db.execute('DELETE FROM guardadas WHERE asignatura=? AND pregunta=?', (asignatura, ident))

    def acierto_en_fallo(self, asignatura, ident):
        with self.transaccion() as db:
            db.execute('UPDATE guardadas SET aciertos=aciertos+1 WHERE asignatura=? AND pregunta=?',
                       (asignatura, ident))

    def guardadas(self, asignatura):
        """Preguntas de «Mis fallos», las más recientes primero."""
        with self.transaccion() as db:
            filas = db.execute('''SELECT g.creada guardada, g.aciertos racha, p.* FROM guardadas g
                JOIN preguntas p ON p.id = g.pregunta
                WHERE g.asignatura=? AND p.descartada=0 ORDER BY g.creada DESC''', (asignatura,)).fetchall()
        return [_pregunta(f) for f in filas]

    def ids_guardadas(self, asignatura):
        with self.transaccion() as db:
            return {f[0] for f in db.execute('SELECT pregunta FROM guardadas WHERE asignatura=?', (asignatura,))}

    # -------------------------------------------------------------- tarjetas

    def tarjetas(self, asignatura):
        """Estado de repaso de las tarjetas ya vistas: id -> {caja, proxima, vistas}."""
        with self.transaccion() as db:
            return {f['id']: dict(f) for f in db.execute(
                'SELECT id, caja, proxima, vistas FROM tarjetas WHERE asignatura=?', (asignatura,))}

    def repasar_tarjeta(self, asignatura, ident, nota, ahora=None):
        """nota: 0 = otra vez, 1 = bien, 2 = fácil. Devuelve la caja nueva."""
        ahora = ahora or time.time()
        with self.transaccion() as db:
            fila = db.execute('SELECT caja FROM tarjetas WHERE asignatura=? AND id=?', (asignatura, ident)).fetchone()
            caja = fila['caja'] if fila else 0
            caja = 0 if nota <= 0 else min(caja + nota, len(REPASO_DIAS) - 1)
            proxima = ahora + REPASO_DIAS[caja] * 86400 if nota > 0 else ahora
            db.execute('INSERT INTO tarjetas(asignatura,id,caja,proxima,vistas) VALUES(?,?,?,?,1) '
                       'ON CONFLICT(asignatura,id) DO UPDATE SET caja=?, proxima=?, vistas=vistas+1',
                       (asignatura, ident, caja, proxima, caja, proxima))
            db.execute('INSERT INTO repasos VALUES(?,?,?,?)', (asignatura, ident, ahora, nota))
        return caja

    # --------------------------------------------------------------- ajustes

    def ajustes(self, asignatura):
        with self.transaccion() as db:
            fila = db.execute('SELECT config FROM ajustes WHERE asignatura=?', (asignatura,)).fetchone()
        return json.loads(fila['config']) if fila else {}

    def guardar_ajustes(self, asignatura, config):
        with self.transaccion() as db:
            db.execute('INSERT OR REPLACE INTO ajustes VALUES(?,?)',
                       (asignatura, json.dumps(config, ensure_ascii=False)))

    # ------------------------------------------------------------- resúmenes

    def resumenes(self, asignatura):
        """ruta -> {firma, texto, origen, modelo, creado}"""
        with self.transaccion() as db:
            return {f['ruta']: dict(f) for f in db.execute(
                'SELECT * FROM resumenes WHERE asignatura=?', (asignatura,))}

    def guardar_resumen(self, asignatura, ruta, firma, texto, origen, modelo=''):
        with self.transaccion() as db:
            db.execute('INSERT OR REPLACE INTO resumenes VALUES(?,?,?,?,?,?,?)',
                       (asignatura, ruta, firma, texto, origen, modelo, time.time()))

    # --------------------------------------------------- preguntas abiertas

    def guardar_abierta(self, a):
        with self.transaccion() as db:
            db.execute('''INSERT OR IGNORE INTO abiertas(id,asignatura,ruta,ubicacion,origen,enunciado,puntos,
                respuesta,cita,modelo,creada) VALUES(?,?,?,?,?,?,?,?,?,?,?)''',
                       (a['id'], a['asignatura'], a['ruta'], a['ubicacion'], a['origen'], a['enunciado'],
                        json.dumps(a['puntos'], ensure_ascii=False), a['respuesta'], a['cita'],
                        a.get('modelo', ''), time.time()))

    def abierta(self, ident):
        with self.transaccion() as db:
            fila = db.execute('SELECT * FROM abiertas WHERE id=?', (ident,)).fetchone()
        return _abierta(fila) if fila else None

    def abiertas(self, asignatura, rutas=None):
        """Preguntas abiertas con cuántas veces se han intentado y su mejor nota."""
        with self.transaccion() as db:
            filas = db.execute('''SELECT a.*, COUNT(i.momento) intentos, MAX(i.nota) mejor, MAX(i.momento) ultimo
                FROM abiertas a LEFT JOIN intentos i ON i.abierta = a.id
                WHERE a.asignatura=? AND a.descartada=0 GROUP BY a.id''', (asignatura,)).fetchall()
        lista = [_abierta(f) for f in filas]
        if rutas is not None:
            rutas = set(rutas)
            lista = [a for a in lista if a['ruta'] in rutas]
        return lista

    def descartar_abierta(self, ident):
        with self.transaccion() as db:
            db.execute('UPDATE abiertas SET descartada=1 WHERE id=?', (ident,))

    def guardar_intento(self, ident, asignatura, respuesta, nota, correccion):
        with self.transaccion() as db:
            db.execute('INSERT INTO intentos VALUES(?,?,?,?,?,?)',
                       (ident, asignatura, time.time(), respuesta, nota, json.dumps(correccion, ensure_ascii=False)))

    def intentos(self, asignatura, limite=20):
        with self.transaccion() as db:
            filas = db.execute('''SELECT i.momento, i.nota, a.enunciado, a.ruta, a.id FROM intentos i
                JOIN abiertas a ON a.id = i.abierta WHERE i.asignatura=? ORDER BY i.momento DESC LIMIT ?''',
                               (asignatura, limite)).fetchall()
        return [dict(f) for f in filas]

    # ------------------------------------------------------------ conversación

    def guardar_turno(self, asignatura, pregunta, respuesta, fuentes, contexto=''):
        with self.transaccion() as db:
            db.execute('INSERT INTO conversacion(asignatura,momento,pregunta,respuesta,fuentes,contexto) '
                       'VALUES(?,?,?,?,?,?)', (asignatura, time.time(), pregunta, respuesta,
                                               json.dumps(fuentes, ensure_ascii=False), contexto))

    def conversacion(self, asignatura, limite=30):
        """Turnos de la conversación en curso (no archivada), del más antiguo al más reciente."""
        with self.transaccion() as db:
            filas = db.execute('SELECT * FROM conversacion WHERE asignatura=? AND archivada=0 '
                               'ORDER BY momento DESC LIMIT ?', (asignatura, limite)).fetchall()
        return [{'pregunta': f['pregunta'], 'respuesta': f['respuesta'], 'fuentes': json.loads(f['fuentes'] or '[]'),
                 'contexto': f['contexto'], 'momento': f['momento']} for f in filas][::-1]

    def archivar_conversacion(self, asignatura):
        with self.transaccion() as db:
            db.execute('UPDATE conversacion SET archivada=1 WHERE asignatura=?', (asignatura,))

    # ------------------------------------------------------- tiempo de estudio

    def registrar_estudio(self, asignatura, minutos, tipo='pomodoro'):
        with self.transaccion() as db:
            db.execute('INSERT INTO estudio VALUES(?,?,?,?)', (asignatura, time.time(), int(minutos), tipo))

    def minutos_estudio(self, asignatura, desde=0):
        with self.transaccion() as db:
            return db.execute('SELECT COALESCE(SUM(minutos),0) FROM estudio WHERE asignatura=? AND momento>=?',
                              (asignatura, desde)).fetchone()[0]

    def minutos_por_dia(self, asignatura, desde):
        """{'AAAA-MM-DD': minutos} desde el instante desde (dia local de cada registro)."""
        with self.transaccion() as db:
            filas = db.execute('SELECT momento, minutos FROM estudio WHERE asignatura=? AND momento>=?',
                               (asignatura, desde)).fetchall()
        dias = {}
        for momento, minutos in filas:
            dia = time.strftime('%Y-%m-%d', time.localtime(momento))
            dias[dia] = dias.get(dia, 0) + int(minutos)
        return dias

    def contar(self, consulta, args=()):
        """Un número a partir de una consulta (para los logros)."""
        with self.transaccion() as db:
            fila = db.execute(consulta, args).fetchone()
        return (fila[0] or 0) if fila else 0

    # ---------------------------------------------------------------- memoria

    def recordar(self, asignatura, clave, valor):
        with self.transaccion() as db:
            db.execute('INSERT OR REPLACE INTO memoria VALUES(?,?,?,?)',
                       (asignatura, clave, json.dumps(valor, ensure_ascii=False), time.time()))

    def recuerdo(self, asignatura, clave):
        """(valor, momento) o (None, 0)."""
        with self.transaccion() as db:
            fila = db.execute('SELECT valor, momento FROM memoria WHERE asignatura=? AND clave=?',
                              (asignatura, clave)).fetchone()
        return (json.loads(fila['valor']), fila['momento']) if fila else (None, 0)


def _pregunta(fila):
    p = dict(fila)
    p['opciones'] = json.loads(p['opciones'])
    try:
        p['motivos'] = json.loads(p.get('motivos') or '[]')
    except ValueError:
        p['motivos'] = []
    return p


def _abierta(fila):
    a = dict(fila)
    a['puntos'] = json.loads(a['puntos'])
    return a
