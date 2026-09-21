/*
 * Servidor PMCD - Fase 2
 * Comunicacion basica con la API de Sockets Berkeley (sin librerias externas).
 *
 * Uso: ./server <puerto> <archivoDeLogs>
 *
 *   - Escucha TCP en <puerto>          -> REGISTER, EVENT, LOGIN, QUERY
 *   - Escucha UDP en <puerto + 1>      -> STATUS (telemetria, se puede perder)
 *
 * Un solo proceso, un solo hilo: usamos select() para atender el socket de
 * escucha TCP, el socket UDP y todas las conexiones TCP ya aceptadas al
 * mismo tiempo. La concurrencia "de verdad" (hilos) y el manejo de errores
 * mas fino quedan para la Fase 3; aqui el objetivo es demostrar que el
 * protocolo definido en la Fase 1 efectivamente funciona.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUF_SIZE       1024
#define MAX_CLIENTS    64
#define MAX_NODES      64
#define MAX_HISTORY    5
#define MAX_FIELDS     10

/* ---------- estructuras del protocolo ---------- */

typedef struct {
    char key[32];
    char value[128];
} Field;

typedef struct {
    double cpu;
    double temp;
    double battery;
    time_t timestamp;
} StatusSnapshot;

typedef struct {
    int en_uso;
    char node_id[64];
    char node_type[32];
    int activo;
    time_t ultimo_reporte;
    StatusSnapshot historial[MAX_HISTORY];
    int hist_count;
    int hist_next;
    char ultimo_evento[128];
} NodeInfo;

typedef struct {
    int fd;
    int en_uso;
    char ip[INET6_ADDRSTRLEN];
    int puerto;
    int autenticado;
} ClientConn;

static NodeInfo nodos[MAX_NODES];
static int total_nodos = 0;

static ClientConn clientes[MAX_CLIENTS];

static FILE *archivo_log = NULL;

/* Usuarios validos para LOGIN. En una version posterior esto deberia salir
 * de un servicio de autenticacion aparte (asi lo dejamos anotado en la Fase 1);
 * por ahora, para poder demostrar el flujo completo, usamos una tabla fija. */
typedef struct { const char *user; const char *pass; } Credencial;
static const Credencial CREDENCIALES[] = {
    {"admin1", "1234"},
    {"admin2", "abcd"}
};
#define NUM_CREDENCIALES (int)(sizeof(CREDENCIALES) / sizeof(CREDENCIALES[0]))

/* ---------- utilidades de logging ---------- */

static void log_evento(const char *ip, int puerto, const char *direccion, const char *contenido) {
    time_t ahora = time(NULL);
    char tbuf[32];
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", localtime(&ahora));

    printf("[%s] %s:%d %s %s\n", tbuf, ip, puerto, direccion, contenido);
    fflush(stdout);

    if (archivo_log) {
        fprintf(archivo_log, "[%s] %s:%d %s %s\n", tbuf, ip, puerto, direccion, contenido);
        fflush(archivo_log);
    }
}

/* ---------- parseo de mensajes: "TIPO;campo=valor;campo=valor" ---------- */

static int parsear_mensaje(char *raw, char *tipo_out, Field *campos, int max_campos) {
    int n = 0;
    char *tok = strtok(raw, ";");
    if (!tok) return -1;

    strncpy(tipo_out, tok, 31);
    tipo_out[31] = '\0';

    while ((tok = strtok(NULL, ";")) != NULL && n < max_campos) {
        char *igual = strchr(tok, '=');
        if (!igual) continue;
        *igual = '\0';
        strncpy(campos[n].key, tok, 31);
        campos[n].key[31] = '\0';
        strncpy(campos[n].value, igual + 1, 127);
        campos[n].value[127] = '\0';
        n++;
    }
    return n;
}

static const char *obtener_campo(Field *campos, int n, const char *clave) {
    for (int i = 0; i < n; i++) {
        if (strcmp(campos[i].key, clave) == 0) return campos[i].value;
    }
    return NULL;
}

/* ---------- manejo de nodos ---------- */

static NodeInfo *buscar_nodo(const char *id) {
    for (int i = 0; i < total_nodos; i++) {
        if (nodos[i].en_uso && strcmp(nodos[i].node_id, id) == 0) return &nodos[i];
    }
    return NULL;
}

static NodeInfo *registrar_nodo(const char *id, const char *tipo) {
    NodeInfo *n = buscar_nodo(id);
    if (!n) {
        if (total_nodos >= MAX_NODES) return NULL;
        n = &nodos[total_nodos++];
        memset(n, 0, sizeof(NodeInfo));
        strncpy(n->node_id, id, 63);
    }
    strncpy(n->node_type, tipo ? tipo : "desconocido", 31);
    n->en_uso = 1;
    n->activo = 1;
    n->ultimo_reporte = time(NULL);
    return n;
}

static void actualizar_status(const char *id, double cpu, double temp, double battery) {
    NodeInfo *n = buscar_nodo(id);
    if (!n) return; /* nodo desconocido: se descarta, es UDP, no hay a quien responder */

    n->activo = 1;
    n->ultimo_reporte = time(NULL);

    StatusSnapshot *s = &n->historial[n->hist_next];
    s->cpu = cpu;
    s->temp = temp;
    s->battery = battery;
    s->timestamp = time(NULL);

    n->hist_next = (n->hist_next + 1) % MAX_HISTORY;
    if (n->hist_count < MAX_HISTORY) n->hist_count++;
}

/* ---------- construccion de RESPONSE ---------- */

static void resp_ok(char *out, size_t out_size, const char *extra) {
    if (extra && extra[0] != '\0')
        snprintf(out, out_size, "RESPONSE;status=OK;%s\n", extra);
    else
        snprintf(out, out_size, "RESPONSE;status=OK\n");
}

static void resp_error(char *out, size_t out_size, int codigo, const char *msg) {
    snprintf(out, out_size, "RESPONSE;status=ERROR;code=%d;msg=%s\n", codigo, msg);
}

/* ---------- handlers de cada tipo de mensaje (llegan por TCP) ---------- */

static void handle_register(Field *campos, int n, char *out, size_t out_size) {
    const char *node_id = obtener_campo(campos, n, "node_id");
    const char *tipo = obtener_campo(campos, n, "tipo");

    if (!node_id) {
        resp_error(out, out_size, 400, "falta_node_id");
        return;
    }
    if (!registrar_nodo(node_id, tipo)) {
        resp_error(out, out_size, 500, "limite_de_nodos_alcanzado");
        return;
    }
    char extra[64];
    snprintf(extra, sizeof(extra), "msg=registrado;node_id=%s", node_id);
    resp_ok(out, out_size, extra);
}

static void handle_event(Field *campos, int n, char *out, size_t out_size) {
    const char *node_id = obtener_campo(campos, n, "node_id");
    const char *tipo_evento = obtener_campo(campos, n, "tipo");
    const char *valor = obtener_campo(campos, n, "valor");

    if (!node_id) {
        resp_error(out, out_size, 400, "falta_node_id");
        return;
    }
    NodeInfo *nodo = buscar_nodo(node_id);
    if (!nodo) {
        resp_error(out, out_size, 404, "nodo_no_encontrado");
        return;
    }
    snprintf(nodo->ultimo_evento, sizeof(nodo->ultimo_evento), "%s:%s",
             tipo_evento ? tipo_evento : "?", valor ? valor : "?");

    resp_ok(out, out_size, "msg=evento_recibido");
}

static int verificar_login(const char *user, const char *pass) {
    for (int i = 0; i < NUM_CREDENCIALES; i++) {
        if (strcmp(CREDENCIALES[i].user, user) == 0 && strcmp(CREDENCIALES[i].pass, pass) == 0)
            return 1;
    }
    return 0;
}

static void handle_login(Field *campos, int n, ClientConn *cliente, char *out, size_t out_size) {
    const char *user = obtener_campo(campos, n, "user");
    const char *clave = obtener_campo(campos, n, "clave");

    if (!user || !clave) {
        resp_error(out, out_size, 400, "faltan_credenciales");
        return;
    }
    if (!verificar_login(user, clave)) {
        resp_error(out, out_size, 401, "credenciales_invalidas");
        return;
    }
    cliente->autenticado = 1;
    resp_ok(out, out_size, "msg=bienvenido");
}

static void handle_query(Field *campos, int n, ClientConn *cliente, char *out, size_t out_size) {
    if (!cliente->autenticado) {
        resp_error(out, out_size, 403, "no_autorizado_haga_login_primero");
        return;
    }

    const char *node_id = obtener_campo(campos, n, "node_id");
    const char *n_str = obtener_campo(campos, n, "n");
    int cuantos = n_str ? atoi(n_str) : 1;
    if (cuantos < 1) cuantos = 1;
    if (cuantos > MAX_HISTORY) cuantos = MAX_HISTORY;

    if (!node_id) {
        resp_error(out, out_size, 400, "falta_node_id");
        return;
    }
    NodeInfo *nodo = buscar_nodo(node_id);
    if (!nodo) {
        resp_error(out, out_size, 404, "nodo_no_encontrado");
        return;
    }
    if (nodo->hist_count == 0) {
        resp_error(out, out_size, 404, "sin_datos_todavia");
        return;
    }

    int disponibles = nodo->hist_count < cuantos ? nodo->hist_count : cuantos;

    /* recorremos el historial desde el mas reciente hacia atras */
    char datos[512] = "";
    for (int i = 0; i < disponibles; i++) {
        int idx = (nodo->hist_next - 1 - i + MAX_HISTORY) % MAX_HISTORY;
        StatusSnapshot *s = &nodo->historial[idx];
        char entrada[80];
        snprintf(entrada, sizeof(entrada), "%.1f,%.1f,%.1f%s",
                 s->cpu, s->temp, s->battery, (i < disponibles - 1) ? "|" : "");
        strncat(datos, entrada, sizeof(datos) - strlen(datos) - 1);
    }

    char extra[700];
    snprintf(extra, sizeof(extra), "node_id=%s;activo=%s;count=%d;data=%s",
             nodo->node_id, nodo->activo ? "SI" : "NO", disponibles, datos);
    resp_ok(out, out_size, extra);
}

/* ---------- despachador: recibe una linea de texto por TCP ---------- */

static void procesar_mensaje_tcp(char *linea, ClientConn *cliente, char *out, size_t out_size) {
    char tipo[32];
    Field campos[MAX_FIELDS];

    int n = parsear_mensaje(linea, tipo, campos, MAX_FIELDS);
    if (n < 0) {
        resp_error(out, out_size, 400, "mensaje_mal_formado");
        return;
    }

    if (strcmp(tipo, "REGISTER") == 0) {
        handle_register(campos, n, out, out_size);
    } else if (strcmp(tipo, "EVENT") == 0) {
        handle_event(campos, n, out, out_size);
    } else if (strcmp(tipo, "LOGIN") == 0) {
        handle_login(campos, n, cliente, out, out_size);
    } else if (strcmp(tipo, "QUERY") == 0) {
        handle_query(campos, n, cliente, out, out_size);
    } else {
        resp_error(out, out_size, 400, "tipo_desconocido");
    }
}

/* ---------- utilidades de sockets ---------- */

static int crear_socket_tcp_escucha(int puerto) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket TCP"); exit(1); }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(puerto);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind TCP");
        exit(1);
    }
    if (listen(fd, 16) < 0) {
        perror("listen");
        exit(1);
    }
    return fd;
}

static int crear_socket_udp(int puerto) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { perror("socket UDP"); exit(1); }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(puerto);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind UDP");
        exit(1);
    }
    return fd;
}

static ClientConn *registrar_cliente(int fd, const char *ip, int puerto) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clientes[i].en_uso) {
            clientes[i].en_uso = 1;
            clientes[i].fd = fd;
            clientes[i].autenticado = 0;
            strncpy(clientes[i].ip, ip, sizeof(clientes[i].ip) - 1);
            clientes[i].puerto = puerto;
            return &clientes[i];
        }
    }
    return NULL;
}

static ClientConn *buscar_cliente_por_fd(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clientes[i].en_uso && clientes[i].fd == fd) return &clientes[i];
    }
    return NULL;
}

static void liberar_cliente(int fd) {
    ClientConn *c = buscar_cliente_por_fd(fd);
    if (c) c->en_uso = 0;
}

/* ---------- main: select() sobre TCP-escucha, UDP y clientes TCP activos ---------- */

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <puerto> <archivoDeLogs>\n", argv[0]);
        return 1;
    }
    int puerto_tcp = atoi(argv[1]);
    int puerto_udp = puerto_tcp + 1;

    archivo_log = fopen(argv[2], "a");
    if (!archivo_log) {
        fprintf(stderr, "Aviso: no se pudo abrir '%s', se sigue solo con consola (%s)\n",
                argv[2], strerror(errno));
    }

    int fd_tcp = crear_socket_tcp_escucha(puerto_tcp);
    int fd_udp = crear_socket_udp(puerto_udp);

    printf("Servidor PMCD escuchando TCP:%d y UDP:%d\n", puerto_tcp, puerto_udp);

    fd_set master, listos;
    FD_ZERO(&master);
    FD_SET(fd_tcp, &master);
    FD_SET(fd_udp, &master);
    int fdmax = fd_tcp > fd_udp ? fd_tcp : fd_udp;

    while (1) {
        listos = master;
        if (select(fdmax + 1, &listos, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        for (int fd = 0; fd <= fdmax; fd++) {
            if (!FD_ISSET(fd, &listos)) continue;

            if (fd == fd_tcp) {
                /* nueva conexion TCP entrante */
                struct sockaddr_in cliaddr;
                socklen_t len = sizeof(cliaddr);
                int nuevo_fd = accept(fd_tcp, (struct sockaddr *)&cliaddr, &len);
                if (nuevo_fd < 0) { perror("accept"); continue; }

                char ipbuf[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &cliaddr.sin_addr, ipbuf, sizeof(ipbuf));
                int puerto_origen = ntohs(cliaddr.sin_port);

                if (!registrar_cliente(nuevo_fd, ipbuf, puerto_origen)) {
                    log_evento(ipbuf, puerto_origen, "RECHAZADO", "limite_de_clientes_alcanzado");
                    close(nuevo_fd);
                    continue;
                }

                FD_SET(nuevo_fd, &master);
                if (nuevo_fd > fdmax) fdmax = nuevo_fd;
                log_evento(ipbuf, puerto_origen, "CONEXION", "nueva conexion TCP aceptada");

            } else if (fd == fd_udp) {
                /* telemetria UDP: STATUS, se puede perder, no hay respuesta */
                char buf[BUF_SIZE];
                struct sockaddr_in origen;
                socklen_t len = sizeof(origen);
                ssize_t n = recvfrom(fd_udp, buf, sizeof(buf) - 1, 0,
                                      (struct sockaddr *)&origen, &len);
                if (n <= 0) continue;
                buf[n] = '\0';
                buf[strcspn(buf, "\r\n")] = '\0';

                char ipbuf[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &origen.sin_addr, ipbuf, sizeof(ipbuf));
                int puerto_origen = ntohs(origen.sin_port);

                log_evento(ipbuf, puerto_origen, "RECV(UDP)", buf);

                char copia[BUF_SIZE];
                strncpy(copia, buf, sizeof(copia) - 1);
                copia[sizeof(copia) - 1] = '\0';

                char tipo[32];
                Field campos[MAX_FIELDS];
                int nc = parsear_mensaje(copia, tipo, campos, MAX_FIELDS);

                if (nc >= 0 && strcmp(tipo, "STATUS") == 0) {
                    const char *node_id = obtener_campo(campos, nc, "node_id");
                    const char *cpu_s = obtener_campo(campos, nc, "cpu");
                    const char *temp_s = obtener_campo(campos, nc, "temp");
                    const char *bat_s = obtener_campo(campos, nc, "battery");
                    if (node_id && cpu_s && temp_s && bat_s) {
                        actualizar_status(node_id, atof(cpu_s), atof(temp_s), atof(bat_s));
                    }
                }

            } else {
                /* datos de un cliente TCP ya conectado */
                ClientConn *cliente = buscar_cliente_por_fd(fd);
                char buf[BUF_SIZE];
                ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);

                if (n <= 0) {
                    /* cliente cerro la conexion */
                    if (cliente) {
                        log_evento(cliente->ip, cliente->puerto, "DESCONEXION", "cliente cerro la conexion");
                        liberar_cliente(fd);
                    }
                    close(fd);
                    FD_CLR(fd, &master);
                    continue;
                }

                buf[n] = '\0';
                buf[strcspn(buf, "\r\n")] = '\0';

                if (!cliente) continue; /* no deberia pasar */

                log_evento(cliente->ip, cliente->puerto, "RECV(TCP)", buf);

                char copia[BUF_SIZE];
                strncpy(copia, buf, sizeof(copia) - 1);
                copia[sizeof(copia) - 1] = '\0';

                char respuesta[BUF_SIZE];
                procesar_mensaje_tcp(copia, cliente, respuesta, sizeof(respuesta));

                send(fd, respuesta, strlen(respuesta), 0);

                char resp_log[BUF_SIZE];
                strncpy(resp_log, respuesta, sizeof(resp_log) - 1);
                resp_log[strcspn(resp_log, "\r\n")] = '\0';
                log_evento(cliente->ip, cliente->puerto, "SEND(TCP)", resp_log);
            }
        }
    }

    if (archivo_log) fclose(archivo_log);
    close(fd_tcp);
    close(fd_udp);
    return 0;
}
