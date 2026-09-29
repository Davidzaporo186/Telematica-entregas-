"""
Cliente de administracion - Fase 3

Uso:
    python3 client.py <servidor> <puerto_tcp>

Hace LOGIN y despues permite consultar el estado/historial de un nodo (QUERY)
las veces que se quiera, hasta que el usuario escriba "salir". Maneja el caso
de que el servidor no este arriba, se caiga a media sesion, o no responda a
tiempo, sin que el programa se caiga.
"""

import socket
import sys

TIMEOUT_RESPUESTA_SEG = 5


def resolver_servidor(nombre):
    try:
        return socket.gethostbyname(nombre)
    except socket.gaierror as e:
        print(f"No se pudo resolver el servidor '{nombre}': {e}")
        sys.exit(1)


def conectar(ip_servidor, puerto_tcp):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(TIMEOUT_RESPUESTA_SEG)
    try:
        sock.connect((ip_servidor, puerto_tcp))
        return sock
    except (ConnectionRefusedError, OSError, socket.timeout) as e:
        print(f"No se pudo conectar al servidor {ip_servidor}:{puerto_tcp} -> {e}")
        sys.exit(1)


def enviar_y_recibir(sock, mensaje):
    """Manda un mensaje y espera respuesta, sin dejar que un timeout o una
    caida de conexion tumben el programa completo."""
    try:
        sock.sendall(mensaje.encode())
        return sock.recv(1024).decode().strip()
    except socket.timeout:
        return None
    except OSError:
        return None


def main():
    if len(sys.argv) < 3:
        print("Uso: python3 client.py <servidor> <puerto_tcp>")
        sys.exit(1)

    servidor = sys.argv[1]
    puerto_tcp = int(sys.argv[2])
    ip_servidor = resolver_servidor(servidor)

    sock = conectar(ip_servidor, puerto_tcp)

    usuario = input("Usuario: ")
    clave = input("Clave: ")

    respuesta = enviar_y_recibir(sock, f"LOGIN;user={usuario};clave={clave}\n")
    if respuesta is None:
        print("El servidor no respondio (timeout o conexion perdida). Intenta de nuevo mas tarde.")
        sock.close()
        return

    print("Servidor:", respuesta)
    if "status=OK" not in respuesta:
        print("Login fallido, se cierra el cliente.")
        sock.close()
        return

    print("\nListo. Puedes consultar nodos (escribe 'salir' para terminar).")

    while True:
        node_id = input("\nID del nodo a consultar: ").strip()
        if node_id.lower() == "salir":
            break

        n = input("Cuantos datos historicos quieres (1-5, default 1): ").strip() or "1"

        respuesta = enviar_y_recibir(sock, f"QUERY;node_id={node_id};n={n}\n")
        if respuesta is None:
            print("El servidor no respondio (timeout o se cayo la conexion). Se cierra el cliente.")
            break

        print("Servidor:", respuesta)

    sock.close()
    print("Sesion cerrada.")


if __name__ == "__main__":
    main()
