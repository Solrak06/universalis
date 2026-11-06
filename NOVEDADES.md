# Novedades

## 6 de noviembre de 2026

### Lector del Temario

- **La barra de la derecha se puede arrastrar** (o pulsar en su carril para saltar ahí). Antes solo se veía:
  ahora sirve para ir a cualquier página del PDF, y lo mismo en todas las pantallas del Profesor.
- **La lista de documentos se queda fija** a la izquierda mientras bajas por el apunte, y tiene su propio
  desplazamiento: con el ratón encima, la rueda mueve la lista; fuera, el documento. Si es larga, tiene su
  barra, y al abrir un documento se coloca para que se vea.
- **‹ Página 3 / 20 ›**: flechas para saltar al principio de la página anterior o la siguiente (también
  `RePág` / `AvPág`).
- **Zoom sin botón aparte**: el «Ajustar» que aparecía y desaparecía ya no está. Para volver al tamaño de
  inicio se pulsa el propio porcentaje (se resalta cuando no está al 100 %) o `0`. Al ampliar ya no se
  oculta la lista.

### Conectar tu propio modelo de IA

- Además de Ollama, el Profesor puede usar **cualquier servidor compatible con la API de OpenAI**: LM
  Studio, llama.cpp, vLLM, OpenAI, OpenRouter, Groq… Se configura con `datos/ia.json` (ejemplo en
  `ia.ejemplo.json`) y todo se explica en [CONECTAR_IA.md](CONECTAR_IA.md). Sin ese archivo, todo sigue
  como siempre.
- Probado de verdad contra el endpoint `/v1` de Ollama: JSON con esquema, texto en streaming y un lote de
  preguntas de test (2 válidas de 2).

### Coherencia

- Sin IA ya no se menciona preguntar al profesor en ningún sitio: ni en la ayuda de Buscar, ni en Mis
  fallos, ni en los atajos (`F1`).
- Los atajos de «cualquier pantalla» (`Ctrl+T`, `Ctrl+M`, `Ctrl+F`, `F9`, `Alt+←`, `Alt+0…9`) funcionan
  también escribiendo en un campo. `Alt+1…9` siguen el orden del menú tal como se ve.
- Mismos nombres en todas partes: «Solo IA», «Test de mis fallos», «Repaso de lo que toca hoy», las
  repreguntas del lector iguales que las de Dudas, el simulacro con la misma descripción.
- Plurales («1 página», «1 tarjeta», «1 test»), tildes y «%» con espacio también en el informe HTML, y
  comillas «» en los diálogos de borrar.
- README y PROFESOR.md al día (requisitos de Python, compilación a mano, menú, lector, IA).

## 27 de octubre de 2026

### El profesor (IA)

- **Responde antes.** Antes, cada vez que pasabas de una duda a un resumen o a un test, Ollama volvía a cargar el
  modelo (más de un minuto), porque cada tarea le pedía un contexto distinto. Ahora todas usan las mismas opciones
  y casi todos los hilos del procesador (deja dos libres para que la ventana no se congele): escribe ~50 % más rápido.
- **Lee menos y mejor.** De cada fragmento de tus apuntes solo le pasa las frases que tienen que ver con tu
  pregunta. Las portadas y las tablas de números cuentan menos. En una repregunta y en «Explícame esta página»
  no repite el texto que ya tiene. La primera palabra de una duda llega en 15–40 s (antes, 35–75 s).
- **Encuentra mejor.** La búsqueda ya no depende de tildes ni de plurales («validaciones» encuentra «validación»).
- **Va al grano.** Sin «¡Hola! Como tu profesor…» ni despedidas.
- **Entiende de qué le hablas.** Cuando preguntas sobre un trozo seleccionado de los apuntes, ya no lo confunde
  con una pregunta de test.
- **No te deja a ciegas.** Mientras prepara la respuesta, la burbuja dice en qué está y lleva un cronómetro.
- **«IA apagada» sin estarlo:** corregido. Pasaba cuando Ollama estaba ocupado y tardaba en contestar.
- **Si Ollama se cierra solo**, el profesor lo vuelve a abrir y repite tu petición, sin darte error.
- **Cuida la batería.** Con el portátil desenchufado, la preparación de preguntas en segundo plano se pausa.

### La interfaz

- Todas las pantallas del profesor usan la misma columna y el buscador de arriba no cambia de sitio.
- Los avisos salen arriba a la derecha, sin tapar el título (el trofeo, solo para los logros).
- Temario: los botones caben en una fila y «Leído» es una pastilla que se pulsa. En pantalla completa ya no se
  solapa nada.
- Tarjetas: nuevo panel **Tu mazo por cajas** (cuántas tienes en cada escalón del repaso).
- Corrección del test: botones con icono, como en el resto.
- Atajos **Alt+1…9** (Alt+0: Mis fallos) para ir a cada sección. La ayuda (F1) explica también Ctrl+rueda y
  la selección de texto en el lector.
- Ventana principal: la ficha de la asignatura lleva su icono en un cuadro de color (antes, un dibujo cortado).
  Los nombres de las asignaturas ya no se cortan. En ventanas pequeñas el calendario cede sitio y los
  botones que no caben muestran solo su icono.
- Detalles: los nombres de los temas salen con tildes («Validación Cruzada»), plurales bien
  («1 tarjeta pendiente»), «70 %» con espacio en todas partes y el bocadillo del profesor habla de la
  pantalla en la que estás.
