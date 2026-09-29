# Fase 3 — Concurrencia, Resiliencia y Pruebas

## Cómo resolvimos la concurrencia

No usamos hilos. El servidor atiende el socket de escucha TCP, el socket UDP y todas las
conexiones ya aceptadas con un solo `select()` — el mismo modelo que usan servidores como nginx o
Redis. Ninguna conexión bloquea a las demás: si un cliente se queda callado, el servidor sigue
atendiendo a los otros sin problema.

La razón para no usar hilos: la tabla de nodos y la de clientes conectados es un estado
compartido, y con un solo hilo nunca hay dos ejecuciones tocándolo al mismo tiempo — nos ahorramos
mutex/locks y toda la complejidad (y los bugs) que traen. El enunciado permite usar hilos, pero
también dice que el grupo puede elegir su propia estrategia mientras la justifique — y esta es la
nuestra.

Lo probamos conectando dos nodos y un cliente al mismo tiempo, y los tres siguieron funcionando en
paralelo sin que ninguno esperara a otro.

## Cómo manejamos los errores que pide el enunciado

| Situación | Qué hace el sistema |
|---|---|
| **Mensaje con formato incorrecto** (tipo desconocido, campos faltantes) | Responde `RESPONSE;status=ERROR;code=400` y sigue funcionando normal |
| **Parámetros inválidos** (puerto no numérico al arrancar el servidor) | El servidor no arranca, imprime el error y explica el uso correcto |
| **Desconexión** (un nodo o cliente cierra la conexión, o se cae de golpe) | El servidor libera ese socket y sigue atendiendo a los demás sin caerse |
| **Falla en la comunicación** (`send()`/`recv()` fallan) | Se captura el error, se loggea, y no se usa señal `SIGPIPE` que tumbaría el proceso |
| **Nodo desconocido** manda `STATUS` o `EVENT` | Se descarta (UDP) o se responde `404 nodo_no_encontrado` (TCP), y queda en el log |
| **Expiración de temporizador** | Si un nodo no manda `STATUS` en 15 segundos, se marca `INACTIVO` automáticamente (chequeo cada 3 segundos) |
| **Pérdida de mensajes** | Solo aplica a `STATUS` (UDP) — es tolerable a propósito, no se implementa retransmisión porque no hace falta |
| **Duplicación de mensajes** | Cada `STATUS` es una foto completa del estado (no un incremento), así que si UDP llegara a duplicar un datagrama no causa ningún daño — no hace falta lógica extra |

## Un problema real que no se veía en la Fase 2

TCP no entrega mensajes por líneas — puede juntar dos mensajes en un solo `recv()`, o partir uno a
la mitad entre dos `recv()`. Antes no lo manejábamos. Ahora cada cliente tiene su propio buffer que
va acumulando bytes hasta encontrar el salto de línea (`\n`); si llega más de un mensaje junto, se
procesan uno por uno; si llega uno incompleto, se espera al siguiente `recv()` para completarlo.
Lo probamos mandando dos mensajes pegados y uno partido a propósito, y ambos casos se
interpretaron bien.

## Cómo probarlo

Igual que en la Fase 2 (`make`, `./server <puerto> <log>`, y correr `node.py`/`client.py`), pero
ahora puedes además:

- Conectar dos o más nodos/clientes a la vez y ver que ninguno bloquea a los otros.
- Mandar un mensaje sin sentido y ver que responde error en vez de caerse.
- Cerrar un nodo con `Ctrl+C` a la mitad y ver que el servidor sigue funcionando.
- Dejar un nodo registrado sin mandarle más `STATUS` y esperar ~15-18 segundos: en el log aparece
  `TIMEOUT ... marcado INACTIVO`.

## Para la sustentación

Las preguntas más probables y la respuesta corta:

- **¿Por qué no usaron hilos?** Por evitar condiciones de carrera en la tabla de nodos sin tener
  que meter mutex; con `select()` un solo hilo atiende a todos sin bloquearse.
- **¿Qué pasa si dos mensajes llegan pegados por TCP?** Se acumulan en un buffer por cliente y se
  separan por el `\n`.
- **¿Qué pasa si un nodo se cae sin avisar?** El servidor lo detecta cuando el `recv()` falla (si
  tenía conexión TCP abierta) o cuando deja de mandar `STATUS` por más de 15 segundos (temporizador).
- **¿Por qué no hay retransmisión sobre UDP?** Porque el enunciado permite perder telemetría a
  propósito — agregarla sería resolver un problema que no existe.
