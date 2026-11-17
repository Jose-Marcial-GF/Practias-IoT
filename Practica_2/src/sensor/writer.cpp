//------------------------------------------------------------------------------
// writer.cpp - Saca las respuestas de su mailbox y las envía por Serial1 al
// supervisor. Es la única hebra que escribe en Serial1, así los mensajes de
// las distintas hebras nunca se mezclan.
// Sigue el esquema del ejemplo 2 de ChibiOS (worker -> mailbox -> printer)
//
// Respuestas (cabecera = RESP/ERROR | dispositivo | MESSAGE | operación):
//   ACK      RESP  | disp | NO_MESSAGE | op                    (sin cuerpo)
//   ERROR    ERROR | disp | MESSAGE    | op   + código(1)
//   MEDIDA   RESP  | disp | MESSAGE    | US_ONESHOT o OP_US_PERIODIC_ON
//                                           + unidad(1) valor(2)
//   STATUS   RESP  | disp | MESSAGE    | OP_STATUS  + 7 bytes (sensor.cpp)
//   LISTA    RESP  | SENSORES | MESSAGE | US_LIST   + n(1) (num, dir) * n
// Los valores de 16 bits van con el byte bajo primero (PROTO_PUT16)
//------------------------------------------------------------------------------
#include <string.h>
#include "messages.h"

#define WRITER_THD_PRIO (NORMALPRIO + 2)

// Array de objetos de tipo response_t
response_t responsePool[N_SLOTS];

// Banco de memoria (memory pool) sobre ese array
MEMORYPOOL_DECL(responseMemoryPool, sizeof(response_t), PORT_NATURAL_ALIGN, NULL);

// Slots del mailbox: guardan punteros a objetos del memory pool
msg_t slots_W[N_SLOTS];
MAILBOX_DECL(mb_writer, &slots_W, N_SLOTS);

static THD_WORKING_AREA(waWriter, 1024);

// Deja una respuesta en el mailbox del writer. La puede llamar cualquier hebra
void send2Writer(uint8_t head, const uint8_t *body, uint8_t len)
{
    if (len > PROTO_MAX_BODY) return;

    // Pedimos un objeto al memory pool
    response_t *p = (response_t *)chPoolAlloc(&responseMemoryPool);
    if (!p) {
        Serial.println("writer: chPoolAlloc failed");
        return;
    }

    // Formamos el mensaje
    p->head = head;
    p->len = len;
    if (len > 0) memcpy(p->body, body, len);

    // Lo enviamos sin bloquear (TIME_IMMEDIATE)
    if (chMBPostTimeout(&mb_writer, (msg_t)p, TIME_IMMEDIATE) != MSG_OK) {
        Serial.println("writer: chMBPost failed");
        chPoolFree(&responseMemoryPool, p);   // mailbox lleno: devolvemos el hueco
    }
}


// Atajos para las respuestas más comunes. sensor: 0 pantalla, 1, 2 o 3 (ambos)
void send_ack(uint8_t sensor, uint8_t operation)
{
    send2Writer(generateHead(RESP, sensor << SENSOR_POSITION, NO_MESSAGE, operation),
                NULL, 0);
}

void send_error(uint8_t sensor, uint8_t operation, uint8_t code)
{
    send2Writer(generateHead(ERROR, sensor << SENSOR_POSITION, MESSAGE, operation),
                &code, 1);
}

void send_measure(uint8_t sensor, uint8_t operation, uint8_t unit, uint16_t value)
{
    uint8_t body[3];
    body[0] = unit;
    PROTO_PUT16(&body[1], value);
    send2Writer(generateHead(RESP, sensor << SENSOR_POSITION, MESSAGE, operation),
                body, sizeof(body));
}

static THD_FUNCTION(writer, arg)
{
    (void)arg;

    while (true) {
        response_t *p = 0;

        // Duerme hasta que alguien deje una respuesta en el mailbox
        if (chMBFetchTimeout(&mb_writer, (msg_t *)&p, TIME_INFINITE) == MSG_OK) {
            Serial1.write(p->head);
            if (p->len > 0) Serial1.write(p->body, p->len);

            // Devolvemos la memoria al pool (¡siempre!)
            chPoolFree(&responseMemoryPool, p);
        }
    }
}

void startWriter()
{
    // Vinculamos el memory pool con el array de objetos
    chPoolLoadArray(&responseMemoryPool, responsePool, N_SLOTS);

    chThdCreateStatic(waWriter, sizeof(waWriter), WRITER_THD_PRIO, writer, NULL);
}
