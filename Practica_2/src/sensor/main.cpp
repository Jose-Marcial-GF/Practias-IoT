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

constexpr const uint32_t serial_monitor_bauds = 115200;
constexpr const uint32_t MONITOR_WAIT_MS      = 2000;  


void chSetup(){
    for (int i = 0; i < N_SENSORS; i++) {
        Serial.print("Sensor "); Serial.print(i + 1);
        Serial.print(" (0x"); Serial.print(sensor_addr[i]<<1, HEX); Serial.print("): ");
        Serial.println(checkSensor(sensor_addr[i]) ? "OK" : "NO RESPONDE");
    }

    uint8_t screen_addr = check_screen();
    Serial.print("Pantalla");
    if (screen_addr != 0) {
        Serial.print(" (0x"); Serial.print(screen_addr, HEX); Serial.println("): OK");
    } else {
        Serial.println(": NO RESPONDE");
    }

    startWriter();
    startScreen();
    startSensor();
    startReader();
}

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);

 
    Serial.begin(serial_monitor_bauds);
    uint32_t start_ms = millis();
    while (!Serial && millis() - start_ms < MONITOR_WAIT_MS);

 
    Wire.begin();

 
    Serial1.begin(PROTO_BAUD);

 
    chBegin(chSetup);
}

// Hebra idle de ChibiOS: no hace nada
void loop() {}
