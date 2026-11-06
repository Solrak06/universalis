# Conectar tu propio modelo de IA a Universalis

El **Profesor** de Universalis usa un modelo de lenguaje para redactar preguntas de test, responder dudas
sobre tus apuntes, resumir temas, corregir preguntas de desarrollo y hacer mapas de conceptos. Sin IA
también funciona (modo **Rápido**: preguntas sacadas de las frases de los apuntes), pero las opciones de
preguntar al profesor se ocultan.

Este documento explica cómo conectar **tu** modelo, sea cual sea:

| Dónde está tu modelo | Tipo | Ejemplo de `url` |
|---|---|---|
| Ollama en tu equipo (lo de siempre) | `ollama` | `http://127.0.0.1:11434` |
| Ollama en otro equipo de tu red | `ollama` | `http://192.168.1.50:11434` |
| LM Studio | `openai` | `http://127.0.0.1:1234/v1` |
| llama.cpp (`llama-server`) | `openai` | `http://127.0.0.1:8080/v1` |
| vLLM, LocalAI, Jan, text-generation-webui… | `openai` | la que indique el servidor, acabada en `/v1` |
| OpenAI | `openai` | `https://api.openai.com/v1` |
| OpenRouter | `openai` | `https://openrouter.ai/api/v1` |
| Groq, Mistral, DeepSeek, Together… | `openai` | su dirección compatible con OpenAI |

«`openai`» no significa que tenga que ser de OpenAI: es **cualquier servidor compatible con la API de chat
de OpenAI** (`POST /v1/chat/completions` con `stream: true`). Casi todos los programas y servicios la
ofrecen.

---

## 1. Lo más rápido: un modelo cualquiera con Ollama

Si ya usas Ollama no hace falta configurar nada:

```
ollama pull qwen2.5:7b
```

Al abrir el Profesor aparece en **Nuevo test → Modelo IA** junto a los demás; elige el que quieras. Si no
eliges ninguno, el Profesor prefiere, por este orden: `gemma4`, `gemma3`, `qwen3`, `qwen2.5`, `llama3.1`,
`mistral`, `gemma`, `llama3`, `phi` (y si no hay ninguno, el primero que encuentre).

¿Tienes un modelo propio en formato **GGUF** (por ejemplo, uno ajustado por ti)? Impórtalo en Ollama con un
`Modelfile`:

```
FROM C:\ruta\a\mi-modelo.gguf
```

```
ollama create mi-modelo -f Modelfile
```

y ya sale en la lista como `mi-modelo:latest`.

---

## 2. El archivo `datos/ia.json`

Para cualquier otra cosa se crea el archivo **`datos/ia.json`** (en la carpeta `datos`, junto a
`asignaturas.json`). Hay un ejemplo listo para copiar en [`ia.ejemplo.json`](ia.ejemplo.json).

```json
{
  "proveedor": "openai",
  "nombre": "LM Studio",
  "url": "http://127.0.0.1:1234/v1",
  "clave": "",
  "modelo": "qwen2.5-7b-instruct"
}
```

| Campo | Para qué sirve | Por defecto |
|---|---|---|
| `proveedor` | `"ollama"` u `"openai"` (compatible con OpenAI) | `"ollama"` |
| `url` | Dirección del servidor. Con `openai`, normalmente acaba en `/v1` | `http://127.0.0.1:11434` (ollama) · `http://127.0.0.1:1234/v1` (openai) |
| `nombre` | Cómo se llama en los mensajes de la app («Conectando con LM Studio…») | «Ollama» o «el servidor de IA» |
| `clave` | Clave de la API (se manda como `Authorization: Bearer …`). Mejor `"env:NOMBRE_VARIABLE"` para leerla de una variable de entorno y no dejarla escrita | vacía |
| `modelo` | Modelo que se usa si en la app no has elegido otro | el primero recomendado |
| `modelos` | Lista de modelos que se ofrecen en la app (útil con servicios que tienen cientos) | todos los que liste el servidor |
| `json` | Cómo se pide el JSON de los tests: `"esquema"` (json_schema), `"objeto"` (json_object) o `"texto"` (solo en el mensaje) | `"esquema"` |
| `max_tokens` | Mandar un límite de longitud a cada respuesta. Ponlo a `false` con modelos que «razonan» | `true` |
| `razonamiento` | Se manda como `reasoning_effort` (`"none"`, `"low"`, `"medium"`, `"high"`) a los modelos que piensan antes de responder. `"none"` lo apaga y responden mucho antes | no se manda |
| `cabeceras` | Cabeceras HTTP extra, p. ej. `{"HTTP-Referer": "…"}` para OpenRouter | ninguna |
| `tiempo` | Segundos de espera de la conexión | `240` |

La carpeta `datos/` está en el `.gitignore` del proyecto: tu `ia.json` (y la clave que pongas) no se sube
a GitHub si compartes el código.

**No hace falta cerrar la app al cambiar el archivo**: el Profesor lo vuelve a leer cada vez que comprueba
la IA (al abrir una asignatura o al pulsar «IA sin conexión · Reintentar» en la barra de arriba). Si el
archivo tiene un error, la app avisa y sigue con Ollama.

### Variables de entorno (opcional)

Tienen prioridad sobre `ia.json`. Útiles para no dejar claves en un archivo:

| Variable | Equivale a |
|---|---|
| `UNIVERSALIS_IA_PROVEEDOR` | `proveedor` |
| `UNIVERSALIS_IA_URL` | `url` |
| `UNIVERSALIS_IA_CLAVE` | `clave` |
| `UNIVERSALIS_IA_MODELO` | `modelo` |
| `UNIVERSALIS_IA_NOMBRE` | `nombre` |
| `UNIVERSALIS_OLLAMA` | `url` de Ollama (la de siempre, se mantiene) |
| `UNIVERSALIS_HILOS` | hilos de CPU que usa Ollama |

En Windows se ponen con `setx UNIVERSALIS_IA_CLAVE "sk-..."` (y se vuelve a abrir la app).

---

## 3. Ejemplos

### LM Studio (en tu equipo)

1. En LM Studio, descarga un modelo y abre la pestaña **Developer → Start Server** (puerto 1234).
2. `datos/ia.json`:

```json
{ "proveedor": "openai", "nombre": "LM Studio", "url": "http://127.0.0.1:1234/v1" }
```

### llama.cpp

```
llama-server -m mi-modelo.gguf --port 8080 -c 8192
```

```json
{ "proveedor": "openai", "nombre": "llama.cpp", "url": "http://127.0.0.1:8080/v1", "modelo": "mi-modelo" }
```

### Ollama por su vía compatible con OpenAI

Ollama también ofrece la API de OpenAI en `/v1`. No hace falta (el tipo `ollama` es mejor: precarga el
modelo y apaga el razonamiento solo), pero sirve para probar esta vía sin instalar nada más:

```json
{ "proveedor": "openai", "url": "http://127.0.0.1:11434/v1", "razonamiento": "none" }
```

### Ollama en otro ordenador de casa

En ese ordenador, Ollama tiene que escuchar en la red (`setx OLLAMA_HOST 0.0.0.0` y reiniciarlo). Luego:

```json
{ "proveedor": "ollama", "url": "http://192.168.1.50:11434" }
```

### OpenAI

```json
{
  "proveedor": "openai",
  "nombre": "OpenAI",
  "url": "https://api.openai.com/v1",
  "clave": "env:OPENAI_API_KEY",
  "modelo": "gpt-4o-mini",
  "modelos": ["gpt-4o-mini", "gpt-4o"]
}
```

### OpenRouter (cientos de modelos con una sola clave)

```json
{
  "proveedor": "openai",
  "nombre": "OpenRouter",
  "url": "https://openrouter.ai/api/v1",
  "clave": "env:OPENROUTER_API_KEY",
  "modelo": "meta-llama/llama-3.1-8b-instruct",
  "modelos": ["meta-llama/llama-3.1-8b-instruct", "qwen/qwen-2.5-72b-instruct"],
  "json": "objeto"
}
```

### Volver a Ollama

Borra `datos/ia.json` (o pon `"proveedor": "ollama"`).

---

## 4. Qué le pide el Profesor al modelo

Para que sepas si tu modelo va a dar la talla:

- **Mensajes** en formato chat (`system` + `user`), siempre en **español** y basados en fragmentos de tus
  apuntes (unos 2.000 caracteres). Con 8.000 tokens de contexto sobra.
- **Respuestas en streaming**: el texto se ve según se escribe. El servidor debe aceptar `"stream": true`
  y mandar eventos `data: {...}` (SSE), como hace la API de OpenAI.
- **JSON estructurado** para los tests, las preguntas de desarrollo, sus correcciones y los mapas. Con
  `"json": "esquema"` se manda `response_format: {"type": "json_schema", ...}`; si tu servidor no lo admite
  (da un error 400 que menciona `response_format`), usa `"objeto"` o `"texto"`: el esquema va entonces
  dentro del mensaje y el Profesor repara y valida lo que llegue. Las preguntas que no pasan la validación
  (sin cita en los apuntes, opciones repetidas, respuesta que no está…) se descartan.
- **Longitud**: cada tarea manda su límite (`max_tokens`, de 160 a ~3.200 tokens). Los modelos que razonan
  antes de contestar (Gemma 4, DeepSeek-R1, Qwen3, gpt-oss, o1…) pueden gastarlo pensando: si ves «El modelo
  gastó todo el límite pensando» o «La respuesta de la IA se cortó», pon `"razonamiento": "none"` (si el
  servidor lo admite, como Ollama o LM Studio) o `"max_tokens": false`. El texto entre `<think>` y `</think>`
  se quita solo.
- **Temperatura** entre 0 y 0,7 según la tarea.

Recomendación: un modelo **instruct** de 7–8 B parámetros o más, que sepa español y siga bien formatos JSON
(Qwen 2.5/3, Llama 3.1, Gemma 3/4, Mistral). Los de 1–3 B suelen fallar con el JSON.

### Diferencias con Ollama

- Con Ollama, el Profesor **arranca** Ollama si está cerrado y **precarga** el modelo. Un servidor externo lo
  arrancas tú: la app solo comprueba que conteste.
- Con un servicio **de pago**, cada pregunta cuesta dinero. El Profesor prepara de fondo una **reserva de
  preguntas** para que los tests empiecen al momento; esa reserva también hace llamadas. Si no la quieres,
  pon la variable `UNIVERSALIS_SIN_RESERVA=1`.
- Tus apuntes (los fragmentos que hacen falta para cada pregunta) **se envían a ese servicio**. Con Ollama,
  LM Studio o llama.cpp en tu equipo, no salen de él.

---

## 5. Si algo falla

| Lo que ves | Qué hacer |
|---|---|
| «IA sin conexión · Reintentar» | El servidor no contesta en esa `url`. Ábrelo y pulsa en ese aviso. Comprueba la dirección en el navegador: `<url>/models` debe devolver JSON. |
| «… devolvió un error (401)» | La clave no vale o falta (`clave`). |
| «… devolvió un error (404)» | La `url` no acaba en `/v1` o el `modelo` no existe en ese servidor. |
| «… (400) … response_format» | Pon `"json": "objeto"` o `"texto"`. |
| «… (429)» | Límite de peticiones del servicio: espera o usa otro plan. |
| «El modelo gastó todo el límite pensando» | `"razonamiento": "none"` (o `"low"`). |
| «La respuesta de la IA se cortó» | `"razonamiento": "none"` o `"max_tokens": false`. |
| La app avisa de que no puede leer `ia.json` | JSON mal escrito (comillas, comas). Valídalo en cualquier validador de JSON. |
| No sale mi modelo en la lista | Con `openai`, se ocultan los que parecen de *embeddings*, audio o imágenes; ponlo en `modelo` o en `modelos`. |

El registro del motor está en `datos/profesor/motor.log`.

---

## 6. Para desarrolladores: añadir otro tipo de servidor

Todo lo que habla con la IA está en **`profesor/ia.py`** (solo biblioteca estándar de Python). El resto del
programa llama siempre a las mismas funciones:

| Función | Qué debe hacer |
|---|---|
| `configurar(ruta)` | Leer `ia.json` y las variables de entorno y rellenar `CONFIG` |
| `modelos(timeout)` | Lista de nombres de modelos de chat, o `None` si el servidor no responde |
| `instalado()` | `True` si hay algo que se pueda «iniciar» o reintentar desde la app |
| `arrancar()` | Dejar el servidor respondiendo (o comprobar que responde) |
| `precalentar(modelo)` | Cargar el modelo en memoria si tiene sentido (si no, devolver `True`) |
| `chat(modelo, mensajes, formato, opciones, al_recibir, cancelar, limite, fondo)` | Conversación en streaming. Llama a `al_recibir(texto_acumulado)` con cada trozo; devuelve el texto final. Lanza `Cancelado` si `cancelar` se activa (o si es de `fondo` y llega una petición del estudiante), `SinConexion` si no conecta y `ErrorIA` con un mensaje en español para lo demás |

`formato` es un esquema JSON (o `None`); `opciones` trae `temperature` y `num_predict` (longitud máxima). Para
añadir un tipo nuevo: un valor más en `PROVEEDORES`, su rama en `modelos()` y en `_chat()`, y una prueba en
`profesor/pruebas/test_conexion_ia.py` (usa un servidor HTTP falso, no hace falta conexión). Las pruebas se
ejecutan con `probar.bat`.

El motor (`profesor/servidor.py`) es un proceso aparte que la ventana lanza y con el que habla por
`stdin`/`stdout` con una línea de JSON por orden (`{"id": 1, "cmd": "duda", ...}` →
`{"id": 1, "ok": true, ...}`); la ventana no habla nunca directamente con la IA.
