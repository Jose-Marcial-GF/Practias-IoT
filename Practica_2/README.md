# Práctica 2 del guion (carpeta `Practica_3` del repo) — Supervisor y sensor por Serial1

En el repo, `Practica_1` es la 1.A y `Practica_2` es la 1.B; esta es la siguiente carpeta.

Ejercicio propuesto del guion, con los dos opcionales elegidos: pantalla OLED y ChibiOS.
Dos placas Arduino MKR WAN 1310 conectadas por `Serial1`:

- **Supervisor**: recibe comandos por el monitor serie, los envía como mensajes binarios y muestra las respuestas. Es el firmware de `src/supervisor/`.
- **Sensor**: atiende los mensajes, maneja los SRF02 y la pantalla OLED. Es el firmware de `src/sensor/` (lo hace José).

Los dos comparten el contrato `src/common/protocol.h`. **Si se cambia algo ahí, se cambia para los dos.**

## Estructura

```
Practica_3/
├── .gitignore, .vscode/      igual que en las otras prácticas
├── platformio.ini            dos entornos: supervisor y sensor
├── lib/ChRt-master/          librería ChibiOS (la misma que en Practica_2)
├── src/
│   ├── common/protocol.h     códigos, unidades, errores y reglas del protocolo
│   ├── supervisor/
│   │   ├── main.cpp          setup(), arranque de ChibiOS
│   │   ├── config.h          constantes, tiempos y prioridades
│   │   ├── threads.h         declaración de las hebras
│   │   ├── console.cpp       hebra de consola: lee comandos y envía peticiones
│   │   ├── rx.cpp            hebra de recepción: lee Serial1
│   │   ├── commands.*        interpreta las líneas escritas por el usuario
│   │   ├── link.*            codifica y decodifica los mensajes binarios
│   │   ├── exchange.*        empareja cada petición con su respuesta
│   │   ├── reply.*           da formato a las respuestas
│   │   ├── output.*          salida por USB protegida con un cerrojo
│   │   └── idle.cpp          loop() vacío (hebra idle)
│   └── sensor/               firmware del sensor (pendiente)
└── README.md
```

## Compilar y cargar

Por defecto solo se compila el supervisor (`default_envs` en `platformio.ini`); cuando exista el firmware del sensor, añadirlo ahí.

```
pio run -e supervisor -t upload --upload-port <puerto del supervisor>
pio run -e sensor     -t upload --upload-port <puerto del sensor>
```

Cada placa tiene su puerto. Abrir el monitor serie (9600) solo en la del supervisor.

## Conexión

| Supervisor | Sensor |
|------------|--------|
| TX (Serial1) | RX (Serial1) |
| RX (Serial1) | TX (Serial1) |
| GND | GND |

En la MKR WAN 1310, `Serial1` usa RX = pin 13 y TX = pin 14 (comprobar con el pinout de la placa). Velocidad: 115200 8N1.

## Comandos del supervisor

| Comando | Acción |
|---------|--------|
| `help` | muestra la ayuda |
| `us` | lista los sensores disponibles |
| `us <n> one-shot` | un disparo del sensor n |
| `us <n> on <ms>` | disparo periódico cada `ms` (1-65535) |
| `us <n> off` | detiene el disparo periódico |
| `us <n> unit inc\|cm\|ms` | unidad de la medida (`ms` = tiempo del eco en µs) |
| `us <n> delay <ms>` | retardo mínimo entre disparos |
| `us <n> status` | dirección, retardo, unidad y modo periódico |
| `oled text <línea> <texto>` | escribe en la pantalla (línea desde 0, máx. 32 caracteres) |
| `oled clear` | borra la pantalla |
| `oled mode auto\|manual` | auto: la pantalla muestra el estado de los sensores |

Las medidas periódicas aparecen solas con el prefijo `[periódica]`.

## Lo que debe cumplir el sensor

Está explicado con más detalle en `src/common/protocol.h`. Lo importante:

1. Cada mensaje es `[código][longitud][cuerpo…]`, con enteros de 16 bits en little-endian.
2. **Exactamente una respuesta por petición**: `ACK`, `ERROR` o la respuesta con datos.
3. Las medidas periódicas (`MEASURE`) se envían sin que nadie las pida.
4. No dejar más de 100 ms entre dos bytes de un mismo mensaje; el supervisor lo descarta.
5. El sensor se identifica por su número de orden (1..255), no por la dirección I2C.
6. La unidad `ms` devuelve el tiempo del eco en microsegundos.
7. La línea de la OLED empieza en 0.

## Limitación conocida

Si el sensor n está en modo periódico y se pide un `one-shot` del mismo sensor, la medida periódica que llegue justo antes puede interpretarse como la respuesta al one-shot. El mensaje es igualmente una medida válida del mismo sensor, así que no se pierde información.
