//------------------------------------------------------------------------------
// messages.h - Lo que comparten las hebras del sensor (como config.h en el
// ejemplo 2 de ChibiOS): tipos de los mensajes, mailboxes y funciones start
//------------------------------------------------------------------------------
#ifndef MESSAGES_H
#define MESSAGES_H

#include <Arduino.h>
#include <ChRt.h>
#include "../common/protocol.h"


#define N_SENSORS 2
#define N_SLOTS   8   // huecos de cada mailbox/memory pool





static inline msg_t packCommand(uint8_t operation, uint16_t value)
{
    return ((msg_t)value << 8) | operation;
}
static inline uint8_t  cmdOperation(msg_t m) { return m & 0x0F; }
static inline uint16_t cmdValue(msg_t m)     { return (m >> 8) & 0xFFFF; }







// Respuesta: sensor/reader/screen -> writer, ya lista para enviar por Serial1.
// aligned: el memory pool guarda un puntero dentro de cada objeto libre, así
// que cada objeto debe empezar en una dirección múltiplo de 4
typedef struct __attribute__((aligned(PORT_NATURAL_ALIGN))) {
    uint8_t head;      // cabecera del protocolo (generateHead)
    uint8_t len;       // bytes de body (no viaja: el supervisor lo deduce de la cabecera)
    uint8_t body[PROTO_MAX_BODY];
} response_t;

// Estado de un SRF02 para la pantalla: sensor -> screen. Lo manda cada hebra
// sensor al arrancar, al aplicar un comando y al medir
#define NO_MEASURE     0   // result: no se ha medido
#define MEASURE_OK     1   // result: value es una medida válida
#define MEASURE_ERROR  2   // result: la medida falló (I2C)

// event: cambio de estado que no es una acción del usuario (p. ej. el estado
// inicial al arrancar). La pantalla lo guarda pero no lo pinta como "Ultima
// accion". Fuera del rango de operaciones del protocolo (4 bits: 0x0-0xF)
#define NO_SHOW_OPERATION  0xFF

typedef struct {
    uint8_t  sensor;      // 1 o 2, como en el protocolo
    uint8_t  event;       // operación que lo provoca (protocol.h) o NO_SHOW_OPERATION
    uint8_t  unit;        // PROTO_UNIT_*
    uint8_t  periodic;    // 1 si la medida periódica está activa
    uint8_t  result;      // NO_MEASURE, MEASURE_OK o MEASURE_ERROR
    uint16_t delay_ms;    // retardo mínimo entre disparos
    uint16_t period_ms;   // periodo de la medida periódica
    uint16_t value;       // medida, si result es MEASURE_OK
} sensor_state_t;

// Mensaje para la pantalla: reader -> screen (comandos oled del supervisor)
// y sensor -> screen (SCREEN_SENSOR_STATE). aligned: igual que response_t
#define SCREEN_SENSOR_STATE 0xF0   // código interno, no viaja por Serial1

typedef struct __attribute__((aligned(PORT_NATURAL_ALIGN))) {
    uint8_t op;              // OP_OLED_ON, OP_OLED_OFF o SCREEN_SENSOR_STATE
    sensor_state_t state;    // solo con SCREEN_SENSOR_STATE
} screen_msg_t;

// ----- sensor.cpp ------------------------------------------------------------
extern const uint8_t sensor_addr[N_SENSORS];   // dirección I2C de cada sensor
extern mailbox_t *sensor_mb[N_SENSORS];        // sensor 1 -> [0], sensor 2 -> [1]
extern mutex_t i2c_mtx;                        // bus I2C: SRF02 y pantalla

bool checkSensor(byte address);
void startSensor();

// ----- screen.cpp ------------------------------------------------------------
uint8_t check_screen();   // dirección I2C de la pantalla, 0 si no responde
bool post_screen_command(uint8_t op);
void post_sensor_state(const sensor_state_t *state);
void startScreen();

// ----- reader.cpp ------------------------------------------------------------
void startReader();

// ----- writer.cpp ------------------------------------------------------------
void send2Writer(uint8_t head, const uint8_t *body, uint8_t len);
void send_ack(uint8_t sensor, uint8_t operation);
void send_error(uint8_t sensor, uint8_t operation, uint8_t code);
void send_measure(uint8_t sensor, uint8_t operation, uint8_t unit, uint16_t value);
void startWriter();

#endif
