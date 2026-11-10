//------------------------------------------------------------------------------
// writer.cpp - Saca las respuestas de su mailbox y las envía por Serial1 al
// supervisor. Es la única hebra que escribe en Serial1, así los mensajes de
// las distintas hebras nunca se mezclan.
// Sigue el esquema del ejemplo 2 de ChibiOS (worker -> mailbox -> printer)
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
void post_to_writer(uint8_t op, const uint8_t *body, uint8_t len)
{
    // Pedimos un objeto al memory pool
    response_t *p = (response_t *)chPoolAlloc(&responseMemoryPool);
    if (!p) {
        Serial.println("writer: chPoolAlloc failed");
        return;
    }

    // Formamos el mensaje
    p->op = op;
    p->len = len;
    memcpy(p->body, body, len);

    // Lo enviamos sin bloquear (TIME_IMMEDIATE)
    msg_t s = chMBPostTimeout(&mb_writer, (msg_t)p, TIME_IMMEDIATE);
    if (s != MSG_OK) {
        Serial.println("writer: chMBPost failed");
        chPoolFree(&responseMemoryPool, p);   // mailbox lleno: devolvemos el hueco
    }
}

// Atajos para las respuestas más comunes (formato en protocol.h)
void send_ack(uint8_t op)
{
    uint8_t body[1] = { op };
    post_to_writer(OP_RSP_ACK, body, 1);
}

void send_error(uint8_t op, uint8_t error)
{
    uint8_t body[2] = { op, error };
    post_to_writer(OP_RSP_ERROR, body, 2);
}

void send_measure(uint8_t sensor, uint8_t unit, uint16_t value)
{
    uint8_t body[4] = { sensor, unit, 0, 0 };
    protoPut16(&body[2], value);
    post_to_writer(OP_RSP_MEASURE, body, 4);
}

static THD_FUNCTION(writer, arg)
{
    (void)arg;

    while (true) {
        response_t *p = 0;

        // Duerme hasta que alguien deje una respuesta en el mailbox
        msg_t res = chMBFetchTimeout(&mb_writer, (msg_t *)&p, TIME_INFINITE);
        if (res == MSG_OK) {
            Serial1.write(p->op);
            Serial1.write(p->len);
            Serial1.write(p->body, p->len);

            // Devolvemos la memoria al pool (¡siempre!)
            chPoolFree(&responseMemoryPool, p);
        }
        else {
            Serial.println("writer: chMBFetchTimeout() failed");
        }
    }
}

void startWriter()
{
    // Vinculamos el memory pool con el array de objetos
    chPoolLoadArray(&responseMemoryPool, responsePool, N_SLOTS);

    chThdCreateStatic(waWriter, sizeof(waWriter), WRITER_THD_PRIO, writer, NULL);
}
