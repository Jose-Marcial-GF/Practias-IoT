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

// Comando: reader -> sensor
typedef struct {
    uint8_t  op;       // código de operación (one-shot, on, off, unit, delay, status)
    uint8_t  sensor;   // 1 o 2, como en el protocolo
    uint32_t arg;      // periodo, unidad, retardo...
} commands_t;

// Respuesta: sensor/reader -> writer, ya lista para enviar por Serial1.
// aligned: el memory pool guarda un puntero dentro de cada objeto libre, así
// que cada objeto debe empezar en una dirección múltiplo de 4. Sin esto la
// estructura mide 50 bytes y chPoolLoadArray() provoca un HardFault
typedef struct __attribute__((aligned(PORT_NATURAL_ALIGN))) {
    uint8_t op;
    uint8_t len;
    uint8_t body[PROTO_MAX_BODY];
} response_t;

// ----- sensor.cpp ------------------------------------------------------------
extern const uint8_t sensor_addr[N_SENSORS];   // dirección I2C de cada sensor
extern memory_pool_t commandMemoryPool;
extern mailbox_t *sensor_mb[N_SENSORS];        // sensor 1 -> [0], sensor 2 -> [1]

bool check_sensor(byte address);
void startSensor();

// ----- reader.cpp ------------------------------------------------------------
void startReader();

// ----- writer.cpp ------------------------------------------------------------
void post_to_writer(uint8_t op, const uint8_t *body, uint8_t len);
void send_ack(uint8_t op);
void send_error(uint8_t op, uint8_t error);
void send_measure(uint8_t sensor, uint8_t unit, uint16_t value);
void startWriter();

#endif
