//------------------------------------------------------------------------------
// reader.cpp - Lee los comandos que llegan del supervisor por Serial1 y los
// deja en el mailbox del sensor que toca (sensor_mb, declarado en sensor.cpp).
// Sigue el esquema del ejemplo 2 de ChibiOS (worker -> mailbox -> printer)
//
// Formato de cada mensaje (protocol.h): código(1) | longitud(1) | cuerpo
//------------------------------------------------------------------------------
#include "messages.h"

#define READER_THD_PRIO  (NORMALPRIO + 2)
#define FRAME_TIMEOUT_MS 100   // espera máxima entre bytes de un mismo mensaje
#define READER_POLL_MS   1     // cada cuánto se mira si han llegado bytes
#define WAIT_FOREVER     0

static THD_WORKING_AREA(waReader, 1024);

// Espera un byte de Serial1 como mucho timeout_ms (WAIT_FOREVER: sin límite).
// Devuelve false si no llega a tiempo
static bool read_byte(uint8_t *b, uint32_t timeout_ms)
{
    systime_t start_i = chVTGetSystemTimeX();
    while (Serial1.available() == 0) {
        if (timeout_ms != WAIT_FOREVER &&
            chVTTimeElapsedSinceX(start_i) > TIME_MS2I(timeout_ms))
            return false;
        chThdSleepMilliseconds(READER_POLL_MS);   // cede la CPU mientras espera
    }
    *b = (uint8_t)Serial1.read();
    return true;
}

// Longitud del cuerpo que debe tener cada petición de sensor (ver protocol.h)
static int expected_len(uint8_t op)
{
    switch (op) {
        case OP_US_ONESHOT:      return 1;   // sensor
        case OP_US_PERIODIC_ON:  return 3;   // sensor, periodo_ms(2)
        case OP_US_PERIODIC_OFF: return 1;   // sensor
        case OP_US_SET_UNIT:     return 2;   // sensor, unidad
        case OP_US_SET_DELAY:    return 3;   // sensor, retardo_ms(2)
        case OP_US_STATUS:       return 1;   // sensor
        default:                 return -1;  // no es una petición de sensor
    }
}

// true si op es uno de los códigos de petición de protocol.h
static bool is_request(uint8_t op)
{
    return (op >= OP_US_LIST && op <= OP_US_STATUS) ||
           (op >= OP_OLED_TEXT && op <= OP_OLED_MODE);
}

// "us": la lista no depende de ningún sensor, así que la contesta el reader
static void send_list()
{
    uint8_t body[1 + 2 * N_SENSORS];
    body[0] = N_SENSORS;
    for (int i = 0; i < N_SENSORS; i++) {
        body[1 + 2 * i] = i + 1;            // número de sensor
        body[2 + 2 * i] = sensor_addr[i];   // dirección I2C
    }
    post_to_writer(OP_RSP_LIST, body, sizeof(body));
}

// Decide qué hacer con un mensaje completo
static void dispatch(uint8_t op, const uint8_t *body, uint8_t len)
{
    if (op == OP_US_LIST) {
        send_list();
        return;
    }

    if (expected_len(op) < 0) {
        send_error(op, PROTO_ERR_UNKNOWN_OP);
        return;
    }
    if (len != expected_len(op)) {
        send_error(op, PROTO_ERR_MALFORMED);
        return;
    }

    uint8_t sensor = body[0];   // 1, 2, ... (no la dirección I2C)
    if (sensor < 1 || sensor > N_SENSORS) {
        send_error(op, PROTO_ERR_NO_SENSOR);
        return;
    }

    // Pedimos un objeto al memory pool de comandos
    commands_t *p = (commands_t *)chPoolAlloc(&commandMemoryPool);
    if (!p) {
        Serial.println("reader: chPoolAlloc failed");
        send_error(op, PROTO_ERR_BAD_PARAM);   // no hay código de "ocupado"
        return;
    }

    // Formamos el mensaje
    p->op = op;
    p->sensor = sensor;
    p->arg = 0;
    if (op == OP_US_PERIODIC_ON || op == OP_US_SET_DELAY) p->arg = protoGet16(&body[1]);
    if (op == OP_US_SET_UNIT)                             p->arg = body[1];

    // Lo enviamos al mailbox del sensor: el 1 usa sensor_mb[0], el 2 sensor_mb[1]
    msg_t s = chMBPostTimeout(sensor_mb[sensor - 1], (msg_t)p, TIME_IMMEDIATE);
    if (s != MSG_OK) {
        Serial.println("reader: chMBPost failed");
        chPoolFree(&commandMemoryPool, p);   // mailbox lleno: devolvemos el hueco
        send_error(op, PROTO_ERR_BAD_PARAM);
    }
}

static THD_FUNCTION(reader, arg)
{
    (void)arg;
    uint8_t op, len;
    uint8_t body[PROTO_MAX_BODY];

    while (true) {
        // 1. Código: se espera sin límite a que el supervisor mande algo
        read_byte(&op, WAIT_FOREVER);

        // Un byte que no es código de petición (ruido al conectar el cable,
        // restos de un mensaje cortado) se ignora: así se recupera la sincronía
        if (!is_request(op)) continue;

        // 2. Longitud
        if (!read_byte(&len, FRAME_TIMEOUT_MS)) continue;   // mensaje cortado
        if (len > PROTO_MAX_BODY) {
            send_error(op, PROTO_ERR_MALFORMED);
            continue;
        }

        // 3. Cuerpo
        bool complete = true;
        for (int i = 0; i < len && complete; i++)
            complete = read_byte(&body[i], FRAME_TIMEOUT_MS);
        if (!complete) continue;   // mensaje cortado: se descarta (protocol.h)

        dispatch(op, body, len);
    }
}

void startReader()
{
    chThdCreateStatic(waReader, sizeof(waReader), READER_THD_PRIO, reader, NULL);
}
