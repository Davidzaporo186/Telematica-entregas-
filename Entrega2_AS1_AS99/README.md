# Entrega 2 — Packet Tracer: AS1 y AS99

**Interconexión de sistemas autónomos y acceso a servicios centralizados** — escenario individual (`k = 1`).

Este repositorio contiene el proyecto de red desarrollado en **Cisco Packet Tracer 9.0.1**, el informe académico y evidencias de funcionamiento obtenidas durante el montaje.

## Abrir la entrega

1. Instalar y abrir Cisco Packet Tracer (preferentemente versión 9.0.1 o compatible).
2. Abrir [`Entrega2_AS1_AS99.pkt`](Entrega2_AS1_AS99.pkt) desde **File → Open**.
3. Comprobar los equipos, las VLAN y los servicios desde las vistas **Logical** y **Services**.
4. Consultar el informe [`INFORME_ENTREGA2.pdf`](INFORME_ENTREGA2.pdf).

**No es necesario ejecutar scripts para abrir el `.pkt`.** Los archivos de `configuraciones/` son copias legibles de las configuraciones almacenadas dentro del propio proyecto.

## Estructura

```text
Entrega2_AS1_AS99.pkt       Proyecto de Packet Tracer (entregable principal)
INFORME_ENTREGA2.pdf       Informe técnico y resultados observados
INFORME_ENTREGA2.docx      Copia editable del informe
configuraciones/           Configuración IOS guardada para cada router y switch
CONEXIONES.md             Puertos y cableado extraídos del .pkt
evidencias/               Capturas reales y registro de resultados de pruebas
```

## Topología y direccionamiento

- **AS1 (cliente):** R1-R4, SW1, PC10, PC20 y servidor DHCP.
- **AS99 (proveedor):** R5-R7, SW99, servidores DNS y WEB.
- **VLAN 10:** `10.0.0.0/24`; puerta de enlace `10.0.0.1`.
- **VLAN 20:** `172.16.0.0/24`; puerta de enlace `172.16.0.1`.
- **DHCP:** `192.168.10.10` (AS1).
- **DNS:** `198.51.100.10` (AS99).
- **WEB:** `198.51.100.20` (AS99), accesible mediante `http://www.entrega2.test`.
- **Enrutamiento:** OSPF dentro de cada AS; eBGP R4 (AS1) ↔ R6 (AS99), con redistribución controlada a OSPF en el laboratorio.

La topología y las configuraciones efectivamente guardadas en el `.pkt` se usaron como fuente para construir este repositorio. Se identificaron **7 routers, 2 switches, 2 PC, 3 servidores y 15 enlaces**.

## Evidencias

Las capturas reales disponibles están en [`evidencias/`](evidencias/). El detalle de resultados observados y su procedencia está en [`evidencias/RESULTADOS_OBSERVADOS.md`](evidencias/RESULTADOS_OBSERVADOS.md).

**Nota sobre validación:** las evidencias de ping, DHCP, OSPF, BGP y HTTP proceden de pruebas que el estudiante ejecutó y reportó durante la configuración. No se presenta una simulación independiente adicional como si hubiera sido ejecutada aquí.

## Comandos de revisión

En los routers:

```text
show ip interface brief
show ip ospf neighbor
show ip route
show ip bgp summary
```

En PC10 y PC20, mediante **Desktop → Command Prompt**:

```text
ipconfig /all
ping 10.0.0.1
ping 198.51.100.20
nslookup www.entrega2.test
```

Luego abrir **Desktop → Web Browser** y visitar `http://www.entrega2.test`.

## Integridad del entregable principal

SHA-256 de `Entrega2_AS1_AS99.pkt`:

```text
0ccf2767a317e27f87b3b453d2e204f072aec3b44d518c7fe0abf6a6e92b6309
```
