//------------------------------------------------------------------------------
// sensor.cpp - Una hebra por SRF02. Cada hebra espera comandos en su mailbox
// (los deja el reader), los ejecuta por I2C y manda la respuesta al writer
//------------------------------------------------------------------------------
#include <Arduino.h>
#include <Wire.h> // Arduino's I2C library
#include "messages.h"

#define SENSOR_THD_PRIO        (NORMALPRIO + 1)
#define DEFAULT_SENSOR_PERIOD  100   // ms

// Registros del SRF02
#define COMMAND_REGISTER   byte(0x00)   // al escribir: comando
#define SOFTWARE_REVISION  byte(0x00)   // al leer: versión de firmware
#define RANGE_HIGH_BYTE    byte(2)
#define RANGE_LOW_BYTE     byte(3)

// Comandos de medida del SRF02
#define REAL_RANGING_MODE_INCHES  byte(80)
#define REAL_RANGING_MODE_CMS     byte(81)
#define REAL_RANGING_MODE_USECS   byte(82)
#define SRF02_RANGING_DELAY       70   // ms que tarda una medida


/* ------------------------- Mailboxes de los sensores ---------------------- */
/* ------------------------- pio run -e sensor -t upload --upload-port /dev/ttyACM1 ---------------------- */
// Mismos pasos que en el ejemplo 2 de ChibiOS: array de objetos, memory pool
// sobre ese array y un mailbox por sensor. El mailbox solo guarda punteros
// (msg_t) a objetos del pool, por eso tiene tantos huecos como el pool.
commands_t commandPool[N_SLOTS];
MEMORYPOOL_DECL(commandMemoryPool, sizeof(commands_t), PORT_NATURAL_ALIGN, NULL);

msg_t slots_S1[N_SLOTS], slots_S2[N_SLOTS];
MAILBOX_DECL(mb_s1, &slots_S1, N_SLOTS);
MAILBOX_DECL(mb_s2, &slots_S2, N_SLOTS);

mailbox_t *sensor_mb[N_SENSORS] = { &mb_s1, &mb_s2 };

// Las dos hebras comparten el bus I2C
MUTEX_DECL(i2c_mtx);

static THD_WORKING_AREA(waSensor1, 2048);
static THD_WORKING_AREA(waSensor2, 2048);


/* ------------------------- Configuración de cada sensor ------------------- */
// Posición 0: sensor 1, posición 1: sensor 2
const uint8_t sensor_addr[N_SENSORS]  = { 0xE0 >> 1, 0xE2 >> 1 };   // I2C, 7 bits
uint8_t   sensor_unit[N_SENSORS]      = { PROTO_UNIT_CM, PROTO_UNIT_CM };
uint16_t  sensor_delay_ms[N_SENSORS]  = { 0, 0 };   // retardo mínimo entre disparos
systime_t last_shot_i[N_SENSORS]      = { 0, 0 };


/* ------------------------- Acceso al SRF02 por I2C ------------------------ */
// Devuelve false si el sensor no contesta
bool write_command(byte address, byte command)
{
    Wire.beginTransmission(address);
    Wire.write(COMMAND_REGISTER);
    Wire.write(command);
    return Wire.endTransmission() == 0;   // 0: el sensor reconoció la dirección
}

// Devuelve false si el sensor no contesta
bool read_register(byte address, byte the_register, byte *value)
{
    Wire.beginTransmission(address);
    Wire.write(the_register);
    if (Wire.endTransmission() != 0) return false;

    if (Wire.requestFrom(address, byte(1)) != 1) return false;
    *value = Wire.read();
    return true;
}

// true si hay un SRF02 contestando en esa dirección
bool check_sensor(byte address)
{
    byte revision = 0;
    chMtxLock(&i2c_mtx);
    bool answers = read_register(address, SOFTWARE_REVISION, &revision);
    chMtxUnlock(&i2c_mtx);
    return answers && revision != 0x00 && revision != 0xFF;
}

// Comando de medida del SRF02 según la unidad del protocolo
byte unit_cmd(uint8_t unit)
{
    if (unit == PROTO_UNIT_INC) return REAL_RANGING_MODE_INCHES;
    if (unit == PROTO_UNIT_MS)  return REAL_RANGING_MODE_USECS;
    return REAL_RANGING_MODE_CMS;
}

// Mide y manda la medida al writer. Devuelve false si falla el I2C
// (quien la llama decide qué error enviar al supervisor)
bool measure_and_report(int id, uint8_t address)
{
    // Respetamos el retardo mínimo entre dos disparos de este sensor
    systime_t next_shot_i = last_shot_i[id - 1] + TIME_MS2I(sensor_delay_ms[id - 1]);
    if (next_shot_i > chVTGetSystemTimeX()) chThdSleepUntil(next_shot_i);
    last_shot_i[id - 1] = chVTGetSystemTimeX();

    chMtxLock(&i2c_mtx);
    bool ok = write_command(address, unit_cmd(sensor_unit[id - 1]));
    chMtxUnlock(&i2c_mtx);
    if (!ok) return false;

    // Se duerme sin el mutex: mientras, la otra hebra puede usar el bus
    chThdSleepMilliseconds(SRF02_RANGING_DELAY);

    byte hi = 0, lo = 0;
    chMtxLock(&i2c_mtx);
    ok = read_register(address, RANGE_HIGH_BYTE, &hi) &&
         read_register(address, RANGE_LOW_BYTE, &lo);
    chMtxUnlock(&i2c_mtx);
    if (!ok) return false;

    send_measure(id, sensor_unit[id - 1], (hi << 8) | lo);
    return true;
}


/* ------------------------- Comandos del supervisor ------------------------ */
// Ejecuta un comando y deja la respuesta en el writer. El one-shot solo se
// valida aquí: la medida la hace la hebra justo después
void apply_command(commands_t *command, bool *periodic, sysinterval_t *period, int id)
{
    switch (command->op) {
        case OP_US_ONESHOT:
            break;

        case OP_US_PERIODIC_ON:
            if (command->arg == 0 || command->arg < sensor_delay_ms[id - 1]) {
                send_error(command->op, PROTO_ERR_BAD_PARAM);
                break;
            }
            *periodic = true;
            *period = TIME_MS2I(command->arg);
            send_ack(command->op);
            break;

        case OP_US_PERIODIC_OFF:
            *periodic = false;
            send_ack(command->op);
            break;

        case OP_US_SET_UNIT:
            if (command->arg > PROTO_UNIT_MS) {
                send_error(command->op, PROTO_ERR_BAD_PARAM);
                break;
            }
            sensor_unit[id - 1] = command->arg;
            send_ack(command->op);
            break;

        case OP_US_SET_DELAY:
            sensor_delay_ms[id - 1] = command->arg;
            send_ack(command->op);
            break;

        case OP_US_STATUS: {
            // sensor, dirección, unidad, retardo_ms(2), periódico, periodo_ms(2)
            uint8_t body[8];
            body[0] = id;
            body[1] = sensor_addr[id - 1];
            body[2] = sensor_unit[id - 1];
            protoPut16(&body[3], sensor_delay_ms[id - 1]);
            body[5] = *periodic ? 1 : 0;
            protoPut16(&body[6], TIME_I2MS(*period));
            post_to_writer(OP_RSP_STATUS, body, sizeof(body));
            break;
        }

        default:
            send_error(command->op, PROTO_ERR_UNKNOWN_OP);
            break;
    }
}


/* ------------------------- Hebra de cada sensor --------------------------- */
static THD_FUNCTION(sensor, arg)
{
    int id = (int)arg;                        // 1 o 2, como en el protocolo
    uint8_t address = sensor_addr[id - 1];
    bool periodic = false;
    sysinterval_t period_i = TIME_MS2I(DEFAULT_SENSOR_PERIOD);
    systime_t deadline_i = chVTGetSystemTimeX();

    while (true) {
        // El mailbox hace de despertador y de temporizador: se espera un
        // comando como mucho hasta el próximo disparo periódico (o sin
        // límite si no hay disparo periódico)
        sysinterval_t wait = TIME_INFINITE;
        if (periodic) {
            systime_t now = chVTGetSystemTimeX();
            wait = (deadline_i > now) ? (sysinterval_t)(deadline_i - now) : TIME_IMMEDIATE;
        }

        commands_t *command = nullptr;
        msg_t result = chMBFetchTimeout(sensor_mb[id - 1], (msg_t *)&command, wait);

        if (result == MSG_OK) {
            // Ha llegado un comando
            apply_command(command, &periodic, &period_i, id);
            bool shoot = (command->op == OP_US_ONESHOT);
            if (command->op == OP_US_PERIODIC_ON) deadline_i = chVTGetSystemTimeX();  // primer disparo ya
            chPoolFree(&commandMemoryPool, command);   // ¡siempre liberar!

            if (shoot && !measure_and_report(id, address))
                send_error(OP_US_ONESHOT, PROTO_ERR_I2C);
        }
        else {
            // MSG_TIMEOUT: toca el disparo periódico
            if (!measure_and_report(id, address))
                send_error(OP_US_PERIODIC_ON, PROTO_ERR_I2C);
            deadline_i += period_i;
        }
    }
}

void startSensor()
{
    // Vinculamos el memory pool con el array de comandos
    chPoolLoadArray(&commandMemoryPool, commandPool, N_SLOTS);

    chThdCreateStatic(waSensor1, sizeof(waSensor1), SENSOR_THD_PRIO, sensor, (void *)1);
    chThdCreateStatic(waSensor2, sizeof(waSensor2), SENSOR_THD_PRIO, sensor, (void *)2);
}
