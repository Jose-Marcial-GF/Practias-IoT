#include <Arduino.h>
#include <Wire.h>
#include "messages.h"

#define SENSOR_THD_PRIO        (NORMALPRIO + 1)
#define DEFAULT_SENSOR_PERIOD  100

#define COMMAND_REGISTER   byte(0x00)
#define SOFTWARE_REVISION  byte(0x00)
#define RANGE_HIGH_BYTE    byte(2)
#define RANGE_LOW_BYTE     byte(3)

#define REAL_RANGING_MODE_INCHES  byte(80)
#define REAL_RANGING_MODE_CMS     byte(81)
#define REAL_RANGING_MODE_USECS   byte(82)
#define SRF02_RANGING_DELAY       70

msg_t slots_S1[N_SLOTS], slots_S2[N_SLOTS];
MAILBOX_DECL(mb_s1, &slots_S1, N_SLOTS);
MAILBOX_DECL(mb_s2, &slots_S2, N_SLOTS);

mailbox_t *sensor_mb[N_SENSORS] = { &mb_s1, &mb_s2 };

MUTEX_DECL(i2c_mtx);

static THD_WORKING_AREA(waSensor1, 2048);
static THD_WORKING_AREA(waSensor2, 2048);

const uint8_t sensor_addr[N_SENSORS]  = { 0xF2 >> 1, 0xE2 >> 1 };
uint8_t   sensor_unit[N_SENSORS]      = { PROTO_UNIT_CM, PROTO_UNIT_CM };
uint16_t  sensor_delay_ms[N_SENSORS]  = { MIN_DELAY_SFR, MIN_DELAY_SFR };
systime_t last_shot_i[N_SENSORS]      = { 0, 0 };

bool _writeCommand(byte address, byte command)
{
    Wire.beginTransmission(address);
    Wire.write(COMMAND_REGISTER);
    Wire.write(command);
    return Wire.endTransmission() == 0;
}

static bool _readRegister(byte address, byte the_register, byte *value)
{
    Wire.beginTransmission(address);
    Wire.write(the_register);
    if (Wire.endTransmission() != 0) return false;

    if (Wire.requestFrom(address, byte(1)) != 1) return false;
    *value = Wire.read();
    return true;
}

bool readRegister(byte address, byte the_register, byte *value)
{
    chMtxLock(&i2c_mtx);
    bool ok = _readRegister(address, the_register, value);
    chMtxUnlock(&i2c_mtx);
    return ok;
}

static bool _readMeasure(byte address, uint16_t *value)
{
    Wire.beginTransmission(address);
    Wire.write(RANGE_HIGH_BYTE);
    if (Wire.endTransmission() != 0) return false;

    if (Wire.requestFrom(address, byte(2)) != 2) return false;
    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();
    *value = (hi << 8) | lo;
    return true;
}

bool readMeasure(byte address, uint16_t *value)
{
    chMtxLock(&i2c_mtx);
    bool ok = _readMeasure(address, value);
    chMtxUnlock(&i2c_mtx);
    return ok;
}
bool writeCommand(byte address, byte command)
{
    chMtxLock(&i2c_mtx);
    bool ok = _writeCommand(address, command);
    chMtxUnlock(&i2c_mtx);
    return ok;
}

bool checkSensor(byte address)
{
    byte revision = 0;
    bool answers = readRegister(address, SOFTWARE_REVISION, &revision);
    return answers && revision != 0x00 && revision != 0xFF;
}

byte unitCmd(uint8_t unit)
{
    if (unit == PROTO_UNIT_INC) return REAL_RANGING_MODE_INCHES;
    if (unit == PROTO_UNIT_MS)  return REAL_RANGING_MODE_USECS;
    return REAL_RANGING_MODE_CMS;
}

bool measure(int id, uint8_t address, uint16_t *value)
{
    systime_t next_shot_i = last_shot_i[id - 1] + TIME_MS2I(sensor_delay_ms[id - 1]);
    if (next_shot_i > chVTGetSystemTimeX()) chThdSleepUntil(next_shot_i);
    last_shot_i[id - 1] = chVTGetSystemTimeX();

    if (!writeCommand(address, unitCmd(sensor_unit[id - 1]))) return false;

    chThdSleepMilliseconds(SRF02_RANGING_DELAY);

    return readMeasure(address, value);
}

static uint8_t i2c_error(int id)
{
    return id == 1 ? PROTO_ERR_I2C_SENSOR1 : PROTO_ERR_I2C_SENSOR2;
}

static void publish(int id, uint8_t op, uint8_t result, uint16_t value,
                    bool periodic, sysinterval_t period_i)
{
    if (result == MEASURE_OK)    send_measure(id, op, sensor_unit[id - 1], value);
    if (result == MEASURE_ERROR) send_error(id, op, i2c_error(id));

    sensor_state_t s;
    s.sensor    = id;
    s.event     = op;
    s.unit      = sensor_unit[id - 1];
    s.periodic  = periodic ? 1 : 0;
    s.result    = result;
    s.delay_ms  = sensor_delay_ms[id - 1];
    s.period_ms = TIME_I2MS(period_i);
    s.value     = value;
    post_sensor_state(&s);
}

bool apply_command(uint8_t op, uint16_t value, bool *periodic,
                   sysinterval_t *period, int id)
{
    switch (op) {
        case US_ONESHOT:
            break;

        case OP_US_PERIODIC_ON:
            if (value < sensor_delay_ms[id - 1]) value = sensor_delay_ms[id - 1];
            *periodic = true;
            *period = TIME_MS2I(value);
            send_ack(id, op);
            break;

        case OP_US_PERIODIC_OFF:
            *periodic = false;
            send_ack(id, op);
            break;

        case PROTO_UNIT_INC:
        case PROTO_UNIT_CM:
        case PROTO_UNIT_MS:
            sensor_unit[id - 1] = op;
            send_ack(id, op);
            break;

        case OP_US_SET_DELAY:
            sensor_delay_ms[id - 1] = value;
            send_ack(id, op);
            break;

        case OP_STATUS: {
            uint8_t body[7];
            body[0] = sensor_addr[id - 1];
            body[1] = sensor_unit[id - 1];
            PROTO_PUT16(&body[2], sensor_delay_ms[id - 1]);
            body[4] = *periodic ? 1 : 0;
            PROTO_PUT16(&body[5], TIME_I2MS(*period));
            send2Writer(generateHead(RESP, id << SENSOR_POSITION, MESSAGE, OP_STATUS),
                        body, sizeof(body));
            break;
        }

        default:
            return false;
    }
    return true;
}

static THD_FUNCTION(sensor, arg)
{
    int id = (int)arg;
    uint8_t address = sensor_addr[id - 1];
    bool periodic = false;
    sysinterval_t period_i = TIME_MS2I(DEFAULT_SENSOR_PERIOD);
    systime_t deadline_i = chVTGetSystemTimeX();

    publish(id, NO_SHOW_OPERATION, NO_MEASURE, 0, periodic, period_i);

    while (true) {
        sysinterval_t wait = TIME_INFINITE;
        if (periodic) {
            systime_t now = chVTGetSystemTimeX();
            wait = (deadline_i > now) ? (sysinterval_t)(deadline_i - now) : TIME_IMMEDIATE;
        }

        msg_t cmd;
        if (chMBFetchTimeout(sensor_mb[id - 1], &cmd, wait) == MSG_OK) {
            uint8_t op = cmdOperation(cmd);
            uint16_t value = cmdValue(cmd);
            if (!apply_command(op, value, &periodic, &period_i, id)) continue;
            if (op == OP_US_PERIODIC_ON) deadline_i = chVTGetSystemTimeX();
            if (op == US_ONESHOT) {
                uint16_t m = 0;
                bool ok = measure(id, address, &m);
                publish(id, op, ok ? MEASURE_OK : MEASURE_ERROR, m, periodic, period_i);
            } else {
                publish(id, op, NO_MEASURE, 0, periodic, period_i);
            }
        }
        else {
            uint16_t m = 0;
            bool ok = measure(id, address, &m);
            deadline_i = chTimeAddX(chVTGetSystemTimeX(), period_i);

            publish(id, OP_US_PERIODIC_ON, ok ? MEASURE_OK : MEASURE_ERROR, m, periodic, period_i);
        }

    }
}

void startSensor()
{
    chThdCreateStatic(waSensor1, sizeof(waSensor1), SENSOR_THD_PRIO, sensor, (void *)1);
    chThdCreateStatic(waSensor2, sizeof(waSensor2), SENSOR_THD_PRIO, sensor, (void *)2);
}
