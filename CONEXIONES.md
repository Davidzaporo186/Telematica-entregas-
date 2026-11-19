# Cableado registrado en el archivo .pkt

| Equipo A | Puerto A | Equipo B | Puerto B |
|---|---|---|---|
| PC10 | `FastEthernet0` | SW1 | `FastEthernet0/2` |
| PC20 | `FastEthernet0` | SW1 | `FastEthernet0/3` |
| R1 | `GigabitEthernet0/0` | SW1 | `FastEthernet0/1` |
| R2 | `GigabitEthernet0/0` | DHCP | `FastEthernet0` |
| R6 | `GigabitEthernet0/1` | Sw99 | `FastEthernet0/1` |
| DNS | `FastEthernet0` | Sw99 | `FastEthernet0/2` |
| WEB | `FastEthernet0` | Sw99 | `FastEthernet0/3` |
| R1 | `Serial0/0/0` | R2 | `Serial0/0/0` |
| R1 | `Serial0/0/1` | R3 | `Serial0/0/0` |
| R2 | `Serial0/0/1` | R4 | `Serial0/0/0` |
| R3 | `Serial0/0/1` | R4 | `Serial0/0/1` |
| R6 | `Serial0/0/0` | R5 | `Serial0/0/0` |
| R6 | `Serial0/0/1` | R7 | `Serial0/0/0` |
| R4 | `GigabitEthernet0/0` | R6 | `GigabitEthernet0/0` |
| R5 | `GigabitEthernet0/0` | R7 | `GigabitEthernet0/0` |
