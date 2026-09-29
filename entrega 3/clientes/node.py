"""
Simulador de nodo (sensor) - Fase 3

Uso:
    python3 node.py <servidor> <puerto_tcp> <node_id>

- Se registra por TCP (REGISTER) una sola vez, con reintentos si el servidor
  no esta arriba todavia.
- Manda telemetria por UDP (STATUS) cada 3 segundos - se puede perder, no importa,
  por eso no se le pone ni timeout ni reintento.
- Si la temperatura simulada supera 40, manda un EVENT por TCP y espera
  confirmacion; si no llega a tiempo (timeout) o la conexion se cae, reintenta
  hasta MAX_REINTENTOS veces antes de continuar sin bloquear el programa.
"""

import socket
import sys
import time
import random

TIMEOUT_RESPUESTA_SEG = 3
MAX_REINTENTOS = 3


def resolver_servidor(nombre):
    """Resuelve el nombre del servidor por DNS. Si falla, se maneja la excepcion
    y el programa termina de forma controlada en lugar de reventar."""
    try:
        return socket.gethostbyname(nombre)
    except socket.gaierror as e:
        print(f"No se pudo resolver el servidor '{nombre}': {e}")
        sys.exit(1)


def conectar_tcp(ip_servidor, puerto_tcp, node_id):
    """Intenta conectar por TCP, con reintentos si el servidor todavia no
    esta arriba (ConnectionRefusedError) o si hay un fallo de red (OSError)."""
    intento = 1
    while True:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(TIMEOUT_RESPUESTA_SEG)
            sock.connect((ip_servidor, puerto_tcp))
            return sock
        except (ConnectionRefusedError, OSError, socket.timeout) as e:
            print(f"[{node_id}] No se pudo conectar al servidor (intento {intento}): {e}")
            if intento >= MAX_REINTENTOS:
                print(f"[{node_id}] Se agotaron los reintentos de conexion. Se detiene el nodo.")
                sys.exit(1)
            intento += 1
            time.sleep(2)


def registrar(tcp_sock, node_id):
    """Manda REGISTER y espera RESPONSE. Si hay timeout o el servidor no
    confirma, avisa con claridad en vez de quedarse esperando para siempre."""
    registro = f"REGISTER;node_id={node_id};tipo=iot_sensor\n"
    try:
        tcp_sock.sendall(registro.encode())
        respuesta = tcp_sock.recv(1024).decode().strip()
    except socket.timeout:
        print(f"[{node_id}] El servidor no respondio el REGISTER a tiempo.")
        return False
    except OSError as e:
        print(f"[{node_id}] Fallo de comunicacion al registrarse: {e}")
        return False

    print(f"[{node_id}] Registro -> {respuesta}")
    return "status=OK" in respuesta


def mandar_evento_con_reintentos(tcp_sock, node_id, temp):
    """Manda EVENT y espera confirmacion. Si se cae la conexion o expira el
    timeout esperando la respuesta, reintenta hasta MAX_REINTENTOS veces
    antes de rendirse (sin tumbar el programa)."""
    evento = f"EVENT;node_id={node_id};tipo=ALARMA;valor={temp}\n"

    for intento in range(1, MAX_REINTENTOS + 1):
        try:
            tcp_sock.sendall(evento.encode())
            resp = tcp_sock.recv(1024).decode().strip()
            print(f"[{node_id}] EVENT enviado (temp={temp}) -> {resp}")
            return True
        except socket.timeout:
            print(f"[{node_id}] Timeout esperando confirmacion del EVENT (intento {intento})")
        except OSError as e:
            print(f"[{node_id}] Fallo de comunicacion mandando EVENT (intento {intento}): {e}")

    print(f"[{node_id}] No se pudo confirmar el EVENT despues de {MAX_REINTENTOS} intentos.")
    return False


def main():
    if len(sys.argv) < 4:
        print("Uso: python3 node.py <servidor> <puerto_tcp> <node_id>")
        sys.exit(1)

    servidor = sys.argv[1]
    puerto_tcp = int(sys.argv[2])
    node_id = sys.argv[3]
    puerto_udp = puerto_tcp + 1

    ip_servidor = resolver_servidor(servidor)

    tcp_sock = conectar_tcp(ip_servidor, puerto_tcp, node_id)

    if not registrar(tcp_sock, node_id):
        print(f"[{node_id}] No quedo registrado. Se detiene el nodo.")
        tcp_sock.close()
        sys.exit(1)

    # canal UDP: telemetria periodica, sin timeout ni reintento a proposito
    # (si un datagrama se pierde, no pasa nada, el proximo llega en 3 segundos)
    udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    try:
        while True:
            cpu = round(random.uniform(10, 90), 1)
            temp = round(random.uniform(20, 45), 1)
            battery = round(random.uniform(50, 100), 1)

            status = f"STATUS;node_id={node_id};cpu={cpu};temp={temp};battery={battery}\n"
            try:
                udp_sock.sendto(status.encode(), (ip_servidor, puerto_udp))
                print(f"[{node_id}] STATUS enviado -> cpu={cpu} temp={temp} battery={battery}")
            except OSError as e:
                print(f"[{node_id}] No se pudo enviar STATUS (se ignora, es UDP): {e}")

            if temp > 40:
                mandar_evento_con_reintentos(tcp_sock, node_id, temp)

            time.sleep(3)

    except KeyboardInterrupt:
        print(f"\n[{node_id}] Nodo detenido por el usuario.")
    finally:
        tcp_sock.close()
        udp_sock.close()


if __name__ == "__main__":
    main()
