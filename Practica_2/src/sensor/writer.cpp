#include <string.h>
#include "messages.h"

#define WRITER_THD_PRIO (NORMALPRIO + 2)

response_t responsePool[N_SLOTS];

MEMORYPOOL_DECL(responseMemoryPool, sizeof(response_t), PORT_NATURAL_ALIGN, NULL);

msg_t slots_W[N_SLOTS];
MAILBOX_DECL(mb_writer, &slots_W, N_SLOTS);

static THD_WORKING_AREA(waWriter, 1024);

void send2Writer(uint8_t head, const uint8_t *body, uint8_t len)
{
    if (len > PROTO_MAX_BODY) return;

    response_t *p = (response_t *)chPoolAlloc(&responseMemoryPool);
    if (!p) {
        Serial.println("writer: chPoolAlloc failed");
        return;
    }

    p->head = head;
    p->len = len;
    if (len > 0) memcpy(p->body, body, len);

    if (chMBPostTimeout(&mb_writer, (msg_t)p, TIME_IMMEDIATE) != MSG_OK) {
        Serial.println("writer: chMBPost failed");
        chPoolFree(&responseMemoryPool, p);
    }
}

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

        if (chMBFetchTimeout(&mb_writer, (msg_t *)&p, TIME_INFINITE) == MSG_OK) {
            Serial1.write(p->head);
            if (p->len > 0) Serial1.write(p->body, p->len);

            chPoolFree(&responseMemoryPool, p);
        }
    }
}

void startWriter()
{
    chPoolLoadArray(&responseMemoryPool, responsePool, N_SLOTS);

    chThdCreateStatic(waWriter, sizeof(waWriter), WRITER_THD_PRIO, writer, NULL);
}
