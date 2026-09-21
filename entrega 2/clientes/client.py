"""
Cliente de administracion - Fase 2

Uso:
    python3 client.py <servidor> <puerto_tcp>

Hace LOGIN y despues permite consultar el estado / historial de un nodo (QUERY)
las veces que se quiera, hasta que el usuario escriba "salir".
"""

import socket
import sys


def resolver_servidor(nombre):
    try:
        return socket.gethostbyname(nombre)
    except socket.gaierror as e:
        print(f"No se pudo resolver el servidor '{nombre}': {e}")
        sys.exit(1)


def main():
    if len(sys.argv) < 3:
        print("Uso: python3 client.py <servidor> <puerto_tcp>")
        sys.exit(1)

    servidor = sys.argv[1]
    puerto_tcp = int(sys.argv[2])
    ip_servidor = resolver_servidor(servidor)

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((ip_servidor, puerto_tcp))

    usuario = input("Usuario: ")
    clave = input("Clave: ")

    sock.sendall(f"LOGIN;user={usuario};clave={clave}\n".encode())
    respuesta = sock.recv(1024).decode().strip()
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

        sock.sendall(f"QUERY;node_id={node_id};n={n}\n".encode())
        respuesta = sock.recv(1024).decode().strip()
        print("Servidor:", respuesta)

    sock.close()
    print("Sesion cerrada.")


if __name__ == "__main__":
    main()
