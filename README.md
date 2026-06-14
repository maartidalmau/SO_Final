# Maester

## Compilar

```bash
cd Code
make
```

## Ejecutar

```bash
./maester <fichero_config.dat> <fichero_stock.db>
```

Atajos para los reinos de ejemplo (desde `Code/`):

```bash
make A   # Dragonstone
make B   # KingsLanding
make C   # TheVale
make D   # Driftmark
make E   # Riverrun
```

> La IP indicada en el `.dat` de cada reino debe existir en una interfaz de la máquina
> donde se ejecuta (si no, falla el `bind()`). Las configuraciones de ejemplo asumen 3
> hosts: `192.168.1.3`, `192.168.1.4` y `192.168.1.5`.

## Comandos de la consola

| Comando | Descripción |
|---|---|
| `LIST REALMS` | Lista los reinos conocidos |
| `LIST PRODUCTS [reino]` | Inventario propio, o catálogo de un reino aliado |
| `PLEDGE <reino> <sello.png>` | Envía una petición de alianza |
| `PLEDGE STATUS` | Estado de las alianzas |
| `PLEDGE RESPOND <reino> <ACCEPT\|REJECT>` | Responde a una petición de alianza |
| `START TRADE <reino>` | Modo comercio con un reino aliado |
| `ENVOY STATUS` | Estado de los procesos envoy |
| `PING <reino>` | Mide la latencia hasta un reino |
| `EXIT` | Cierra el Maester |

## Limpiar

```bash
make clean
```
