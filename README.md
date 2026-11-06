# Universalis · Gestor de Asignaturas

Programa en C++17 para llevar el control del curso: calendario de exámenes, desglose de
notas con ponderaciones, nota media por asignatura y acceso directo a las carpetas de
temario y prácticas. Exporta además un informe HTML.

Lo último que ha cambiado está en [NOVEDADES.md](NOVEDADES.md); el profesor con IA se explica en
[PROFESOR.md](PROFESOR.md), y cómo conectarle tu propio modelo de IA (Ollama, LM Studio, llama.cpp, OpenAI,
OpenRouter…) en [CONECTAR_IA.md](CONECTAR_IA.md).

Viene en **dos versiones que comparten el mismo código y el mismo fichero de datos**:

| Ejecutable | Qué es |
|---|---|
| `gestor_gui.exe` | Interfaz gráfica de Windows: ventana con calendario, tablas y botones |
| `gestor_asignaturas.exe` | Versión de consola con menús de texto |

Sin dependencias externas en C++: parser JSON, calendario y exportador están incluidos en el código.
Tampoco usa `<filesystem>`, así que compila con cualquier compilador desde **C++11**
(probado con `-std=c++11`, `c++14` y `c++17`).

**Requisitos del Profesor** (la parte de estudio con IA): Python 3.10 o posterior con `pypdfium2` y
`pillow` (`python -m pip install pypdfium2 pillow`) y, para las funciones de IA, un servidor de modelos:
[Ollama](https://ollama.com) en tu equipo (lo que se usa por defecto) o cualquiera compatible con la API
de OpenAI (ver [CONECTAR_IA.md](CONECTAR_IA.md)). Sin IA el Profesor también funciona en modo Rápido.

## Arranque rápido (sin abrir Code::Blocks)

1. Doble clic en **`compilar.bat`**. Encuentra el compilador solo (incluido el MinGW que trae
   Code::Blocks) y genera `gestor_gui.exe` y `gestor_asignaturas.exe` en esta carpeta.
2. Doble clic en **`crear_acceso_directo.bat`**. Deja en el Escritorio un icono
   *Gestor de Asignaturas* que apunta directamente al `.exe` — sin ventana negra de por medio.
3. A partir de ahí, doble clic en el icono del Escritorio y listo.

Alternativas: `Gestor de Asignaturas.bat` abre la versión gráfica (y la compila si falta), y
`Gestor consola.bat` abre la de texto.

Si prefieres hacer el acceso directo a mano: clic derecho sobre `gestor_gui.exe` →
*Mostrar más opciones* → *Enviar a* → *Escritorio (crear acceso directo)*.

## La ventana

Tres columnas: asignaturas, ficha de la asignatura y calendario.

```
+---------------------------------------------------------------------------+
| UNIVERSALIS                                [informe] [tema] [ajustes] [?]   |
+---------------------+------------------------------+----------------------+
| PRIMER CUATRIMESTRE |  ASIGNATURA                  |  CALENDARIO          |
| dia nombre media    |  nombre en su color, ECTS    |  (mes dibujado)      |
| [lista]             |  nota media grande + escala  |                      |
|                     |  aviso de prueba cercana     |                      |
| SEGUNDO CUATRIMESTRE|  barra de % evaluado         |                      |
| [lista]             +------------------------------+----------------------+
|                     |  PRUEBAS EVALUABLES          | PRUEBAS DEL DIA      |
| Nueva Editar Dupl.  |  [tabla]                     |  24/12/2026          |
| Borrar              |                              |  PL: Parcial 2 (25%) |
| Carpeta Temario Pra.|  Anadir|Nota|Dupl|Edit|Borrar|  Clases: PL          |
+---------------------+------------------------------+----------------------+
| Media 8,06 · Notable   1.o: 8,06  2.o: --      Proxima: PL - Parcial 2 (en 5 dias) · 3 sin nota |
+---------------------------------------------------------------------------+
```

Las asignaturas van en **dos listas separadas**, cada una con su rótulo de cuatrimestre, y
la primera columna es el **día de clase**. Las columnas tienen ancho fijo: no se pueden
arrastrar sin querer.

**Barra superior negra** con el título *UNIVERSALIS · Tu espacio académico* y los botones de informe,
tema, ajustes y guía rápida. La barra de título de Windows también se pone oscura si tu versión lo soporta.

**Todos los cuadros de diálogo son propios**, dibujados por el programa: formularios, avisos,
confirmaciones y el selector de color (una rejilla de 24 muestras). Los cuadros estándar de
Windows salen siempre en blanco, así que no se usa ninguno. Las barras de desplazamiento de
las listas también se ponen oscuras.

**Tu agenda del día** (abajo a la derecha) se dibuja por secciones: **Pruebas** de ese día (con su peso o su nota),
**Clases** (las asignaturas que tienen clase ese día) y, si es hoy,
**Para estudiar hoy** (las pruebas de las próximas dos semanas, de la más urgente a la menos, y las tarjetas
pendientes). Cada fila lleva una franja del color de la asignatura y una pastilla («mañana», «09:00–11:00»,
«40 %»). Pulsa una fila para abrir esa asignatura; si no cabe todo, se desplaza con la rueda.

**Cuatrimestres y días de clase.** Cada asignatura tiene un cuatrimestre (1 o 2) y los días
en que se imparte (`L M X J V S D`). Eso decide en qué lista aparece y qué se ve en su
primera columna. El **calendario solo marca las pruebas evaluables**, nada más.

## El profesor

El botón **Estudiar con el Profesor** abre un tutor de la asignatura elegida que trabaja solo con su
carpeta **Temario** y una IA: Ollama en tu equipo por defecto (tus apuntes no salen del ordenador) u otro
servidor que elijas (ver [CONECTAR_IA.md](CONECTAR_IA.md)):

- **Plan para hoy** según el calendario de pruebas, tus tarjetas, tus fallos y tus temas flojos, con
  la cuenta atrás hasta la próxima prueba.
- **Tests** rápidos sin IA, redactados por la IA o mezclando ambos, con modo examen, penalización,
  tiempo límite y **simulacro de examen**.
- **Tarjetas de memoria** con repetición espaciada y trucos de la IA para recordarlas.
- **Resúmenes por tema** (ideas clave, conceptos y preguntas de examen) escritos por la IA.
- **Preguntas de desarrollo** que la IA redacta y te corrige: nota, ideas clave cubiertas, errores
  y respuesta modelo.
- **Dudas**: chat que explica con tus apuntes citando las fuentes, y se guarda entre sesiones.
- **Progreso** con un **análisis del profesor** y un plan hasta el examen, y **Mis fallos**.
- **Buscador y glosario** de todos tus apuntes, **mapas mentales** de cada tema, **calendario**
  hasta el examen con **nota estimada**, temporizador **pomodoro**, **niveles y logros**, y
  exportación a un **dossier** imprimible o a **Anki**. Se cambia de asignatura sin cerrar la ventana.

Sin IA funcionan los tests rápidos, las tarjetas, los resúmenes automáticos, el lector y el resto de
herramientas; lo que es preguntar al profesor (Dudas, Explícame…) necesita la IA y se oculta. La tarjeta de cada asignatura de la
ventana principal muestra tu estudio («Estudio: 31 % de aciertos · 3 tests · racha 1 día…»).
Todo se explica en [`PROFESOR.md`](PROFESOR.md).

`gestor_gui.exe --profesor IA` abre directamente el profesor de la asignatura `IA`.

## Personalización

El **engranaje** abre la ventana de *Ajustes y personalización* (también `gestor_gui.exe --ajustes`).
Los cambios se ven al momento en la propia ventana, que trae una vista previa. «Guardar» los aplica a todo
el programa (ventana principal y Profesor) y «Cancelar» o `Esc` deja todo como estaba.

| Sección | Ajustes |
|---|---|
| **Apariencia** | **Estilos** de un clic que lo cambian todo a la vez (Universalis, Aurora, Medianoche, Bosque, Atardecer, OLED, Papel, Minimal, Océano y Pizarra) · modo oscuro, claro o **como Windows** (cambia solo con el sistema) · 11 **paletas** de fondo (Grafito, Medianoche, Océano, Bosque, Ciruela, Vino, Arena, Carbón —negro puro para pantallas OLED—, Sepia, Pizarra y Rosa) · **color de acento** (14 colores u otro cualquiera) · color de la **barra superior** · **fondo** liso o con halos de color suaves, normales o intensos · **esquinas** rectas, suaves, redondeadas o muy redondeadas · efecto **cristal** (brillo y sombra) · **más contraste** en los textos |
| **Texto y tamaño** | Tamaño de la interfaz (100–150 %) · tamaño de la letra (de pequeña a enorme) · **tipo de letra** (la de Windows, Segoe UI, Bahnschrift, Calibri, Verdana, Georgia, Consolas…; cada una se ve escrita con su letra) |
| **Ventanas** | Animaciones al cambiar de pantalla · ancho del contenido (normal, amplio o todo el ancho) |
| **Calendario y avisos** | Nombre del curso · semana desde el lunes · ver fin de semana · avisar de las pruebas a menos de N días · nota objetivo · ordenar asignaturas por día · preguntar antes de borrar |
| **Estudio** | Minutos de concentración y de descanso del temporizador · poner música al concentrarse · **campanilla** al acabar cada bloque · **pantalla de descanso** con respiración guiada · **objetivo diario** de concentración (30 min a 4 h) |
| **Datos y ayuda** | Abrir las carpetas del programa, Asignaturas y datos · **copia de seguridad** fechada en `datos/copias/` · atajos · restablecer la apariencia |

## Música para estudiar y modo enfoque

En la barra del Profesor, el botón **Música** (o `Ctrl+M` para encenderla y apagarla) ofrece varios ambientes
que el propio programa genera, sin descargar nada: **lluvia**, **olas del mar**, **ruido marrón**,
**chimenea**, **piano tranquilo**, **ondas alfa** (binaurales, con cascos), **tormenta** (lluvia fuerte y truenos
lejanos), **bosque** (viento y pájaros), **cafetería** (murmullo y tazas) y **arroyo**. Todos se repiten sin cortes
y suenan bajitos, de fondo. Mientras suena uno, el fondo del Profesor lo acompaña con un **efecto visual sutil**:
lluvia que cae, relámpagos lejanos, olas abajo, polvo que flota, brasas que suben, luces suaves, ondas, hojas
que caen o destellos de agua (se quitan en Ajustes → Estudio).
**Mi música** reproduce en orden aleatorio los mp3, wav, wma o m4a que copies en la carpeta `Musica/` del
programa. El volumen y el último ambiente se recuerdan, y en Ajustes puedes hacer que la música empiece sola
con el temporizador de concentración.

La **pantalla completa** y el **modo enfoque** no se quitan con `Esc` (así no se salen sin querer): solo con su
botón **Salir** (arriba) o con `F11` / `F9`.

El botón **Enfoque** (o `F9`) pone el **modo enfoque**: pantalla completa sin barra ni menú lateral, con el
contenido centrado y más estrecho, el fondo apagado y sin avisos. Arriba queda una cápsula con la hora, el
temporizador («Concentrarme 25 min»), la música y «Salir». Si no mueves el ratón, la cápsula se reduce a
una pastilla discreta. «Salir» o `F9` lo quitan. Si pones un **objetivo diario** en Ajustes, un anillo junto a la hora
muestra cuánto llevas («35/60 min»).

**Descansos guiados.** Al acabar un bloque de concentración suena una campanilla suave (por encima de la música)
y se abre la **pantalla de descanso**: la cuenta atrás, un círculo que respira contigo (4 s inspirar, 2 s
mantener, 6 s soltar), un consejo para despejarte y lo que llevas estudiado hoy frente a tu objetivo.
«Volver a estudiar ya» empieza otro bloque; `Esc` la oculta y el descanso sigue en la barra. Al terminar el
descanso suena otra campanilla. En **Progreso** hay una gráfica con tus minutos de los últimos 14 días y la línea
del objetivo.

## Leer el temario en el Profesor

La pantalla **Temario** del Profesor deja leer tus apuntes sin salir: a la izquierda los documentos por temas
(con una marca en los que ya has leído) y a la derecha el documento elegido página a página, con letra cómoda.
Al llegar al final queda **marcado como leído** (o a mano). **Pantalla completa** lo abre sin barra ni lateral,
con la columna de lectura centrada; al final hay botones para pasar al documento siguiente o al anterior, y
arriba «Pantalla completa», «Solo el texto» / «Ver como el PDF», «Ocultar lista» y «Abrir el original». Los
tests del tema están en el menú «Test» de la barra de abajo. Desde la paleta: «leer dilemas».

Recuerda **por dónde vas** en cada documento: al volver a abrirlo te lleva a ese punto, la lista muestra un
anillo con lo que llevas («vas por el 40 %») y el Inicio del Profesor ofrece **Seguir leyendo**. Mientras lees,
una franja fija arriba dice el documento, la página y el porcentaje. La barra de abajo tiene «‹» / «›» para
saltar de página y el **tamaño** («−» / «+» o las teclas `-` y `+`: las páginas del PDF del 50 al 300 %, o la
letra en la vista de texto); pulsando el propio valor, o `0`, vuelve al de inicio. La **lista de documentos**
se queda fija a la izquierda con su propio desplazamiento (la rueda encima la mueve a ella) y la barra de la
derecha se puede arrastrar. Teclas: `Espacio` avanza una pantalla, `RePág` / `AvPág` saltan de página,
`←` / `→` cambian de documento, `Inicio` / `Fin`, `T` (PDF o solo texto) y `L` para la lista. Más detalles en
[PROFESOR.md](PROFESOR.md).

## Paleta de comandos

En el Profesor, `Ctrl+K` (o la caja «Buscar o ir a…» de la barra) abre la **paleta de comandos**: escribe unas
letras y salta a cualquier sitio. Encuentra pantallas, acciones (simulacro, repaso, test rápido, test de tus
fallos, concentración, descanso, modo enfoque, exportar…), **cada tema de tus apuntes** («resu dilemas» abre el
resumen de *Dilemas IA*; «test sesgo» hace un test solo de ese tema), los ambientes de música y las otras
asignaturas. No distingue mayúsculas ni tildes. `↑` `↓` para moverse, `Intro` para abrir y `Esc` para cerrar.
`F1` enseña todos los atajos del Profesor.

## Atajos de teclado

| Atajo | Acción |
|---|---|
| `Ctrl+N` | Nueva asignatura |
| `Ctrl+D` | Duplicar asignatura |
| `F2` | Editar asignatura |
| `Ctrl+P` | Nueva prueba |
| `F3` | Editar prueba |
| `Espacio` | Poner nota / marcar como pendiente |
| `Supr` | Borrar la prueba elegida |
| `Ctrl+E` | Exportar informe HTML |
| `Ctrl+T` | Tema claro / oscuro |
| `Ctrl+S` | Guardar ahora |
| `F9` (en el Profesor) | Modo enfoque |
| `Ctrl+M` (en el Profesor) | Música sí / no |
| `Ctrl+K` (en el Profesor) | Paleta de comandos: ir a cualquier pantalla, tema o acción |
| `F1` (en el Profesor) | Todos los atajos del Profesor |
| `F5` | Recargar la vista |
| `F1` | Guía rápida |

## La barra superior

Cuatro botones con icono, con su ayuda emergente al pasar el ratón:

| Icono | Qué hace |
|---|---|
| Bandeja con flecha | Exporta el informe HTML |
| Paleta | Alterna tema claro / oscuro |
| Engranaje | Ajustes y personalización (ver abajo) |
| Interrogación | Guía rápida (`F1`) |

**Escala de calificaciones.** Cada nota se traduce a su nombre y su color:

| Nota | Calificación | Color |
|---|---|---|
| menos de 5 | Suspenso | rojo |
| 5 – 5,9 | Aprobado | naranja |
| 6 – 6,9 | Bien | amarillo |
| 7 – 8,9 | Notable | verde |
| 9 – 10 | Sobresaliente | azul |

Aparece en la tarjeta de resumen (pastilla junto a la nota media), en la columna
*Calificación* de las dos tablas, en el pie con la media del expediente y en el informe HTML.

**Modo oscuro.** La interfaz arranca en oscuro; el botón *Tema claro / Tema oscuro* alterna
y la preferencia se guarda en el JSON. Fondos, listas, cabeceras de tabla y botones están
dibujados por el programa, así que el tema se aplica de verdad a todo.

**Calendario propio.** No es el control de Windows, está dibujado a mano para poder colorear
días sueltos:

- Cada asignatura tiene un **color**; los días con prueba se pintan con un fondo suave de ese
  color y, si hay sitio, con el **nombre de la prueba dentro de la celda**.
- Los días de los meses vecinos se ven atenuados, y el fin de semana en un tono más suave.
- El **día de hoy** lleva un borde de acento; el **día seleccionado**, un recuadro relleno.
- Flechas `<` `>`, botón **Hoy**, rueda del ratón o flechas del teclado para cambiar de mes.
- La celda bajo el ratón se resalta.
- Al **pulsar una prueba** en la tabla, el calendario salta a su mes y marca ese día.
- Al **pulsar un día con prueba**, se selecciona automáticamente esa asignatura y esa prueba.

Otros detalles: doble clic sobre una fila la abre para editarla, y todo se guarda solo tras
cada cambio, en el mismo `datos/asignaturas.json` que usa la versión de consola.

## Dónde va el material de cada asignatura

Las carpetas de temario y prácticas viven **dentro de esta misma carpeta**:

```
Universalis/
└── Asignaturas/
    ├── PROG2/
    │   ├── Temario/
    │   └── Practicas/
    └── BBDD/
        ├── Temario/
        └── Practicas/
```

Se crean **solas al arrancar el programa**, para todas las asignaturas de golpe, y también al
crear una asignatura nueva. Los tres botones bajo la lista abren cada nivel: *Carpeta* (la de
la asignatura), *Temario* y *Prácticas*.

Así puedes mover o copiar toda la carpeta del programa a otro ordenador y seguir teniendo
apuntes, notas y programa juntos.

Si renombras el código de una asignatura, se crea la carpeta nueva y la antigua se queda
donde estaba: mueve tú el contenido si te interesa.

## Compilar

**Opción rápida (Windows):** doble clic en `compilar.bat`. Busca `g++` (en el PATH o en la
carpeta de Code::Blocks) y genera los dos ejecutables.

**Code::Blocks:** abre `GestorAsignaturas.cbp`, elige el target y pulsa F9. Detalles abajo.

**A mano con g++** (ojo: no vale `src\*.cpp`, porque `main.cpp` y `GuiMain.cpp` tienen cada
uno su punto de entrada):

```bat
set COMUNES=src\Json.cpp src\Fecha.cpp src\Modelo.cpp src\Rutas.cpp src\Almacen.cpp src\Utilidades.cpp src\Calendario.cpp src\ExportadorHtml.cpp

g++ -std=c++17 -O2 -Iinclude %COMUNES% src\main.cpp -o gestor_asignaturas.exe -static -lshell32

windres gestor.rc -O coff -o gestor_res.o
g++ -std=c++17 -O2 -Wall -Iinclude %COMUNES% src\GuiMain.cpp src\GuiProfesor.cpp src\GuiProfesorEstudio.cpp src\GuiProfesorHerramientas.cpp src\GuiAjustes.cpp src\GuiAgenda.cpp src\GuiProfesorEnfoque.cpp src\GuiProfesorComandos.cpp src\GuiProfesorTemario.cpp src\GuiProfesorAmbiente.cpp src\Musica.cpp src\ProfesorMotor.cpp src\GuiUtil.cpp src\GuiCalendario.cpp gestor_res.o -o gestor_gui.exe -mwindows -static -static-libgcc -static-libstdc++ -lgdiplus -lcomctl32 -lcomdlg32 -lshell32 -lole32 -lgdi32 -luser32 -lmsimg32 -ladvapi32 -lwinmm
```

(Es lo mismo que hace `compilar.bat`; si cambia la lista de archivos, la de referencia es la suya.)

**Con CMake / Visual Studio:**

```bat
cmake -S . -B build
cmake --build build --config Release
```

El ejecutable se genera en la raíz del proyecto para que encuentre la carpeta `datos/`.

### Abrir el proyecto en Code::Blocks

1. `File > Open...` y elige `GestorAsignaturas.cbp`.
2. El proyecto tiene **dos targets**. Elige cuál compilar en la lista desplegable de la barra
   de herramientas (o en `Build > Select target`):
   - **Interfaz grafica** → `gestor_gui.exe`
   - **Consola** → `gestor_asignaturas.exe`
3. F9 para compilar y ejecutar. Los `.exe` salen en la raíz del proyecto, junto al `.cbp`.

Detalles de configuración que ya vienen puestos, por si tocas algo:

- Cada `.cpp` está marcado como `CPP` (si se marca como `CC`, Code::Blocks enlaza con `gcc`
  y salen cientos de `undefined reference to std::...`).
- El target gráfico es de tipo *GUI application* (`-mwindows`) y enlaza
  `gdiplus`, `comctl32`, `comdlg32`, `shell32`, `ole32`, `gdi32`, `user32`, `msimg32`, `advapi32` y `winmm`.
- `gestor.rc` incrusta el manifiesto (estilos visuales modernos) y el icono `gestor.ico`.
  Si tu MinGW no trae `windres`, quita ese fichero del proyecto y seguirá funcionando, solo
  sin icono ni estilo moderno.
- El enlazado usa **`-static`**. Sin él, el `.exe` pide `libwinpthread-1.dll` al abrirlo fuera
  del IDE y Windows da un error de "no se encontró el DLL".

## Cambiar el nombre de la carpeta

El programa no depende del nombre de la carpeta: todas las rutas se calculan a partir de
dónde está el `.exe`. Puedes renombrarla desde el Explorador (F2) y todo sigue funcionando. Después vuelve a ejecutar `crear_acceso_directo.bat` para que el
icono del Escritorio apunte a la ruta nueva.

## Ejecutar

```bat
gestor_asignaturas.exe                 REM usa datos\asignaturas.json
gestor_asignaturas.exe otro_curso.json REM usa otro fichero
```

En el primer arranque, si no existe el fichero de datos, se carga un curso de ejemplo con
tres asignaturas para que veas cómo funciona. Todo se guarda automáticamente al salir y
después de cada cambio.

## Qué hace cada pantalla

| Opción | Contenido |
|---|---|
| **1. Calendario** | Mes en ASCII con los días de examen marcados `[dd]` y hoy `>dd<`. `a`/`s` para moverte de mes, `h` para volver a hoy. Debajo, el detalle de los exámenes del mes. |
| **2. Mis asignaturas** | Tabla con media actual, % evaluado y próximo examen. Entra en una para ver su ficha. |
| **3. Resumen** | Media del expediente, próximos exámenes y carga pendiente por asignatura. |
| **4. Informe HTML** | Genera `datos/informe.html` con calendarios, tarjetas por asignatura y enlaces a las carpetas. Se abre en el navegador. |

Dentro de la ficha de una asignatura puedes añadir pruebas, poner notas, editarlas o
borrarlas, abrir la carpeta de temario o de prácticas (opciones 5 y 6) y editar los datos
de la asignatura, incluidas esas rutas.

## Cómo se calculan las notas

Cada prueba tiene una **ponderación** (% sobre la nota final de la asignatura):

- **Nota media actual** = media ponderada *solo de lo ya calificado*.
  Es lo que llevas de media, ignorando lo que aún no has hecho.
- **Nota acumulada** = puntos sobre 10 que ya tienes asegurados del total del curso
  (`Σ nota × ponderación / 100`).
Ejemplo: Parcial 1 (25 %) con 7,5 y Práctica (15 %) con 9 →
media actual `(7,5·25 + 9·15) / 40 = 8,06`; acumulada `1,875 + 1,35 = 3,23` puntos.

Las pruebas se guardan **sin hora**: solo la fecha. Si necesitas anotar el aula o la hora,
usa el campo *Qué entra / notas*.

## Estructura del código

```
Universalis/
├── Gestor de Asignaturas.bat   lanzador de la version grafica
├── Gestor consola.bat          lanzador de la version de texto
├── crear_acceso_directo.bat    crea el icono en el Escritorio (ejecutar una vez)
├── compilar.bat                compila los dos ejecutables con g++
├── diagnostico.bat             dice que compilador tienes y que falta
├── GestorAsignaturas.cbp       proyecto de Code::Blocks (2 targets)
├── CMakeLists.txt              alternativa con CMake / Visual Studio
├── gestor.rc / gestor.manifest  estilos visuales modernos de la GUI
├── probar.bat                  pruebas automaticas del profesor
├── PROFESOR.md                 guia del profesor (estudio con IA)
├── CONECTAR_IA.md              como conectar tu propio modelo de IA
├── ia.ejemplo.json             ejemplo de datos/ia.json
├── NOVEDADES.md                lo ultimo que ha cambiado
├── profesor/                   motor del profesor en Python (y sus pruebas en profesor/pruebas)
├── pruebas/test_musica.cpp     pruebas de la musica (las compila compilar.bat)
├── assets/                     retrato del profesor e iconos
├── Asignaturas/<CODIGO>/       temario y practicas de cada asignatura (se crea solo)
├── Musica/                     tus pistas de musica de fondo (opcional)
├── datos/asignaturas.json      tus datos (se crea solo al primer arranque)
├── datos/ia.json               que IA usa el profesor (opcional, ver CONECTAR_IA.md)
├── include/                    cabeceras
└── src/                        implementación
```

| Módulo | Responsabilidad | Consola | GUI |
|---|---|:---:|:---:|
| `Json` | Parser y serializador JSON mínimos | x | x |
| `Fecha` | Fecha/hora, día de la semana, días restantes | x | x |
| `Modelo` | `Examen`, `Asignatura`, `Curso` y los cálculos de notas | x | x |
| `Rutas` | Ficheros y carpetas sin `<filesystem>` | x | x |
| `Almacen` | Carga/guardado JSON con escritura atómica | x | x |
| `Utilidades` | Consola UTF-8, color, entrada validada | x | |
| `Calendario` | Vista mensual de texto y próximos exámenes | x | x |
| `ExportadorHtml` | Informe HTML autocontenido | x | x |
| `main` | Menús de la versión de consola | x | |
| `GuiUtil` | Tema, listas, botones, iconos y diálogos dibujados | | x |
| `GuiCalendario` | Control de calendario propio con colores por asignatura | | x |
| `GuiMain` | Ventana principal y acciones | | x |
| `GuiProfesor` | Ventana del profesor: navegación, tests, resultados, progreso, dudas | | x |
| `GuiProfesorEstudio` | Inicio, tarjetas, resúmenes, desarrollo, iconos y texto con formato | | x |
| `GuiProfesorHerramientas` | Buscador, glosario, mapa mental, calendario, logros, pomodoro, exportar | | x |
| `GuiAjustes` | Ventana de Ajustes y personalización (estilos de serie, paletas, letra, esquinas...) | | x |
| `GuiProfesorEnfoque` | Modo enfoque, panel de música y pantalla de descanso del Profesor | | x |
| `GuiProfesorComandos` | Paleta de comandos (Ctrl+K) y ayuda de atajos (F1) del Profesor | | x |
| `GuiProfesorTemario` | Lector del temario: selección para preguntar, tests de lo que lees, panel del profesor | | x |
| `GuiProfesorAmbiente` | Efectos visuales de la música de fondo (lluvia, olas, brasas…) | | x |
| `GuiAgenda` | «Tu agenda del día» de la ventana principal, dibujada por secciones | | x |
| `Musica` | Ambientes sintetizados (lluvia, olas, piano, tormenta, bosque...), campanilla y «Mi música» | | x |
| `ProfesorMotor` | Arranca `profesor/servidor.py` y habla con él por tuberías | | x |
| `profesor/*.py` | Motor del profesor en Python (ver `PROFESOR.md`) | | x |

La lógica de notas está en `Modelo`, separada de las dos interfaces: si cambias una fórmula,
cambia en ambas versiones a la vez.

El formato de `datos/asignaturas.json` es texto plano legible: puedes editarlo a mano o
versionarlo con git.

## Ideas para ampliarlo

- Recordatorios del sistema (notificaciones de Windows) para las pruebas y tareas cercanas.
- Adjuntar ficheros concretos (apuntes de un tema) y no solo carpetas.
- Exportar a CSV para abrir las notas en Excel.
