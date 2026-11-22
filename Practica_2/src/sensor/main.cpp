/* ----------------------------------------------------------------------------
 *  Práctica 2 - Dispositivo sensor - Arduino MKR WAN 1310 + ChibiOS
 *
 *  1. Comprueba al arrancar qué SRF02 y qué pantalla OLED contestan en el I2C.
 *  2. Recibe por Serial1 los comandos binarios del supervisor (protocol.h).
 *  3. Cada SRF02 tiene su hebra: aplica los comandos, mide (one-shot o
 *     periódico) y contesta con ACK, error o la medida.
 *  4. La pantalla OLED muestra en todo momento la configuración y la última
 *     medida de cada sensor y la última acción realizada.
 *  5. Hebras (se comunican con mailboxes + memory pools):
 *     - reader: lee Serial1 y reparte cada comando al sensor o a la pantalla
 *     - sensor: una por SRF02, ejecuta los comandos y mide
 *     - screen: pinta la pantalla OLED
 *     - writer: única hebra que escribe en Serial1 (respuestas al supervisor)
 *     El bus I2C lo comparten sensores y pantalla, protegido con i2c_mtx.
 *
 *  Conexión de Serial1 con el supervisor: TX con RX cruzados y GND común.
 *
 *  Autores:
 *  Juan Daniel López Melainine
 *  José Marcial Galván Franco
 * ----------------------------------------------------------------------------
 */

#include <Arduino.h>
#include <Wire.h>
#include "messages.h"

// Parámetros
constexpr const uint32_t serial_monitor_bauds = 115200;   // monitor de depuración
constexpr const uint32_t MONITOR_WAIT_MS      = 2000;     // espera máxima al monitor


// Continúa el setup() ya dentro de ChibiOS: comprueba el I2C y lanza las hebras
void chSetup(){
    // Qué SRF02 contestan
    for (int i = 0; i < N_SENSORS; i++) {
        Serial.print("Sensor "); Serial.print(i + 1);
        Serial.print(" (0x"); Serial.print(sensor_addr[i], HEX); Serial.print("): ");
        Serial.println(checkSensor(sensor_addr[i]) ? "OK" : "NO RESPONDE");
    }

    // Si contesta la pantalla (si no, su hebra la sigue buscando)
    uint8_t screen_addr = check_screen();
    Serial.print("Pantalla");
    if (screen_addr != 0) {
        Serial.print(" (0x"); Serial.print(screen_addr, HEX); Serial.println("): OK");
    } else {
        Serial.println(": NO RESPONDE");
    }

    // El orden importa: primero quien recibe mensajes, el reader el último
    startWriter();   // las demás hebras le mandan sus respuestas
    startScreen();   // los sensores le mandan su estado al arrancar
    startSensor();
    startReader();   // cuando llegue un comando todo está ya en marcha
}

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);

    // Monitor de depuración: si no hay PC conectado se arranca igualmente
    Serial.begin(serial_monitor_bauds);
    uint32_t start_ms = millis();
    while (!Serial && millis() - start_ms < MONITOR_WAIT_MS);

    // Bus I2C (SRF02 y pantalla), el MKR es el maestro
    Wire.begin();

    // Puerto serie con el supervisor
    Serial1.begin(PROTO_BAUD);

    // Arranca ChibiOS y sigue en chSetup(). No retorna
    chBegin(chSetup);
}

// Hebra idle de ChibiOS: no hace nada
void loop() {}
