#ifndef MESSAGES_H
#define MESSAGES_H

#include <Arduino.h>
#include <ChRt.h>
#include "../common/protocol.h"

#define N_SENSORS 2
#define N_SLOTS   8

static inline msg_t packCommand(uint8_t operation, uint16_t value)
{
    return ((msg_t)value << 8) | operation;
}
static inline uint8_t  cmdOperation(msg_t m) { return m & 0x0F; }
static inline uint16_t cmdValue(msg_t m)     { return (m >> 8) & 0xFFFF; }

typedef struct __attribute__((aligned(PORT_NATURAL_ALIGN))) {
    uint8_t head;
    uint8_t len;
    uint8_t body[PROTO_MAX_BODY];
} response_t;

#define NO_MEASURE     0
#define MEASURE_OK     1
#define MEASURE_ERROR  2

#define NO_SHOW_OPERATION  0xFF

typedef struct {
    uint8_t  sensor;
    uint8_t  event;
    uint8_t  unit;
    uint8_t  periodic;
    uint8_t  result;
    uint16_t delay_ms;
    uint16_t period_ms;
    uint16_t value;
} sensor_state_t;

#define SCREEN_SENSOR_STATE 0xF0

typedef struct __attribute__((aligned(PORT_NATURAL_ALIGN))) {
    uint8_t op;
    sensor_state_t state;
} screen_msg_t;

extern const uint8_t sensor_addr[N_SENSORS];
extern mailbox_t *sensor_mb[N_SENSORS];
extern mutex_t i2c_mtx;

bool checkSensor(byte address);
void startSensor();

uint8_t check_screen();
bool post_screen_command(uint8_t op);
void post_sensor_state(const sensor_state_t *state);
void startScreen();

void startReader();

void send2Writer(uint8_t head, const uint8_t *body, uint8_t len);
void send_ack(uint8_t sensor, uint8_t operation);
void send_error(uint8_t sensor, uint8_t operation, uint8_t code);
void send_measure(uint8_t sensor, uint8_t operation, uint8_t unit, uint16_t value);
void startWriter();

#endif
