#include "messages.h"

#define READER_THD_PRIO  (NORMALPRIO + 2)
#define FRAME_TIMEOUT_MS 100
#define READER_POLL_MS   10
#define WAIT_FOREVER     0

static THD_WORKING_AREA(waReader, 1024);

static bool read_byte(uint8_t *bytes, uint32_t timeout_ms)
{
    systime_t start_i = chVTGetSystemTimeX();
    while (Serial1.available() == 0) {
        if (timeout_ms != WAIT_FOREVER &&
            chVTTimeElapsedSinceX(start_i) > TIME_MS2I(timeout_ms))
            return false;
        chThdSleepMilliseconds(READER_POLL_MS);
    }
    *bytes = (uint8_t)Serial1.read();
    return true;
}

static bool isUsOperation(uint8_t op)
{
    switch (op) {
        case US_ONESHOT:
        case OP_US_PERIODIC_ON:
        case OP_US_PERIODIC_OFF:
        case OP_US_SET_DELAY:
        case OP_STATUS:
        case PROTO_UNIT_INC:
        case PROTO_UNIT_CM:
        case PROTO_UNIT_MS:
            return true;
        default:
            return false;
    }
}

static bool isOledOperation(uint8_t op)
{
    return op == OP_OLED_ON || op == OP_OLED_OFF;
}

static bool needsBody(uint8_t op)
{
    return op == OP_US_PERIODIC_ON || op == OP_US_SET_DELAY;
}

static bool split_request(uint8_t head, uint8_t *sensor, bool *message, uint8_t *operation)
{
    if (head & RESP) return false;

    *sensor    = (head & SENSORES) >> SENSOR_POSITION;
    *message   = (head & MESSAGE) != 0;
    *operation = head & 0x0F;

    if (*message != needsBody(*operation)) return false;
    if (*operation == US_LIST) return *sensor == 3;
    if (*sensor == 0)          return isOledOperation(*operation);
    return isUsOperation(*operation);
}

static void send_list()
{
    uint8_t body[1 + 2 * N_SENSORS];
    body[0] = N_SENSORS;
    for (int i = 0; i < N_SENSORS; i++) {
        body[1 + 2 * i] = i + 1;
        body[2 + 2 * i] = sensor_addr[i];
    }
    send2Writer(generateHead(RESP, SENSORES, MESSAGE, US_LIST), body, sizeof(body));
}

static bool post(mailbox_t * mb, uint8_t operation, uint16_t value)
{
    return chMBPostTimeout(mb, packCommand(operation, value), TIME_IMMEDIATE) == MSG_OK;
}

static void dispatch(uint8_t sensor, uint8_t operation, uint16_t value)
{
    if (operation == US_LIST) { send_list(); return; }

    bool ok;
    switch (sensor) {
        case 0:  ok = post_screen_command(operation);        break;
        case 1:
        case 2:  ok = post(sensor_mb[sensor - 1], operation, value);  break;
        default: ok  = post(sensor_mb[0], operation, value);
                 ok &= post(sensor_mb[1], operation, value);          break;
    }
    if (!ok) send_error(sensor, operation, PROTO_ERROR_BUSY);
}

static THD_FUNCTION(reader, arg)
{
    (void)arg;
    uint8_t head, sensor, operation;
    bool message;

    while (true) {
        read_byte(&head, WAIT_FOREVER);

        if (!split_request(head, &sensor, &message, &operation)) continue;

        uint16_t value = 0;
        if (message) {
            uint8_t body[2];
            if (!read_byte(&body[0], FRAME_TIMEOUT_MS)) continue;
            if (!read_byte(&body[1], FRAME_TIMEOUT_MS)) continue;
            value = PROTO_GET16(body);
        }

        dispatch(sensor, operation, value);
    }
}

void startReader()
{
    chThdCreateStatic(waReader, sizeof(waReader), READER_THD_PRIO, reader, NULL);
}
