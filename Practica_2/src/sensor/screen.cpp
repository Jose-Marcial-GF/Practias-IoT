//------------------------------------------------------------------------------
// screen.cpp - Hebra de la pantalla OLED (SSD1306 128x64 por I2C). Espera
// mensajes en su mailbox y los pinta. Le escriben dos tipos de hebra:
//   - reader: comandos oled del supervisor (on, off). La pantalla contesta
//     con ACK o ERROR a través del writer, como los sensores.
//   - sensor: el estado de su SRF02 cada vez que cambia o mide.
// Mismo esquema que sensor.cpp y writer.cpp: memory pool + mailbox.
//
// La pantalla comparte el bus I2C con los SRF02: todo acceso a la pantalla se
// hace con i2c_mtx, igual que en sensor.cpp.
//------------------------------------------------------------------------------
#include <stdio.h>
#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"
#include "messages.h"

#define SCREEN_THD_PRIO  (NORMALPRIO + 1)
#define SCREEN_ADDR      0x3D   // dirección del SSD1306 (7 bits; 0x7A en 8 bits)
#define SCREEN_RETRY_MS  5000   // sin pantalla: cada cuánto se vuelve a buscar
#define SCREEN_ROWS      8      // 64 px / 8 px por fila (fuente System5x7)
#define SCREEN_COLS      21     // 128 px / 6 px por carácter

// Filas de la pantalla
#define LINES_PER_SENSOR  2
#define ROW_TITLE         0
#define TITLE_LINES         1
#define ROW_SENSOR(id, line)    (TITLE_LINES + LINES_PER_SENSOR * ((id) - 1) + (line))   // 2 filas por sensor: 1-2 y 3-4
#define ROW_ACTION_TITLE  6
#define ROW_ACTION        7

// Qué partes hay que repintar (un bit por sensor y la acción)
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
// Solo lo toca la hebra de la pantalla
static SSD1306AsciiWire oled;
static bool    screen_on = true;             // false tras "oled off"
static sensor_state_t state[N_SENSORS];       // último estado de cada sensor
static uint8_t last_sensor = 0;               // sensor de la última acción

// Última medida de cada sensor. Va aparte del estado porque no todos los
// mensajes traen medida (unit, delay, off...) y no debe borrarse. Guarda su
// propia unidad: si después se cambia la unidad, la medida sigue siendo la
// que se tomó (p. ej. 67in hasta la siguiente medida en cm)
typedef struct {
    uint8_t  result;     // NO_MEASURE, MEASURE_OK o MEASURE_ERROR
    uint8_t  unit;       // unidad en la que se tomó
    uint16_t value;
} last_measure_t;
static last_measure_t measure[N_SENSORS];
static uint8_t dirty = 0;                     // DIRTY_*: pendiente de pintar


/* ------------------------- Envío de mensajes a la pantalla ---------------- */
// Las llama otras hebras. No bloquean: si el mailbox está lleno el mensaje se
// pierde (como en send2Writer)

// reader -> screen. Devuelve false si no hay hueco
bool post_screen_command(uint8_t op)
{
    // Pedimos un objeto al memory pool
    screen_msg_t *p = (screen_msg_t *)chPoolAlloc(&screenMemoryPool);
    if (!p) return false;

    p->op = op;

    // Lo enviamos sin bloquear (TIME_IMMEDIATE)
    if (chMBPostTimeout(&mb_screen, (msg_t)p, TIME_IMMEDIATE) != MSG_OK) {
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

// Comprueba si la pantalla contesta. Devuelve su dirección o 0 si no hay pantalla
uint8_t check_screen()
{
    chMtxLock(&i2c_mtx);
    uint8_t address = probe(SCREEN_ADDR) ? SCREEN_ADDR : 0;
    chMtxUnlock(&i2c_mtx);
    return address;
}

// Escribe una fila entera (lo que sobre del texto se borra). El mutex se coge
// fila a fila, así los sensores pueden usar el bus entre una fila y otra
static void draw_row(uint8_t row_index, const char *text)
{
    chMtxLock(&i2c_mtx);
    oled.setCursor(0, row_index);
    oled.print(text);
    oled.clearToEOL();
    chMtxUnlock(&i2c_mtx);
}

// Enciende o apaga el panel (comandos 0xAF / 0xAE del SSD1306)
static void set_display(bool on)
{
    chMtxLock(&i2c_mtx);
    oled.ssd1306WriteCmd(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
    chMtxUnlock(&i2c_mtx);
}


/* ------------------------- Estado de los sensores ------------------------- */
// Dos filas por sensor:   S1 0xF2 cm d:300ms
//                          171cm    T:500ms
static void draw_sensor(int id)
{
    const sensor_state_t *s = &state[id - 1];
    const last_measure_t *m = &measure[id - 1];
    char line[SCREEN_COLS + 1];
    char value[8];

    // Dirección de 8 bits, como en la documentación del SRF02
    snprintf(line, sizeof(line), "S%d 0x%02X %s d:%ums",
             id, sensor_addr[id - 1] << 1, unitName(s->unit), s->delay_ms);
    draw_row(ROW_SENSOR(id, 0), line);

    if (m->result == MEASURE_OK)
        snprintf(value, sizeof(value), "%u%s", m->value, unitName(m->unit));
    else if (m->result == MEASURE_ERROR)
        snprintf(value, sizeof(value), "err I2C");
    else
        snprintf(value, sizeof(value), "---");

    if (s->periodic) snprintf(line, sizeof(line), " %-8s T:%ums", value, s->period_ms);
    else             snprintf(line, sizeof(line), " %-8s T:off", value);
    draw_row(ROW_SENSOR(id, 1), line);
}

// Última acción del dispositivo sensor
static void draw_action()
{
    char line[SCREEN_COLS + 1] = "";

    if (last_sensor != 0) {
        const sensor_state_t *s = &state[last_sensor - 1];
        int n = last_sensor;
        switch (s->event) {
            case US_ONESHOT:         snprintf(line, sizeof(line), "S%d one-shot", n); break;
            case OP_US_PERIODIC_ON:
                if (s->result == NO_MEASURE)
                    snprintf(line, sizeof(line), "S%d on cada %ums", n, s->period_ms);
                else
                    snprintf(line, sizeof(line), "S%d medida periodica", n);
                break;
            case OP_US_PERIODIC_OFF: snprintf(line, sizeof(line), "S%d periodico off", n); break;
            case PROTO_UNIT_INC:
            case PROTO_UNIT_CM:
            case PROTO_UNIT_MS:      snprintf(line, sizeof(line), "S%d unidad %s", n, unitName(s->unit)); break;
            case OP_US_SET_DELAY:    snprintf(line, sizeof(line), "S%d retardo %ums", n, s->delay_ms); break;
            case OP_STATUS:          snprintf(line, sizeof(line), "S%d status", n); break;
        }
    }
    draw_row(ROW_ACTION, line);
}

// Pinta la pantalla entera
static void draw_all()
{
    draw_row(ROW_TITLE, "DISPOSITIVO SENSOR");
    for (int id = 1; id <= N_SENSORS; id++) draw_sensor(id);
    draw_row(ROW_SENSOR(N_SENSORS, LINES_PER_SENSOR), "");
    draw_row(ROW_ACTION_TITLE, "Ultima accion:");
    draw_action();
    dirty = 0;
}

// Pinta solo lo que ha cambiado desde la última vez
static void draw_dirty()
{
    if (screen_on) {
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
    if (s->event != NO_SHOW_OPERATION) {   // no es una acción: no se pinta
        last_sensor = s->sensor;
        dirty |= DIRTY_ACTION;
    }
}


/* ------------------------- Comandos del supervisor ------------------------ */
// Ejecuta un comando oled y deja la respuesta en el writer. El reader ya ha
// comprobado que op es OP_OLED_ON u OP_OLED_OFF. Solo se llama con la pantalla
// ya detectada
static void apply_screen_command(uint8_t op)
{
    if (op == OP_OLED_ON) {
        screen_on = true;
        set_display(true);
        draw_all();          // mientras estaba apagada se ha seguido guardando el estado
    } else {
        screen_on = false;
        set_display(false);
    }
    send_ack(0, op);
}


/* ------------------------- Hebra de la pantalla --------------------------- */
static THD_FUNCTION(screen, arg)
{
    (void)arg;

    // Estado inicial hasta que cada sensor mande el suyo
    for (int i = 0; i < N_SENSORS; i++) {
        state[i].sensor = i + 1;
        state[i].event = NO_SHOW_OPERATION;
        state[i].unit = PROTO_UNIT_CM;
        state[i].result = NO_MEASURE;
    }

    // Sin pantalla la hebra duerme y vuelve a buscarla cada SCREEN_RETRY_MS.
    // A partir de aquí se da por hecho que la pantalla está conectada
    while (!check_screen()) chThdSleepMilliseconds(SCREEN_RETRY_MS);

    chMtxLock(&i2c_mtx);
    oled.begin(&Adafruit128x64, SCREEN_ADDR);   // sin pin RST: no usa delay()
    oled.setFont(System5x7);
    chMtxUnlock(&i2c_mtx);
    draw_all();   // pinta también el estado guardado mientras se esperaba

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
            else                              apply_screen_command(p->op);
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
