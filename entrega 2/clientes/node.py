"""
Simulador de nodo (sensor) - Fase 2

Uso:
    python3 node.py <servidor> <puerto_tcp> <node_id>

- Se registra por TCP (REGISTER) una sola vez.
- Manda telemetria por UDP (STATUS) cada 3 segundos - se puede perder, no importa.
- Si la temperatura simulada supera 40, manda un EVENT por TCP y espera confirmacion.
"""

import socket
import sys
import time
import random


def resolver_servidor(nombre):
    """Resuelve el nombre del servidor por DNS. Si falla, se maneja la excepcion
    y el programa termina de forma controlada en lugar de reventar."""
    try:
        return socket.gethostbyname(nombre)
    except socket.gaierror as e:
        print(f"No se pudo resolver el servidor '{nombre}': {e}")
        sys.exit(1)


def main():
    if len(sys.argv) < 4:
        print("Uso: python3 node.py <servidor> <puerto_tcp> <node_id>")
        sys.exit(1)

    servidor = sys.argv[1]
    puerto_tcp = int(sys.argv[2])
    node_id = sys.argv[3]
    puerto_udp = puerto_tcp + 1

    ip_servidor = resolver_servidor(servidor)

    # --- canal TCP: registro y eventos criticos ---
    tcp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    tcp_sock.connect((ip_servidor, puerto_tcp))

    registro = f"REGISTER;node_id={node_id};tipo=iot_sensor\n"
    tcp_sock.sendall(registro.encode())
    respuesta = tcp_sock.recv(1024).decode().strip()
    print(f"[{node_id}] Registro -> {respuesta}")

    if "status=OK" not in respuesta:
        print(f"[{node_id}] El servidor rechazo el registro, se detiene el nodo.")
        tcp_sock.close()
        sys.exit(1)

    # --- canal UDP: telemetria periodica ---
    udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    try:
        while True:
            cpu = round(random.uniform(10, 90), 1)
            temp = round(random.uniform(20, 45), 1)
            battery = round(random.uniform(50, 100), 1)

            status = f"STATUS;node_id={node_id};cpu={cpu};temp={temp};battery={battery}\n"
            udp_sock.sendto(status.encode(), (ip_servidor, puerto_udp))
            print(f"[{node_id}] STATUS enviado -> cpu={cpu} temp={temp} battery={battery}")

            if temp > 40:
                evento = f"EVENT;node_id={node_id};tipo=ALARMA;valor={temp}\n"
                tcp_sock.sendall(evento.encode())
                resp = tcp_sock.recv(1024).decode().strip()
                print(f"[{node_id}] EVENT enviado (temp={temp}) -> {resp}")

            time.sleep(3)

    except KeyboardInterrupt:
        print(f"\n[{node_id}] Nodo detenido por el usuario.")
    finally:
        tcp_sock.close()
        udp_sock.close()


if __name__ == "__main__":
    main()
