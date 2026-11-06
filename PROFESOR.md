# Universalis · El profesor

Botón **Estudiar con el Profesor** de la ventana principal (o `gestor_gui.exe --profesor CODIGO` para
abrirlo directamente). Es tu tutor de la asignatura elegida: te prepara un plan para cada día, te
hace tests, tarjetas de memoria, resúmenes y preguntas de desarrollo, te corrige y resuelve tus
dudas. Todo sale **solo de su carpeta Temario** (nunca de Prácticas). La IA es Ollama en tu propio
equipo, o el servidor que elijas (ver *La IA* y [CONECTAR_IA.md](CONECTAR_IA.md)). Sin IA funcionan los tests
rápidos, las tarjetas, los resúmenes automáticos y el lector; lo que es preguntar al profesor (Dudas,
Explícame, Preguntar…) necesita la IA y, sin ella, no aparece.

## La ventana

Se abre maximizada y sin la barra de título de Windows (las pantallas cambian con un fundido suave).

- **Barra de arriba**: **←** para volver a la pantalla anterior (también `Retroceso`, `Alt+←` o el
  botón «atrás» del ratón), el nombre de la pantalla, el **estado de la IA** y los botones de
  pantalla completa (`F11`; `Esc` no la quita), minimizar, maximizar y cerrar. La barra se arrastra
  para mover la ventana y con doble clic se maximiza/restaura.
- **Estado de la IA**: «IA lista», «Cargando…» o lo que está haciendo ahora mismo («Escribiendo un
  resumen · 1:23», «Corrigiendo tu respuesta»…). Pulsándolo vas a esa pantalla. Si Ollama está
  instalado pero cerrado, pone «IA apagada · Iniciar»; si usas otro servidor y no contesta, «IA sin conexión ·
  Reintentar»; sin nada instalado, «Sin IA · modo rápido».
- **Concentración**: un temporizador pomodoro (ver *Temporizador*), a la izquierda del estado de la IA.
- **Buscar o ir a…** (`Ctrl+K`): la **paleta de comandos**. Escribe unas letras y aparece todo lo que coincide:
  pantallas, acciones (simulacro, repaso, test rápido de 10, test de tus fallos, concentración, descanso, modo
  enfoque, exportar, ordenar los temas con la IA…), **un test o el resumen de cada tema** de tus apuntes, los
  ambientes de música y las otras asignaturas. Sin distinguir mayúsculas ni tildes; `↑` `↓`, `Intro` y `Esc`.
  `F1` enseña todos los atajos del profesor.
- **Lateral**: el profesor con su bocadillo (te va comentando lo que haces) y el menú **Inicio ·
  Nuevo test · Test en curso · Resultados · Tarjetas · Resúmenes · Temario · Desarrollo · Buscar y glosario ·
  Dudas · Progreso · Mis fallos** («Test en curso» y «Resultados» solo con un test abierto; «Dudas», solo con
  IA). `Alt+1…9` van a cada opción en el orden en que se ven y `Alt+0` a «Mis fallos». Pulsando el nombre de la asignatura (bajo el profesor) puedes
  **cambiar de asignatura** sin cerrar la ventana (salen las que tienen algo en su Temario). Cada opción lleva su cifra (tareas pendientes del plan, tarjetas para hoy,
  preguntas en «Mis fallos», nota del último test…) o un indicador girando si la IA está
  trabajando en esa pantalla. Abajo, **Tu situación**: la próxima prueba del calendario y el test
  en curso o tu precisión, racha y fallos.

Puedes salir de cualquier cosa a medias: el test queda guardado y las tareas de la IA siguen en
segundo plano mientras usas otra pantalla.

## Inicio

- **Portada**: el profesor, la próxima prueba evaluable (del calendario del gestor) con una cuenta
  atrás en días y lo que pesa en la nota, y tus cifras: aciertos, racha, **nota estimada** y
  **nivel** (con lo que te falta para el siguiente).
- **Tu plan para hoy**: de 3 a 5 tareas concretas, de más a menos urgente, cada una con su botón
  *Empezar* y marcada como hecha cuando la cumples:
  tarjetas que toca repasar (o 10 nuevas), preguntas falladas cuyo repaso toca hoy, tu tema más
  flojo, un tema que aún no has tocado, y un **simulacro** si el examen está a 5 días o menos (si
  no, una pregunta de desarrollo).
- **Camino al examen**: tus temas repartidos por días hasta la próxima prueba, los más flojos
  primero, con un simulacro el día antes. Pulsando un día haces un test de sus temas.
- **Nota estimada si el examen fuera hoy**, a partir de tus aciertos en cada tema (ponderados por
  sus páginas; los temas sin practicar cuentan como un 30 %), con un consejo.
- **Herramientas de estudio**: acceso a cada herramienta con su cifra del momento.

## Tests

1. Copia tus apuntes en `Asignaturas/<CODIGO>/Temario` (PDF, Word `.docx`, PowerPoint `.pptx`, TXT, MD o TEX; los
   `.doc` y `.ppt` antiguos no se leen;
   también en subcarpetas). El botón *Abrir carpeta* de *Nuevo test* te lleva allí.
2. Elige cómo quieres el test en **Nuevo test** y pulsa **Empezar test**.
3. Responde con el ratón o el teclado. Al corregir verás, **bajo cada opción, por qué es correcta o en
   qué falla** (en las de IA y en las rápidas de definiciones), la explicación, la frase exacta del
   apunte que la respalda y de qué documento y página sale (*Abrir el apunte* lo abre).
4. Si respondes sin estar seguro, marca antes **No estoy seguro** (tecla `S`). Si aciertas cuenta como
   acierto, pero la pregunta vuelve a repaso al día siguiente y entra en *Repetir fallos y dudas*; en los
   resultados se cuenta cuántas acertaste dudando.

**En pantalla completa o en el modo enfoque** no hace falta salir para hacer un test: el botón **Hacer un
test** de la cápsula de arriba (o `Ctrl+T` desde cualquier pantalla) abre un panel para seguir el test en
curso, hacer uno de todo el temario (eligiendo número de preguntas y modo ahí mismo), del tema que estás
leyendo, el repaso del día, tus fallos o un simulacro, o ir a configurarlo. Durante el test, la cápsula
muestra por dónde vas («Test 3/10»).

### Ajustes del test

| Ajuste | Opciones |
|---|---|
| **Modo** | *Rápido* (al instante, sin IA) · *Mixto* (empieza al instante y la IA va sustituyendo las preguntas que aún no has visto) · *Solo IA* (todas redactadas por la IA; las que falten se preparan mientras respondes) |
| Preguntas | 5, 10, 15, 20, 25 o 30 |
| Dificultad | Básica, Intermedia, Avanzada. Ni la básica copia frases del apunte; la avanzada pide casos nuevos, matices y razonar en varios pasos, con distractores casi correctos |
| Tipo | Variado, opción múltiple o verdadero/falso |
| Formatos | *Definiciones* (qué es un concepto, o qué concepto describe un texto reformulado), *Completar* (una frase **nueva**, no copiada del apunte, con un hueco donde encajan gramaticalmente las cuatro opciones), *Casos prácticos*, *Relacionar* (comparar o relacionar dos conceptos), *Razonamiento* (causas, consecuencias, «¿qué pasaría si…?») y *¿Cuál es falsa?*. Sin IA solo salen definiciones, huecos y «¿cuál es falsa?» |
| Corrección | *Al momento* (corrige cada pregunta) o *Al final* (modo examen: puedes cambiar respuestas y no ves nada hasta entregar) |
| Tiempo | Sin límite, 1 o 2 minutos por pregunta. Arriba a la derecha se ve lo que queda (en ámbar y luego rojo al final) y, al llegar a cero, el test se entrega solo |
| Restar fallos | Cada fallo resta 1/(opciones−1), como en los exámenes tipo test reales |
| Verificación doble | La IA revisa sus propias preguntas y descarta las que tengan más de una respuesta válida (más fiable, ≈ 40 % más lento) |
| Temas | Marca qué documentos del Temario entran |

Los ajustes se recuerdan por asignatura.

**Tests de todo el temario o de un tema.** Con un solo tema, la IA recorre ese apunte; con varios (o *Todos*),
reparte las preguntas entre todos los temas antes de repetir ninguno y, uno de cada tres lotes, junta trozos de
**dos temas distintos** para preguntar cómo se relacionan. Cada pregunta lleva su formato, su dificultad y su
tema en etiquetas de color, y en *Resultados* hay un desglose **Por tipo de pregunta** para ver si fallas más
en definiciones, en casos prácticos o al relacionar.

**Animaciones**: cada pregunta entra con sus opciones escalonadas, la respuesta buena lanza una onda y la
fallada tiembla, la corrección sube al aparecer, la racha de aciertos se ve arriba a la derecha y en los
resultados la nota y las barras crecen (con confeti a partir de 8,5). Se desactivan con las animaciones de
Ajustes.

**Simulacro de examen** (en *Nuevo test*, en *Inicio* y en el plan cuando se acerca la prueba):
20 preguntas de los temas marcados, corrección al final, los fallos restan y un minuto por
pregunta. No cambia tus ajustes guardados y su resultado sale marcado como simulacro.

### Teclado

| Tecla | Acción |
|---|---|
| `1`–`4` o `A`–`D` (`V`/`F` en verdadero/falso) | Elegir opción |
| `S` | No estoy seguro (antes de comprobar) |
| `Ctrl+T` | Hacer un test (panel, también en pantalla completa) |
| `Intro` | Comprobar · siguiente · ver resultados |
| `←` `→` | Pregunta anterior / siguiente |
| Rueda, `↑` `↓`, `RePág` `AvPág` | Desplazarse |

Pulsar dos veces la misma opción también la comprueba. Los puntos de colores de arriba llevan a
cualquier pregunta.

### Imagen de cada pregunta

Junto a cada pregunta aparece una figura del propio apunte: la gráfica o el diagrama de la
página de la que sale la pregunta, o el más cercano del mismo documento; si el documento no
tiene figuras, la propia diapositiva (y en los apuntes de solo texto, una portada del tema).
Pulsa la imagen para verla en grande (`Esc` o clic para cerrar). Se guardan en
`datos/profesor/imagenes`.

## Tarjetas

Tarjetas de memoria con **repetición espaciada**: conceptos del glosario de tus apuntes («La
curva ROC es…») y preguntas del banco. Lees la cara de delante, intentas recordar y la giras
(clic o `Espacio`); detrás está la respuesta, la frase del apunte, su fuente y su figura. Luego
te puntúas:

| Botón | Tecla | Vuelve |
|---|---|---|
| Otra vez | `1` | hoy mismo |
| Bien | `2` / `Intro` | según lo bien que la sepas: 1, 3, 7, 16 o 35 días |
| Fácil | `3` | salta un escalón más |

Primero salen las que toca repasar y luego las nuevas (10, 20, 30 o 50 por sesión, de los temas
marcados en *Nuevo test*). Con la tarjeta girada:

- **Truco para recordarla**: la IA escribe una regla mnemotécnica, analogía o ejemplo (≈ 25 s);
  se guarda para la próxima vez y puedes pedir otro.
- **Preguntar al profesor**: abre *Dudas* con la tarjeta como contexto.

Fuera de una sesión, **Tu mazo por cajas** enseña cuántas tarjetas hay nuevas y en cada caja de repaso
(hoy, 1 día, 3 días, 1 semana, 16 días, 5 semanas): cada acierto sube la tarjeta una caja y un fallo la
devuelve a la primera. Las de una semana o más cuentan como **dominadas**.

## Temas agrupados

Cada archivo del Temario es un **apartado**, y los apartados se agrupan en **temas** con un subtítulo
(«Tema 1 · Introducción a la inteligencia artificial»). Así salen en *Nuevo test*, donde un clic en el
subtítulo marca o desmarca el tema entero, y también en *Resúmenes*, *Desarrollo* y *Progreso*.
El número del tema sale del nombre del archivo (`1_2 IA como tendencia.pdf`, `Tema3.1 …`, `2.4. …`) o de su
carpeta. Los apartados sin número se juntan por carpeta o por un prefijo común. La primera vez que abres
una asignatura, la IA pone en segundo plano un **título a cada tema** y coloca los apartados sueltos (por
ejemplo, agrupa los exámenes de otros años). Tarda un minuto y se guarda mientras no cambien los archivos.
La lógica está en `profesor/temas.py`.

## Temario

Para **leer tus apuntes dentro del profesor**: la lista de documentos por temas (con ✓ en los leídos y la cuenta
«2/3» de cada tema) y el documento elegido. Los **PDF se ven tal cual**, como en un visor: cada página en una
hoja blanca con sus gráficas, figuras y fórmulas («Página 3 de 20» encima de cada una). Las páginas se pintan
a medida que llegas a ellas y se guardan en `datos/profesor/imagenes/paginas` (máx. ≈ 400 MB; se borran las
más antiguas). Con **Solo el texto** se lee el texto extraído, con letra ajustable; es también la única vista
de Word, PowerPoint y texto plano. Los PDF escaneados (sin texto) ahora también se pueden leer aquí.

Al llegar al final se marca como leído (también a mano, con la pastilla «Leído» / «Marcar como leído» de arriba a la derecha). **Pantalla completa** abre la
lectura en el modo enfoque, sin nada alrededor; «Salir de pantalla completa» vuelve. Al final del documento,
*Anterior* y *Siguiente*. «Abrir el original» lo abre con el programa de Windows.

El profesor recuerda **por dónde ibas** en cada documento (y vuelve ahí al abrirlo); los empezados llevan un
anillo en la lista y el Inicio te propone **Seguir leyendo**. Una franja fija arriba muestra la página y el
porcentaje leído.

**Barra de lectura** (flota abajo, también en pantalla completa): la página en la que estás con «‹» / «›» para
saltar a la anterior o la siguiente (también `RePág` / `AvPág`), el tamaño («−» / «+», `-` / `+` o `Ctrl` +
rueda), **Test**, **Resumen** del tema y, con IA, **Explícame esta página** y **Preguntar**. En la vista del PDF las
páginas van del 50 al **300 %**: por encima del 100 % se salen de la columna y se mueven de lado con `Mayús` +
rueda (o la rueda lateral del ratón o el panel táctil). Para volver al tamaño de inicio se pulsa el propio
valor (el porcentaje, o los px en la vista de texto; se resalta cuando no está en el de inicio) o `0`; no hay
un botón aparte.

**Desplazarse.** La barra de la derecha se puede **arrastrar** (o pulsar en su carril para saltar ahí), en el
lector y en todas las pantallas del profesor. La **lista de documentos** de la izquierda se queda fija mientras
bajas por el documento y tiene su propio desplazamiento: con el ratón encima, la rueda mueve la lista; fuera de
ella, el documento. Si la lista es larga, también tiene su barra arrastrable. En la vista de texto la letra llega a 34 px y las líneas se quedan en un
ancho cómodo de leer (unos 75 caracteres).

El menú **Test** reúne todo lo que puedes hacer con lo que lees, sin salir del lector: **test de lo que estás
leyendo** (la página actual y la siguiente, al momento y sin esperar a la IA), **test rápido del tema** (5
preguntas), **test del tema** (con tus ajustes), **simulacro del tema**, **repasar mis fallos del tema** y una
**pregunta de desarrollo** del tema.

**Seleccionar y preguntar.** Arrastra el ratón sobre el texto (palabra a palabra) o, en la vista del PDF, haz un
recuadro sobre la página: el profesor saca el texto de esa zona. Sobre la selección sale una barra con
**Preguntar al profesor** (con IA), **Test de esto** (preguntas sacadas solo de ese trozo, si es de un par de
frases o más), **Buscar en el temario** (si es corto) y **Copiar** (`Ctrl+C`). «Preguntar al profesor» abre un
**panel a la derecha**, sin dejar de ver el apunte, con preguntas preparadas (explícamelo, un ejemplo, cómo cae
en el examen…) y su propio campo; bajo cada respuesta, «Más sencillo», «Ponme un ejemplo» y «Ponme a prueba»,
como en Dudas; «En Dudas»
lleva la conversación al chat completo. `P` abre el panel para una pregunta libre y `Esc` quita la selección,
el menú o el panel.

Teclas: `Espacio` (Mayús: atrás), `←` / `→` documento anterior o siguiente, `Inicio` / `Fin`, `T` alterna
PDF / solo texto, `L` muestra u oculta la lista.

## Resúmenes

Un resumen por documento del Temario con **ideas clave**, **conceptos** (término: definición) y
**preguntas que podrían caer en el examen**. A la izquierda, la lista de temas con su estado (✦
resumen de la IA, punto: automático, ámbar: el apunte ha cambiado, círculo vacío: sin resumir) y
tu porcentaje de aciertos; a la derecha, el resumen con formato.

- **Resumir con IA**: la IA lee el apunte (≈ 1 min) y escribe el resumen mientras lo ves aparecer
  (≈ 1–2 min más). Queda guardado; *Rehacer* escribe otro.
- **Resumir todos con IA**: los prepara uno a uno en segundo plano; puedes seguir estudiando.
- **Resumen automático (sin IA)**: instantáneo, con las frases más representativas y las
  definiciones del apunte.
- *Test del tema*, *Abrir apunte* y *Copiar* (al portapapeles, sin formato, para tus apuntes).
- **Mapa mental** (la otra pestaña): el tema en el centro y sus ideas principales alrededor, cada
  rama con su color y sus detalles. El automático sale de los conceptos del apunte; *Mapa con IA*
  lo dibuja la IA (1–2 min) y queda guardado. Intenta reproducirlo de memoria en un papel.

## Buscar y glosario

- **Buscar en los apuntes**: escribe y, en cuanto dejas de teclear, salen los conceptos que
  coinciden y las frases de tus apuntes donde aparece lo buscado (resaltado), con su tema y página.
  Pulsando un resultado se abre el documento. Es instantáneo y no usa la IA.
- **Glosario**: todos los conceptos definidos en tu temario, por orden alfabético y con filtro por
  inicial o por texto. Crece solo: incluye la sección *Conceptos* de cada resumen que hagas.
- `Ctrl+F` abre el buscador desde cualquier pantalla del profesor.

## Desarrollo

Preguntas de examen escrito: te hago una pregunta, la respondes con tus palabras y te la
corrijo.

1. Elige **dificultad** (definir · relacionar · comparar y aplicar) y **tema** (o *Automático*:
   tus temas más flojos y menos trabajados).
2. **Nueva pregunta con IA** (la IA la redacta con el apunte delante: 1–2 min; las que aún no has
   respondido se reutilizan al instante) o **Rápida, sin IA** («¿Qué es…? Explícalo con tus
   palabras», a partir de las definiciones del apunte).
3. Escribe la respuesta (`Ctrl+Intro` para entregar; *Ver la solución* si no la sabes).
4. La corrección (≈ 1 min con IA) trae la **nota**, las **ideas clave** que has cubierto y las
   que faltan, los **errores** que has cometido, un **comentario** del profesor y la **respuesta
   modelo** con la frase del apunte y su página. Sin IA, la corrección es aproximada (por las
   palabras clave de cada idea).

Puedes repetir la misma pregunta sin mirar, pedir otra o preguntar una duda sobre ella. Tus
últimas respuestas y sus notas aparecen en la pantalla inicial de *Desarrollo*.

## Dudas

Un chat: escribes y pulsas `Intro` (`Mayús+Intro` para cambiar de línea). El profesor busca los
fragmentos más relevantes de tu Temario (también por el nombre de cada tema) y la IA te lo
explica citándolos, con **negritas**, listas y fuentes ([1], [2]…) que se abren con un clic. La
respuesta aparece mientras se escribe; el botón rojo la detiene.

- La conversación **se guarda** aunque cierres el profesor; *Nueva conversación* empieza de cero.
- Bajo la última respuesta: **Más sencillo · Ponme un ejemplo · Ponme a prueba · Resúmelo en 3
  puntos**. *Copiar* lleva cada respuesta al portapapeles.
- Con el chat vacío hay sugerencias sacadas de tu temario (tus temas flojos y sus conceptos).
- *No lo entiendo* (tras corregir una pregunta), *Explicar* (en «Mis fallos»), *Preguntar al
  profesor* (en una tarjeta) y *Tengo una duda* (en desarrollo) abren el chat con ese contexto;
  la etiqueta encima del campo lo indica (la ✕ lo quita).
- Sin IA, Dudas y las demás opciones de preguntar al profesor no aparecen.

## Progreso

Precisión, racha, tests y fallos; las notas de tus últimos tests; un **mapa de actividad** de las
últimas 8 semanas (cuenta tests, tarjetas y respuestas de desarrollo) y el **dominio por tema**,
con un botón *Practicar* para hacer un test solo de ese tema. Más abajo, junto al nivel, una gráfica con tus
**minutos de concentración de los últimos 14 días** y la línea de tu objetivo diario.

**Análisis del profesor**: *Analizar mi progreso* le pasa a la IA tus cifras, tus temas, tus
fallos y la próxima prueba, y escribe un diagnóstico (*Cómo vas*, *Puntos débiles*) y un **plan
concreto hasta el examen**, día a día (≈ 2 min). Se guarda hasta que lo actualices. Sin IA se
hace un análisis automático con los mismos datos.

## Temporizador (Concentración)

Un **pomodoro**: 25 minutos de estudio y 5 de descanso (se cambian en Ajustes → Estudio). Púlsalo para empezar, otra vez para
pausar o seguir, y la ✕ para pararlo (si llevabas 5 minutos o más, se guardan). Al terminar suena
una campanilla y empieza el descanso. Los minutos se suman a tu experiencia y se ven en *Progreso*.

**Pantalla de descanso**: al acabar cada bloque se abre un descanso guiado con la cuenta atrás, un círculo que
respira contigo (inspira 4 s, mantén 2 s, suelta 6 s), un consejo distinto cada poco (estírate, bebe agua, mira
lejos…) y lo estudiado hoy frente a tu **objetivo diario**. «Volver a estudiar ya» empieza otro bloque y `Esc` la
oculta (el descanso sigue en la barra). La campanilla, la pantalla de descanso y el objetivo se cambian en
Ajustes → Estudio.

## Nivel y logros

Cada pregunta, acierto, tarjeta, respuesta de desarrollo, test y minuto de estudio da experiencia
(XP) y subes de nivel. En *Progreso* están tu nivel, el tiempo de estudio y 14 **logros** (primer
test, rachas de 3 y 7 días, 100 y 500 preguntas, 80 % de aciertos, 50 tarjetas, 20 tarjetas
dominadas, 10 respuestas de desarrollo, una matrícula, un simulacro aprobado, todos los temas
resumidos, 5 horas de estudio y vaciar «Mis fallos»), cada uno con su progreso. Al conseguir uno
o subir de nivel aparece un aviso.

## Exportar

En *Progreso*, **Llévatelo contigo**:

- **Dossier de estudio**: una página HTML con la nota estimada, el calendario hasta el examen,
  todos tus resúmenes, el glosario y tus fallos con su respuesta. Se abre en el navegador, desde
  donde puedes imprimirla o guardarla en PDF (`Ctrl+P`).
- **Tarjetas a Anki**: un fichero con todas las tarjetas (separadas por tabuladores y con etiquetas
  por tema) para importarlo en [Anki](https://apps.ankiweb.net) y repasar en el móvil
  (*Archivo › Importar*).

Se guardan en `datos/exportes/`.

## Mis fallos

Cada pregunta que fallas se guarda sola en **Mis fallos** (se puede desactivar en las opciones de
*Nuevo test*); también puedes guardarla o quitarla a mano con el botón de la corrección. Ves de
qué tema es cada una, cuántas veces la has fallado y su respuesta, y puedes:

- hacer un **test solo con tus fallos** (5, 10, 20 o todas), también desde los resultados;
- en los resultados de cualquier test, **Repetir las falladas** vuelve a preguntarte al momento solo las que
  fallaste o dejaste en blanco en ese test;
- pedir al profesor que te la **explique**;
- quitar una pregunta concreta, o al terminar un test de fallos, **quitar de golpe las que has
  acertado**.

Además, cada pregunta fallada vuelve a aparecer en tests posteriores (repetición espaciada: 1, 3,
7, 16 y 35 días) y el plan del día te propone repasarlas cuando toca. *Pregunta errónea* elimina
para siempre una pregunta mal planteada.

Al corregir una pregunta, los botones dependen de cómo te ha ido: si la **fallas**, *Guardar en mis
fallos* (si no se guardó sola) y *No lo entiendo*; si la **aciertas**, *Explícamela más* y, solo si
estaba en «Mis fallos», *Quitar de mis fallos*. Una pregunta acertada nunca te ofrece guardarse como fallo.

## La IA

Por defecto las funciones de IA usan [Ollama](https://ollama.com) en tu propio equipo y nada sale de tu
ordenador. También se puede conectar cualquier servidor compatible con la API de OpenAI (LM Studio,
llama.cpp, vLLM, OpenAI, OpenRouter…) con el archivo `datos/ia.json`: todo está en
[CONECTAR_IA.md](CONECTAR_IA.md). Con un servicio en Internet, los fragmentos de tus apuntes que hacen falta
para cada pregunta se envían a ese servicio. Se elige solo el mejor modelo instalado (preferencia: `gemma4`, `gemma3`, `qwen3`,
`llama3.1`…); si tienes varios, puedes elegir en *Modelo IA* (*Nuevo test*). El modelo se carga
al abrir el profesor y se queda cargado 30 minutos para no pagar los ≈ 35 s de carga cada vez.

**Velocidad.** En CPU lo que más tarda es *leer* lo que se le pasa a la IA (≈ 25 tokens por segundo
con textos largos) antes de escribir la primera palabra, así que el profesor:

- usa **siempre las mismas opciones** con Ollama (`num_ctx` 8192 y los hilos del procesador menos dos):
  si cambian de una llamada a otra, Ollama vuelve a cargar el modelo (más de un minuto). Antes las dudas
  y el resto usaban contextos distintos y cada cambio costaba esa recarga. Los hilos se pueden fijar con
  la variable de entorno `UNIVERSALIS_HILOS`;
- le pasa **solo lo relevante**: de cada fragmento del temario, las frases que tienen que ver con la
  pregunta (≈ 900 caracteres, no la página entera); las páginas que son tablas de números o portadas
  cuentan menos, y en una duda sobre un trozo de los apuntes no se repite ese mismo texto;
- busca por **raíces** («validaciones», «validación» y «validar» se encuentran entre sí) y sin depender
  de tildes;
- pide respuestas **directas**, sin saludos ni despedidas.

Si Ollama se ha cerrado (a veces lo hace solo), lo que pides se reintenta una vez tras arrancarlo; y si al
abrir el profesor tarda en contestar porque está ocupado, se le da más margen antes de decir «IA apagada».

Con todo ello, la primera palabra de una duda llega en ≈ 15–40 s (antes, 35–75 s). Mientras tanto, la
burbuja dice en qué está y lleva un cronómetro.

Tiempos medidos en este portátil (i7-1355U, sin GPU, `gemma4`), que escribe ≈ 5 tokens por segundo:

| Tarea | Tiempo aproximado |
|---|---|
| Lote de 3 preguntas tipo test (formatos exigentes) | 2,5–4 min; cada pregunta entra en el test en cuanto está escrita (la primera, ≈ 1,5 min) |
| Truco para una tarjeta | 25 s |
| Corregir una respuesta de desarrollo | 1–1,5 min |
| Redactar una pregunta de desarrollo | 1–2,5 min |
| Resumen de un tema | 2–2,5 min |
| Análisis del progreso | ≈ 2 min |
| Mapa mental de un tema | 1–2 min |
| Respuesta a una duda | 0,5–2 min (la primera palabra, en 15–40 s) |

Por eso todo lo que escribe la IA **aparece mientras se escribe**, **se guarda** (preguntas,
resúmenes, trucos, análisis) y se puede **cancelar**.

**Reserva de preguntas.** Cuando la IA lleva un rato libre, el profesor va dejando preparadas en segundo
plano preguntas de IA de cada tema (4 sin estrenar por tema, con tu dificultad y tus formatos), para que el
siguiente test en modo Mixto o Solo IA empiece ya con preguntas de IA en vez de huecos rápidos. La barra de
arriba lo indica («Preparando preguntas · tema») y *Nuevo test* cuenta las que hay «sin estrenar». En cuanto
pides algo a la IA (una duda, un resumen, un test), la reserva se corta al instante y sigue después. Se
desactiva con *Ir preparando preguntas de IA en segundo plano* (opciones de *Nuevo test*). Con el portátil
**desenchufado** la reserva se pone en pausa para no gastar la batería (la barra lo indica) y sigue al
enchufarlo; la variable de entorno `UNIVERSALIS_RESERVA_CON_BATERIA=1` la deja trabajar igualmente. Varias tareas a la vez hacen cola en
Ollama, así que tardan más.

Sin IA: los tests rápidos se construyen con las frases del apunte (definiciones en los dos sentidos,
«¿cuál es falsa?», huecos y verdadero/falso), los resúmenes y las preguntas de desarrollo salen de las definiciones y las
frases más representativas, y la corrección se hace por palabras clave.

## Cómo funciona por dentro

```
gestor_gui.exe ──tuberías (JSON por líneas)──► python profesor/servidor.py
 GuiProfesor.cpp        ventana, menú, tests,        motor.py      tests, corrección, progreso, dudas
                        progreso, dudas, fallos      estudio.py    plan, resúmenes, desarrollo,
 GuiProfesorEstudio.cpp inicio, tarjetas, resúmenes,               análisis, trucos, conversación
                        desarrollo, texto con formato texto.py     lectura y limpieza del Temario
 GuiProfesorHerramientas.cpp buscador, mapa mental,  herramientas.py buscador, glosario, mapas,
                        calendario, logros, pomodoro,              calendario, nota estimada, logros,
                        exportar, cambio de asignatura             tiempo de estudio, exportación
 ProfesorUi.hpp         piezas comunes de la ventana  generador.py preguntas instantáneas sin IA
 GuiProfesorTemario.cpp lector del Temario          temas.py      agrupado de los apuntes en temas
 GuiProfesorEnfoque/Comandos/Ambiente.cpp  enfoque,
                        paleta y atajos, ambientes
 ProfesorMotor.cpp      proceso + hilo lector        ia.py         la IA (Ollama u OpenAI compatible, según
                                                                   datos/ia.json): preguntas, dudas, resúmenes,
                                                                   corrección, análisis, trucos
                                                     imagenes.py   figura del apunte para cada pregunta
                                                     banco.py      SQLite: datos/profesor/profesor.sqlite
```

- El proceso Python se arranca una vez por ventana y recibe una orden por línea. Las órdenes que
  esperan a la IA (`duda`, `resumen`, `abierta`, `corregir`, `analisis`, `truco`, `mapa`) corren cada una
  en su hilo y mandan el texto según se escribe con eventos `parcial`; `cancelar` detiene una
  tarea por su id. La generación de tests y «resumir todos» trabajan en segundo plano.
- La ventana manda al motor las pruebas evaluables del calendario (`hola` y `examenes`) para el
  plan, la cuenta atrás y el análisis.
- El Temario se indexa una sola vez: solo se vuelve a leer un documento si cambia su tamaño o
  fecha (y entonces su resumen se marca como antiguo). Los PDF se leen con `pypdfium2` (rápido y
  respeta los espacios; `pypdf` como reserva).
- La IA genera 3 preguntas tipo test por llamada, cada una de un formato distinto (`ia.FORMATOS`,
  repartidos según la dificultad en `ia.MEZCLA`), y la cita que respalda la respuesta la busca el
  programa en el apunte (no se le pide a la IA: ahorra tiempo y evita citas inventadas); la búsqueda
  compara raíces de palabras y también líneas sueltas, porque las diapositivas apenas tienen frases.
  Se descartan las preguntas con opciones repetidas, «todas las anteriores», referencias a figuras,
  sin respaldo en el texto, con el enunciado copiado del apunte o cuya respuesta correcta se delata
  por ser mucho más larga que las demás. El formato se deduce de la pregunta (un hueco es
  *completar*, «¿cuál es incorrecta?» es *¿cuál es falsa?*), no de lo que diga la IA.
- Cada pregunta del lote se valida y se coloca en el test en cuanto la IA cierra su objeto JSON, sin
  esperar al resto (`ia._objetos_completos`). Las de «¿cuál es falsa?», en las que más se equivoca la
  IA, esperan al final del lote y pasan siempre una segunda revisión.
- Los pasajes empiezan preferentemente por páginas con texto explicado y se saltan las que casi no
  tienen frases (portadas, listas de nombres): sin explicaciones, la IA rellenaba con lo que sabe y la
  pregunta se descartaba por no salir del apunte.
- Las llamadas a Ollama tienen prioridad (`ia.chat(..., fondo=True)`): las de fondo (la reserva) se
  cortan en cuanto empieza una del estudiante.
- Sin IA, los huecos de una palabra solo ocultan sustantivos o adjetivos técnicos, y los distractores
  son de la misma clase y concuerdan en género y número con el artículo que precede al hueco. En la corrección de desarrollo, la nota no puede contradecir a las
  ideas clave cubiertas por más de 3 puntos.
- El texto de la IA se convierte en un Markdown ligero (`## título`, `- viñeta`, `**negrita**`)
  que la ventana dibuja; las fórmulas en LaTeX se pasan a símbolos (≤, →, α, ⊂…), incluso cuando
  la IA las escribe mal escapadas dentro del JSON.
- Tablas nuevas en `profesor.sqlite`: `resumenes`, `abiertas` e `intentos` (desarrollo),
  `conversacion` (chat), `memoria` (análisis, trucos y mapas mentales) y `estudio` (minutos del
  temporizador). Se crean solas.
- El profesor publica un resumen de cada asignatura en `datos/profesor/resumen.json`; la ventana
  principal lo enseña en la tarjeta de la asignatura («Estudio: 31 % de aciertos · 3 tests…») y,
  en *Tu agenda del día* de hoy, un bloque **Para estudiar hoy** con las pruebas de los próximos 14
  días de todas las asignaturas (de la más urgente a la menos) y sus tarjetas pendientes.
- Las preguntas de la versión anterior (`progreso.sqlite`) se importan solas la primera vez.
- Si algo falla, el detalle técnico queda en `datos/profesor/motor.log`.

### Python

Hace falta Python 3.10 o posterior. Se busca en este orden: variable `UNIVERSALIS_PYTHON` (la ruta a un
`python.exe`), `profesor\python\python.exe` (un Python portable junto al programa), un runtime de Codex si
lo tienes instalado, el `python.exe` del PATH y las instalaciones de python.org para el usuario
(`%LOCALAPPDATA%\Programs\Python\Python310…313`). Para cualquier otro, usa `UNIVERSALIS_PYTHON`. Para leer
los PDF y ver sus páginas:

```bat
python -m pip install pypdfium2 pillow
```

### Pruebas

`probar.bat` ejecuta las pruebas automáticas de Python (más las de la música en C++): las del motor (lectura de documentos,
generador, validación de preguntas de IA, tests en los tres modos, modo examen, penalización,
repaso, «Mis fallos», tarjetas, imágenes, dudas y el protocolo con la ventana) y las de las
herramientas de estudio (plan del día, simulacro, resúmenes con y sin IA, «resumir todos» y su
cancelación, desarrollo con y sin IA, corrección, análisis, trucos, conversación guardada,
formato del texto y LaTeX) y las de las demás herramientas (glosario, buscador, mapa mental,
nota estimada, calendario, logros, tiempo de estudio y exportación), las del agrupado de los apuntes en
temas y las de la conexión con la IA (`ia.json`, Ollama y servidores compatibles con OpenAI, contra un
servidor falso). No necesitan Ollama ni conexión: la IA se simula. Las de la música (`pruebas\test_musica.exe`)
solo se ejecutan si `compilar.bat` las ha compilado.
