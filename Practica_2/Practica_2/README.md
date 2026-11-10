# Práctica 2 — Supervisor y sensor por Serial1

Dos placas Arduino MKR WAN 1310 conectadas por `Serial1`:

- **Supervisor** (`src/supervisor/`): lee comandos en el monitor serie, los envía como mensajes binarios y muestra todo lo que contesta el sensor.
- **Sensor** (`src/sensor/`): atiende los mensajes, maneja los SRF02 y la pantalla OLED.


Los dos firmwares comparten `src/common/protocol.h`, que define el formato de los mensajes. Si se cambia algo ahí, se cambia para los dos.

## Estructura

```
├── platformio.ini            dos entornos: supervisor y sensor
├── lib/ChRt-master/          librería ChibiOS
├── src/
│   ├── common/protocol.h     códigos de operación, unidades y errores
│   ├── supervisor/
│   │   ├── main.cpp          setup(), arranque de ChibiOS y loop() vacío
│   │   ├── config.h/.cpp     constantes y cerrojo del monitor serie
│   │   ├── threads.h         funciones que arrancan las hebras
│   │   ├── console.cpp       hebra consola: teclado, interpretación y envío
│   │   └── receiver.cpp      hebra receptor: lee Serial1 y muestra los mensajes
│   └── sensor/               firmware del sensor
└── README.md
```

## Compilar y cargar

```
pio run -e supervisor -t upload --upload-port <puerto del supervisor>
pio run -e sensor     -t upload --upload-port <puerto del sensor>
```

Por defecto solo se compila el supervisor (`default_envs` en `platformio.ini`); cuando exista el firmware del sensor, hay que añadirlo ahí.

Se abre el monitor serie (9600) solo en la placa del supervisor. El supervisor ya muestra lo que se escribe, así que no hay que activar el eco del monitor (`monitor_echo`).

## Conexión

| Supervisor | Sensor |
|------------|--------|
| TX (Serial1) | RX (Serial1) |
| RX (Serial1) | TX (Serial1) |
| GND | GND |

En la MKR WAN 1310, `Serial1` usa RX = pin 13 y TX = pin 14 (comprobar con el pinout de la placa). Velocidad: 115200, 8N1.

## Comandos

| Comando | Acción |
|---------|--------|
| `help` | muestra la ayuda |
| `us` | lista los sensores disponibles |
| `us <n> one-shot` | una medida del sensor n |
| `us <n> on <ms>` | medida periódica cada `ms` |
| `us <n> off` | detiene la medida periódica |
| `us <n> unit inc\|cm\|ms` | unidad de la medida (`ms` = tiempo del eco en µs) |
| `us <n> delay <ms>` | retardo mínimo entre medidas |
| `us <n> status` | dirección, retardo, unidad y modo periódico |
| `oled text <línea> <texto>` | escribe en la pantalla (línea 0 = la primera, máximo 32 caracteres) |
| `oled clear` | borra la pantalla |
| `oled mode auto\|manual` | auto: la pantalla muestra el estado de los sensores |

Se puede borrar con la tecla de retroceso. El supervisor no valida los valores (número de sensor, periodos...): si algo es incorrecto, el sensor contesta `ERROR`.

## Cómo funciona el supervisor

Hay dos hebras de ChibiOS, más `loop()` vacío como hebra idle:

- **Consola** (`NORMALPRIO + 1`): lee los caracteres del monitor, los muestra, y al pulsar Enter convierte la línea en un mensaje `[código][longitud][cuerpo]` y lo escribe en `Serial1`. No espera la respuesta.
- **Receptor** (`NORMALPRIO + 2`): lee `Serial1` byte a byte, reconstruye cada mensaje completo y lo muestra. Así se muestran igual las respuestas a los comandos y las medidas periódicas, que llegan solas.

Las dos hebras escriben en el monitor serie, por eso comparten un cerrojo (`usbMutex`): un mensaje se escribe entero antes de que la otra hebra escriba.

## Lo que debe cumplir el sensor

Los detalles están en `src/common/protocol.h`. Lo importante:

1. Cada mensaje es `[código][longitud][cuerpo…]`, con enteros de 16 bits con el byte bajo primero.
2. Una única respuesta por petición: `ACK`, `ERROR` o la respuesta con datos.
3. Las medidas periódicas (`MEASURE`) se envían solas, sin petición.
4. El sensor se identifica por su número de orden (1, 2...), no por la dirección I2C.
5. La unidad `ms` devuelve el tiempo del eco en microsegundos.
6. La línea de la pantalla empieza en 0.
7. El sensor valida los parámetros y contesta `ERROR` si no son válidos.
