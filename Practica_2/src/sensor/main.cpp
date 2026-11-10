/* ----------------------------------------------------------------------
*  Dispositivo sensor
*    Recibe comandos del supervisor por Serial1, los ejecuta sobre los
*    SRF02 (I2C) y le devuelve las respuestas por Serial1.
*
*    Hebras: reader (Serial1 -> mailbox de cada sensor),
*            sensor 1 y sensor 2 (I2C),
*            writer (mailbox -> Serial1)
*
*  Asignatura (GII-IC)
* ----------------------------------------------------------------------
*/

#include <Arduino.h>
#include <Wire.h>
#include "messages.h"


constexpr const uint32_t serial_monitor_bauds=115200;
constexpr const uint32_t MONITOR_WAIT_MS=2000;


// ----------------------------------------------------------------------
// Continúa setup() tras chBegin(): comprueba los sensores y lanza las hebras
// ----------------------------------------------------------------------
void chSetup(){
    for (int i = 0; i < N_SENSORS; i++) {
        Serial.print("Sensor "); Serial.print(i + 1);
        Serial.print(" (0x"); Serial.print(sensor_addr[i], HEX); Serial.print("): ");
        Serial.println(check_sensor(sensor_addr[i]) ? "OK" : "NO RESPONDE");
    }
    //check_screen(screen);

    // El writer primero: las demás hebras le mandan sus respuestas
    startWriter();
    startSensor();
    //StartScreen();
    // El reader el último: cuando llegue un comando todo está ya en marcha
    startReader();
}


// ----------------------------------------------------------------------
// setup() activa los dispositivos y arranca ChibiOS
// ----------------------------------------------------------------------
void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);

    // Inicialización del puerto para el serial monitor
    Serial.begin(serial_monitor_bauds);
    // Espera al monitor como mucho MONITOR_WAIT_MS: si la placa solo está
    // conectada al supervisor (sin PC), arranca igualmente
    uint32_t start_ms = millis();
    while (!Serial && millis() - start_ms < MONITOR_WAIT_MS);

    //Comunicación entre sensores
    Wire.begin();

    // Inicialización del puerto de comunicaciones con el otro dispositivo MKR
    Serial1.begin(PROTO_BAUD);

    // chBegin() resetea el stack y nunca retorna.
    chBegin(chSetup);
}

// ----------------------------------------------------------------------
// loop() actúa como hebra idle (NORMALPRIO): en lib/ChRt-master,
// CH_CFG_NO_IDLE_THREAD es TRUE, así que ChibiOS no crea la suya.
// NO debe dormir ni esperar: si todas las hebras quedan bloqueadas a la
// vez, el planificador no tiene nada que ejecutar y la placa se cuelga.
// ----------------------------------------------------------------------
void loop() {}
