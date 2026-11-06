"""Proceso del profesor: recibe órdenes JSON por stdin (una por línea) y responde por stdout.

Petición:   {"id": 7, "cmd": "responder", ...argumentos}
Respuesta:  {"id": 7, "ok": true, ...}   o   {"id": 7, "ok": false, "mensaje": "..."}
Evento:     {"evento": "pregunta_lista", ...}   (sin id: avisos espontáneos)

Uso:  python servidor.py <carpeta_datos>
"""
import json
import queue
import sys
import threading
import traceback
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import ia  # noqa: E402
from motor import ErrorProfesor, Motor  # noqa: E402

_salida = threading.Lock()

# Órdenes que pueden esperar a la IA: cada una corre en su propio hilo.
CON_IA = {'duda', 'resumen', 'abierta', 'corregir', 'analisis', 'truco', 'mapa'}
# Páginas del lector: van por su propia cola para no hacer esperar al resto de órdenes.
PAGINAS = {'pagina'}


def enviar(mensaje):
    linea = json.dumps(mensaje, ensure_ascii=False)
    with _salida:
        sys.stdout.write(linea + '\n')
        sys.stdout.flush()


def ejecutar(motor, peticion):
    cmd = peticion.get('cmd')
    a = peticion
    if cmd == 'hola':
        return motor.hola(a['codigo'], a.get('nombre', ''), a['temario'], a.get('examenes'))
    if cmd == 'empezar':
        return motor.empezar(a.get('config', {}))
    if cmd == 'continuar':
        return motor.continuar()
    if cmd == 'pregunta':
        return {'pregunta': motor.pregunta(a['token'], int(a['indice']))}
    if cmd == 'responder':
        return {'pregunta': motor.responder(a['token'], int(a['indice']), int(a['eleccion']), bool(a.get('dudosa')))}
    if cmd == 'terminar':
        return motor.terminar(a['token'])
    if cmd == 'progreso':
        return motor.progreso()
    if cmd == 'descartar':
        return motor.descartar(a['token'], int(a['indice']))
    if cmd == 'rellenar':
        return motor.rellenar(a['token'])
    if cmd == 'reindexar':
        return {'archivos': motor.indexar()}
    if cmd == 'ia':
        return {'ia': motor.comprobar_ia(a.get('modelo', ''))}
    if cmd == 'arrancar_ia':
        return motor.arrancar_ia()
    if cmd == 'reserva':
        return motor.poner_reserva(bool(a.get('activa', True)))
    if cmd == 'precalentar':
        return motor.precalentar(a.get('modelo', ''))
    if cmd == 'duda':
        return motor.duda(a.get('texto', ''), a.get('token'), a.get('indice'), a.get('id'), a.get('historial'),
                          a.get('pregunta'), a.get('etiqueta', ''))
    # --- herramientas de estudio (estudio.py)
    if cmd == 'inicio':
        return motor.inicio()
    if cmd == 'examenes':
        motor.poner_examenes(a.get('examenes'))
        return motor.inicio()
    if cmd == 'resumenes':
        return motor.resumenes()
    if cmd == 'resumen':
        return motor.resumen(a['ruta'], bool(a.get('rehacer')), a.get('id'), bool(a.get('ia', True)))
    if cmd == 'resumir_todos':
        return motor.resumir_todos()
    if cmd == 'detener_resumenes':
        return motor.detener_resumenes()
    if cmd == 'desarrollo':
        return motor.desarrollo()
    if cmd == 'abierta':
        return motor.abierta(a.get('ruta'), a.get('dificultad', 'Intermedia'), bool(a.get('ia', True)), a.get('id'),
                             bool(a.get('nueva')))
    if cmd == 'corregir':
        return motor.corregir(a['pregunta'], a.get('respuesta', ''), bool(a.get('ia', True)), a.get('id'))
    if cmd == 'descartar_abierta':
        return motor.descartar_abierta(a['pregunta'])
    if cmd == 'analisis':
        return motor.analisis(bool(a.get('ia', True)), a.get('id'), bool(a.get('rehacer')), bool(a.get('generar', True)))
    if cmd == 'truco':
        return motor.truco(a['tarjeta'], a.get('frente', ''), a.get('reverso', ''), a.get('id'),
                           bool(a.get('rehacer')))
    if cmd == 'conversacion':
        return motor.conversacion()
    if cmd == 'nueva_conversacion':
        return motor.nueva_conversacion()
    # --- más herramientas (herramientas.py)
    if cmd == 'glosario':
        return motor.glosario()
    if cmd == 'buscar':
        return motor.buscar(a.get('texto', ''))
    if cmd == 'mapa':
        return motor.mapa(a['ruta'], bool(a.get('rehacer')), bool(a.get('ia', True)), a.get('id'))
    if cmd == 'calendario':
        return {'calendario': motor.calendario(), 'prediccion': motor.prediccion()}
    if cmd == 'temario':
        return motor.temario()
    if cmd == 'leer':
        return motor.leer(a['ruta'])
    if cmd == 'texto_region':
        return motor.texto_region(a['ruta'], int(a.get('indice', 0)), a.get('caja') or [0, 0, 1, 1],
                                  int(a['hasta']) if a.get('hasta') is not None else None)
    if cmd == 'pagina':
        return motor.pagina(a['ruta'], int(a.get('indice', 0)), int(a.get('ancho', 1000)))
    if cmd == 'posicion_lectura':
        return motor.posicion_lectura(a['ruta'], float(a.get('posicion', 0)))
    if cmd == 'tam_lectura':
        return motor.tam_lectura(int(a.get('tam', 17)))
    if cmd == 'marcar_leido':
        return motor.marcar_leido(a['ruta'], bool(a.get('leido', True)))
    if cmd == 'logros':
        return motor.logros()
    if cmd == 'registrar_estudio':
        return motor.registrar_estudio(int(a.get('minutos', 25)), a.get('tipo', 'pomodoro'))
    if cmd == 'exportar':
        return motor.exportar(a.get('tipo', 'dossier'))
    if cmd == 'guardar':
        return motor.guardar(a['token'], int(a['indice']), bool(a.get('guardar', True)))
    if cmd == 'fallos':
        return motor.fallos()
    if cmd == 'quitar_fallo':
        return motor.quitar_fallo(a.get('pregunta'), a.get('token'), bool(a.get('acertadas')))
    if cmd == 'tarjetas':
        return motor.tarjetas(int(a.get('n', 20)), a.get('temas'), bool(a.get('extra')))
    if cmd == 'tarjeta':
        return motor.tarjeta(a['tarjeta'], a.get('nota', 1))
    if cmd == 'temas':  # agrupado actual; con `ia`, la IA vuelve a nombrar los temas
        salida = {'grupos': motor.grupos()}
        if a.get('ia'):
            salida.update(motor.organizar_temas(forzar=True))
        return salida
    if cmd == 'detener':
        motor.detener_generacion()
        return {}
    raise ErrorProfesor(f'Orden desconocida: {cmd}')


def atender(motor, peticion):
    respuesta = {'id': peticion.get('id')}
    try:
        respuesta.update(ejecutar(motor, peticion) or {})
        respuesta['ok'] = True
    except ErrorProfesor as e:
        respuesta.update(ok=False, mensaje=str(e))
    except Exception as e:  # cualquier fallo inesperado se informa sin tumbar el proceso
        traceback.print_exc(file=sys.stderr)
        respuesta.update(ok=False, mensaje=f'Error interno del profesor: {e}')
    enviar(respuesta)


def main():
    sys.stdin.reconfigure(encoding='utf-8')
    sys.stdout.reconfigure(encoding='utf-8')
    datos = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent / 'datos' / 'profesor'
    # Qué IA se usa: datos/ia.json (junto a asignaturas.json); sin él, Ollama en este equipo
    ia.configurar(datos.parent / 'ia.json')
    motor = Motor(datos, enviar)
    cola = queue.Queue()
    cola_paginas = queue.Queue()

    # Las órdenes se atienden en orden, salvo las que esperan a la IA (pueden tardar minutos), las
    # páginas del lector (cola aparte) y las cancelaciones (deben llegar aunque haya otra orden en curso).
    def trabajador(c):
        while True:
            peticion = c.get()
            if peticion is None:
                return
            atender(motor, peticion)

    hilo = threading.Thread(target=trabajador, args=(cola,), daemon=True)
    hilo.start()
    threading.Thread(target=trabajador, args=(cola_paginas,), daemon=True).start()
    enviar({'evento': 'listo'})
    for linea in sys.stdin:
        linea = linea.strip()
        if not linea:
            continue
        try:
            peticion = json.loads(linea)
        except ValueError:
            enviar({'evento': 'aviso', 'mensaje': 'Petición ilegible.'})
            continue
        cmd = peticion.get('cmd')
        if cmd == 'salir':
            break
        if cmd == 'cancelar_duda':
            motor.cancelar_duda.set()
            continue
        if cmd == 'cancelar':  # cancela la tarea de IA de la petición `tarea` (o todas)
            motor.cancelar(peticion.get('tarea'))
            continue
        if cmd in CON_IA:
            threading.Thread(target=atender, args=(motor, peticion), daemon=True).start()
            continue
        (cola_paginas if cmd in PAGINAS else cola).put(peticion)
    motor.detener_generacion()
    motor.detener_resumenes()
    motor.detener_temas()
    motor.cancelar()
    cola.put(None)
    cola_paginas.put(None)
    hilo.join(timeout=2)


if __name__ == '__main__':
    main()
