# Fase 2 — Comunicación Básica

Esto implementa, con la API de Sockets Berkeley, lo que se diseñó en la Fase 1: el servidor
en C y dos clientes de prueba en Python (un nodo simulado y un cliente de administración).

No hay hilos todavía — el servidor atiende TCP, UDP y todos los clientes conectados con un
solo `select()`. La concurrencia "de verdad" y el manejo de errores más fino quedan para la
Fase 3; aquí lo que se demuestra es que el protocolo de la Fase 1 realmente funciona sobre
la red.

## Estructura

```
fase2/
├── server/
│   ├── server.c     -> servidor en C
│   └── Makefile
└── clientes/
    ├── node.py       -> simula un nodo (sensor)
    └── client.py     -> cliente de administración
```

## Cómo correrlo

**1. Compilar el servidor**

```bash
cd server
make
```

**2. Levantar el servidor** (puerto y archivo de logs por parámetro, como pide el enunciado)

```bash
./server 9000 registro.log
```

Esto abre TCP en el puerto 9000 y UDP en el puerto 9001 (`puerto + 1`). Cada petición y
respuesta queda en consola y en `registro.log`, con la IP y el puerto de quien la mandó.

**3. Simular un nodo** (en otra terminal)

```bash
cd clientes
python3 node.py localhost 9000 sensor01
```

Se registra, y cada 3 segundos manda su estado por UDP. Si la temperatura simulada supera 40,
manda también un evento crítico por TCP.

**4. Simular un cliente de administración** (en otra terminal)

```bash
cd clientes
python3 client.py localhost 9000
```

Pide usuario y clave (usuarios de prueba: `admin1`/`1234` o `admin2`/`abcd`), y después deja
consultar el estado/historial de cualquier nodo por su `node_id`.

## Qué queda pendiente para la Fase 3

- Concurrencia real (o formalizar el `select()` actual y justificarlo).
- Reemplazar la tabla de usuarios fija por un mecanismo de autenticación externo de verdad.
- Manejo más robusto de mensajes truncados o que llegan pegados en el mismo `recv()`.
- Persistir el estado si el servidor se cae y vuelve a arrancar.
