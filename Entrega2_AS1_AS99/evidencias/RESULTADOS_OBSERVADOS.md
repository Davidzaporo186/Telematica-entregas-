# Resultados observados en Packet Tracer

Este archivo recoge resultados que el estudiante mostró o comunicó durante la configuración en la conversación. No equivale a una segunda ejecución independiente de la simulación.

| Comprobación | Resultado observado | Fuente |
|---|---|---|
| R2 → R1 (`ping 172.20.1.1`) | 5/5, 100 % | Captura `03_ping_R2_a_R1.png` |
| PC10 → gateway (`ping 10.0.0.1`) | 4/4, 100 % | Captura `04_ping_PC10_a_gateway.png` |
| PC10 → DHCP (`ping 192.168.10.10`) | 3/4, 75 % | Captura `05_ping_PC10_a_DHCP.png` |
| DHCP VLAN10 y VLAN20 | Dos pools presentes | Captura `02_DHCP_pools.png` |
| DHCP en PC10 y PC20 | Asignación automática reportada exitosa | Confirmación del estudiante |
| WEB desde PC10 | Abrió `http://www.entrega2.test` | Captura `06_HTTP_PC10.png` |
| WEB desde PC20 | Abrió por nombre de dominio | Confirmación del estudiante |
| OSPF en R1 | R2 `2.2.2.2` FULL; R3 `3.3.3.3` FULL | Salida CLI facilitada |
| OSPF en R6 | R5 `5.5.5.5` FULL; R7 `7.7.7.7` FULL | Salida CLI facilitada |
| BGP en R4 | AS local 1, vecino `192.0.2.2` AS99, 4 prefijos | Salida CLI facilitada |
| Ruta hacia AS99 en R1 | `O E2 198.51.100.0/24` vía R2 y R3 | Salida CLI facilitada |
| BGP en R6 | Rutas cliente `10.0.0.0`, `172.16.0.0`, `192.168.10.0/24` vía `192.0.2.1` | Salida CLI facilitada |

**Límite de validación:** se comprobó que el `.pkt` se puede descifrar e inspeccionar y se verificaron sus configuraciones guardadas; no se ejecutó Cisco Packet Tracer desde el entorno de generación de este repositorio.
