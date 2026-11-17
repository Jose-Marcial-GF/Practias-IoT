//------------------------------------------------------------------------------
// screen.cpp - Hebra de la pantalla OLED (SSD1306 128x64 por I2C). Espera
// mensajes en su mailbox y los pinta. Le escriben dos tipos de hebra:
//   - reader: comandos oled del supervisor (text, clear, mode). La pantalla
//     contesta con ACK o ERROR a través del writer, como los sensores.
//   - sensor: el estado de su SRF02 cada vez que cambia o mide (modo auto).
// Mismo esquema que sensor.cpp y writer.cpp: memory pool + mailbox.
//
// La pantalla comparte el bus I2C con los SRF02: todo acceso a la pantalla se
// hace con i2c_mtx, igual que en sensor.cpp.
//------------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"
#include "messages.h"

#define SCREEN_THD_PRIO  (NORMALPRIO + 1)
#define SCREEN_ADDR_1    0x3C   // direcciones posibles del SSD1306 (7 bits)
#define SCREEN_ADDR_2    0x3D
#define SCREEN_ROWS      8      // 64 px / 8 px por fila (fuente System5x7)
#define SCREEN_COLS      21     // 128 px / 6 px por carácter

// Filas del modo auto
#define ROW_TITLE         0
#define ROW_SENSOR(id)    (1 + 2 * ((id) - 1))   // 2 filas por sensor: 1-2 y 3-4
#define ROW_ACTION_TITLE  6
#define ROW_ACTION        7

// Qué partes del modo auto hay que repintar (un bit por sensor y la acción)
#define DIRTY_SENSOR(id)  (1 << ((id) - 1))
#define DIRTY_ACTION      0x80


/* ------------------------- Mailbox de la pantalla ------------------------- */
// Mismos pasos que en sensor.cpp: array de objetos, memory pool sobre ese
// array y un mailbox con tantos huecos como el pool
screen_msg_t screenPool[N_SLOTS];
MEMORYPOOL_DECL(screenMemoryPool, sizeof(screen_msg_t), PORT_NATURAL_ALIGN, NULL);

msg_t slots_screen[N_SLOTS];
MAILBOX_DECL(mb_screen, &slots_screen, N_SLOTS);

static THD_WORKING_AREA(waScreen, 2048);   // snprintf usa bastante pila


/* ------------------------- Estado de la pantalla -------------------------- */
// Solo lo toca la hebra de la pantalla (screen_addr se fija antes de crearla)
static SSD1306AsciiWire oled;
static uint8_t screen_addr = 0;               // 0: no hay pantalla
static uint8_t mode = PROTO_OLED_AUTO;
static sensor_state_t state[N_SENSORS];       // último estado de cada sensor
static uint8_t last_sensor = 0;               // sensor de la última acción

// Última medida de cada sensor. Va aparte del estado porque no todos los
// mensajes traen medida (unit, delay, off...) y no debe borrarse. Guarda su
// propia unidad: si después se cambia la unidad, la medida sigue siendo la
// que se tomó (p. ej. 67in hasta la siguiente medida en cm)
typedef struct {
    uint8_t  result;     // SCREEN_NO_VALUE, SCREEN_VALUE_OK o SCREEN_VALUE_ERROR
    uint8_t  unit;       // unidad en la que se tomó
    uint16_t value;
} last_measure_t;
static last_measure_t measure[N_SENSORS];
static uint8_t dirty = 0;                     // DIRTY_*: pendiente de pintar


/* ------------------------- Envío de mensajes a la pantalla ---------------- */
// Las llama otras hebras. No bloquean: si el mailbox está lleno el mensaje se
// pierde (como en send2Writer)

// reader -> screen. Devuelve false si no hay hueco
bool post_screen_command(uint8_t op, const uint8_t *body, uint8_t len)
{
    // Pedimos un objeto al memory pool
    screen_msg_t *p = (screen_msg_t *)chPoolAlloc(&screenMemoryPool);
    if (!p) {
        Serial.println("screen: chPoolAlloc failed");
        return false;
    }

    // Formamos el mensaje
    p->op = op;
    p->len = len;
    memcpy(p->body, body, len);

    // Lo enviamos sin bloquear (TIME_IMMEDIATE)
    if (chMBPostTimeout(&mb_screen, (msg_t)p, TIME_IMMEDIATE) != MSG_OK) {
        Serial.println("screen: chMBPost failed");
        chPoolFree(&screenMemoryPool, p);   // mailbox lleno: devolvemos el hueco
        return false;
    }
    return true;
}

// sensor -> screen. Si no hay hueco no pasa nada: el siguiente estado que
// mande el sensor ya trae todo actualizado
void post_sensor_state(const sensor_state_t *s)
{
    screen_msg_t *p = (screen_msg_t *)chPoolAlloc(&screenMemoryPool);
    if (!p) return;

    p->op = SCREEN_SENSOR_STATE;
    p->len = 0;
    p->state = *s;

    if (chMBPostTimeout(&mb_screen, (msg_t)p, TIME_IMMEDIATE) != MSG_OK)
        chPoolFree(&screenMemoryPool, p);
}


/* ------------------------- Acceso a la pantalla por I2C ------------------- */
// true si hay un dispositivo contestando en esa dirección
static bool probe(uint8_t address)
{
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

// Busca la pantalla en sus dos direcciones posibles. Devuelve la que contesta
// o 0 si no hay pantalla
uint8_t check_screen()
{
    chMtxLock(&i2c_mtx);
    if (probe(SCREEN_ADDR_1))      screen_addr = SCREEN_ADDR_1;
    else if (probe(SCREEN_ADDR_2)) screen_addr = SCREEN_ADDR_2;
    else                           screen_addr = 0;
    chMtxUnlock(&i2c_mtx);
    return screen_addr;
}

// Escribe una fila entera (lo que sobre del texto se borra). El mutex se coge
// fila a fila, así los sensores pueden usar el bus entre una fila y otra
static void draw_row(uint8_t row, const char *text)
{
    chMtxLock(&i2c_mtx);
    oled.setCursor(0, row);
    oled.print(text);
    oled.clearToEOL();
    chMtxUnlock(&i2c_mtx);
}

static void clear_screen()
{
    for (uint8_t row = 0; row < SCREEN_ROWS; row++) draw_row(row, "");
}


/* ------------------------- Modo auto: estado de los sensores -------------- */
static const char *unit_name(uint8_t unit)
{
    if (unit == PROTO_UNIT_INC) return "in";
    if (unit == PROTO_UNIT_MS)  return "us";
    return "cm";
}

// Dos filas por sensor:   S1 0xF2 cm d:300ms
//                          171cm    T:500ms
static void draw_sensor(int id)
{
    const sensor_state_t *s = &state[id - 1];
    const last_measure_t *m = &measure[id - 1];
    char line[SCREEN_COLS + 1];
    char value[12];

    // Dirección de 8 bits, como en la documentación del SRF02
    snprintf(line, sizeof(line), "S%d 0x%02X %s d:%ums",
             id, sensor_addr[id - 1] << 1, unit_name(s->unit), s->delay_ms);
    draw_row(ROW_SENSOR(id), line);

    if (m->result == MEASURE_OK)
        snprintf(value, sizeof(value), "%u%s", m->value, unit_name(m->unit));
    else if (m->result == MEASURE_ERROR)
        snprintf(value, sizeof(value), "err I2C");
    else
        snprintf(value, sizeof(value), "---");

    if (s->periodic) snprintf(line, sizeof(line), " %-8s T:%ums", value, s->period_ms);
    else             snprintf(line, sizeof(line), " %-8s T:off", value);
    draw_row(ROW_SENSOR(id) + 1, line);
}

// Última acción del dispositivo sensor
static void draw_action()
{
    char line[SCREEN_COLS + 1] = "";

    if (last_sensor != 0) {
        const sensor_state_t *s = &state[last_sensor - 1];
        int n = last_sensor;
        switch (s->event) {
            case OP_US_ONESHOT:      snprintf(line, sizeof(line), "S%d one-shot", n); break;
            case OP_US_PERIODIC_ON:
                if (s->result == NO_MEASURE)
                    snprintf(line, sizeof(line), "S%d on cada %ums", n, s->period_ms);
                else
                    snprintf(line, sizeof(line), "S%d medida periodica", n);
                break;
            case OP_US_PERIODIC_OFF: snprintf(line, sizeof(line), "S%d periodico off", n); break;
            case OP_US_SET_UNIT:     snprintf(line, sizeof(line), "S%d unidad %s", n, unit_name(s->unit)); break;
            case OP_US_SET_DELAY:    snprintf(line, sizeof(line), "S%d retardo %ums", n, s->delay_ms); break;
            case OP_US_STATUS:       snprintf(line, sizeof(line), "S%d status", n); break;
        }
    }
    draw_row(ROW_ACTION, line);
}

// Pinta la pantalla entera del modo auto
static void draw_all()
{
    draw_row(ROW_TITLE, "DISPOSITIVO SENSOR");
    for (int id = 1; id <= N_SENSORS; id++) draw_sensor(id);
    draw_row(ROW_SENSOR(N_SENSORS) + 2, "");
    draw_row(ROW_ACTION_TITLE, "Ultima accion:");
    draw_action();
    dirty = 0;
}

// Pinta solo lo que ha cambiado desde la última vez
static void draw_dirty()
{
    if (screen_addr != 0 && mode == PROTO_OLED_AUTO) {
        for (int id = 1; id <= N_SENSORS; id++)
            if (dirty & DIRTY_SENSOR(id)) draw_sensor(id);
        if (dirty & DIRTY_ACTION) draw_action();
    }
    dirty = 0;
}

// sensor -> screen: se guarda el estado y se marca para repintar
static void update_sensor(const sensor_state_t *s)
{
    if (s->sensor < 1 || s->sensor > N_SENSORS) return;
    state[s->sensor - 1] = *s;
    if (s->result != NO_MEASURE) {   // solo si el mensaje trae medida
        last_measure_t *m = &measure[s->sensor - 1];
        m->result = s->result;
        m->unit   = s->unit;
        m->value  = s->value;
    }
    dirty |= DIRTY_SENSOR(s->sensor);
    if (s->event != 0) {               // 0: estado inicial, no es una acción
        last_sensor = s->sensor;
        dirty |= DIRTY_ACTION;
    }
}


/* ------------------------- Comandos del supervisor ------------------------ */
// Ejecuta un comando oled y deja la respuesta en el writer. La longitud del
// cuerpo ya la ha comprobado el reader
static void apply_screen_command(uint8_t op, const uint8_t *body, uint8_t len)
{
    if (screen_addr == 0) {
        send_error(op, PROTO_ERR_I2C);
        return;
    }

    switch (op) {
        case OP_OLED_TEXT: {
            uint8_t row = body[0];
            if (row >= SCREEN_ROWS) {
                send_error(op, PROTO_ERR_BAD_PARAM);
                break;
            }
            // Escribir texto pasa a modo manual: si no, el modo auto lo
            // taparía en el siguiente repintado
            if (mode == PROTO_OLED_AUTO) {
                mode = PROTO_OLED_MANUAL;
                clear_screen();
            }
            char text[PROTO_OLED_TEXT_MAX + 1];
            memcpy(text, &body[1], len - 1);
            text[len - 1] = '\0';
            draw_row(row, text);   // lo que no cabe en la fila se pierde
            send_ack(op);
            break;
        }

        case OP_OLED_CLEAR:
            mode = PROTO_OLED_MANUAL;
            clear_screen();
            send_ack(op);
            break;

        case OP_OLED_MODE:
            if (body[0] == PROTO_OLED_AUTO) {
                mode = PROTO_OLED_AUTO;
                draw_all();
            }
            else if (body[0] == PROTO_OLED_MANUAL) {
                mode = PROTO_OLED_MANUAL;
                clear_screen();
            }
            else {
                send_error(op, PROTO_ERR_BAD_PARAM);
                break;
            }
            send_ack(op);
            break;

        default:
            send_error(op, PROTO_ERR_UNKNOWN_OP);
            break;
    }
}


/* ------------------------- Hebra de la pantalla --------------------------- */
static THD_FUNCTION(screen, arg)
{
    (void)arg;

    // Estado inicial hasta que cada sensor mande el suyo
    for (int i = 0; i < N_SENSORS; i++) {
        state[i].sensor = i + 1;
        state[i].unit = PROTO_UNIT_CM;
        state[i].result = NO_MEASURE;
    }

    if (screen_addr != 0) {
        chMtxLock(&i2c_mtx);
        oled.begin(&Adafruit128x64, screen_addr);   // sin pin RST: no usa delay()
        oled.setFont(System5x7);
        chMtxUnlock(&i2c_mtx);
        draw_all();
    }

    while (true) {
        // Si hay algo pendiente de pintar no se espera: primero se vacía el
        // mailbox y, cuando ya no queda nada, se pinta todo de una vez. Así,
        // si los sensores miden más rápido de lo que se tarda en pintar, la
        // pantalla no se queda atrás (solo pinta el último estado)
        sysinterval_t wait = dirty ? TIME_IMMEDIATE : TIME_INFINITE;

        screen_msg_t *p = nullptr;
        msg_t result = chMBFetchTimeout(&mb_screen, (msg_t *)&p, wait);

        if (result == MSG_OK) {
            if (p->op == SCREEN_SENSOR_STATE) update_sensor(&p->state);
            else                              apply_screen_command(p->op, p->body, p->len);
            chPoolFree(&screenMemoryPool, p);   // ¡siempre liberar!
        }
        else {
            // MSG_TIMEOUT: el mailbox está vacío, toca pintar
            draw_dirty();
        }
    }
}

void startScreen()
{
    // Vinculamos el memory pool con el array de mensajes
    chPoolLoadArray(&screenMemoryPool, screenPool, N_SLOTS);

    chThdCreateStatic(waScreen, sizeof(waScreen), SCREEN_THD_PRIO, screen, NULL);
}
