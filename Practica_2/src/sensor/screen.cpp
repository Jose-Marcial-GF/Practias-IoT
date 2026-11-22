#include <stdio.h>
#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"
#include "messages.h"

#define SCREEN_THD_PRIO  (NORMALPRIO + 1)
#define SCREEN_ADDR      0x3D
#define SCREEN_RETRY_MS  5000
#define SCREEN_ROWS      8
#define SCREEN_COLS      21

#define LINES_PER_SENSOR  2
#define ROW_TITLE         0
#define TITLE_LINES         1
#define ROW_SENSOR(id, line)    (TITLE_LINES + LINES_PER_SENSOR * ((id) - 1) + (line))
#define ROW_ACTION_TITLE  6
#define ROW_ACTION        7

#define DIRTY_SENSOR(id)  (1 << ((id) - 1))
#define DIRTY_ACTION      0x80

screen_msg_t screenPool[N_SLOTS];
MEMORYPOOL_DECL(screenMemoryPool, sizeof(screen_msg_t), PORT_NATURAL_ALIGN, NULL);

msg_t slots_screen[N_SLOTS];
MAILBOX_DECL(mb_screen, &slots_screen, N_SLOTS);

static THD_WORKING_AREA(waScreen, 2048);

static SSD1306AsciiWire oled;
static bool    screen_on = true;
static sensor_state_t state[N_SENSORS];
static uint8_t last_sensor = 0;

typedef struct {
    uint8_t  result;
    uint8_t  unit;
    uint16_t value;
} last_measure_t;
static last_measure_t measure[N_SENSORS];
static uint8_t dirty = 0;

bool post_screen_command(uint8_t op)
{
    screen_msg_t *p = (screen_msg_t *)chPoolAlloc(&screenMemoryPool);
    if (!p) return false;

    p->op = op;

    if (chMBPostTimeout(&mb_screen, (msg_t)p, TIME_IMMEDIATE) != MSG_OK) {
        chPoolFree(&screenMemoryPool, p);
        return false;
    }
    return true;
}

void post_sensor_state(const sensor_state_t *s)
{
    screen_msg_t *p = (screen_msg_t *)chPoolAlloc(&screenMemoryPool);
    if (!p) return;

    p->op = SCREEN_SENSOR_STATE;
    p->state = *s;

    if (chMBPostTimeout(&mb_screen, (msg_t)p, TIME_IMMEDIATE) != MSG_OK)
        chPoolFree(&screenMemoryPool, p);
}

static bool probe(uint8_t address)
{
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

uint8_t check_screen()
{
    chMtxLock(&i2c_mtx);
    uint8_t address = probe(SCREEN_ADDR) ? SCREEN_ADDR : 0;
    chMtxUnlock(&i2c_mtx);
    return address;
}

static void draw_row(uint8_t row_index, const char *text)
{
    chMtxLock(&i2c_mtx);
    oled.setCursor(0, row_index);
    oled.print(text);
    oled.clearToEOL();
    chMtxUnlock(&i2c_mtx);
}

static void set_display(bool on)
{
    chMtxLock(&i2c_mtx);
    oled.ssd1306WriteCmd(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
    chMtxUnlock(&i2c_mtx);
}

static void draw_sensor(int id)
{
    const sensor_state_t *s = &state[id - 1];
    const last_measure_t *m = &measure[id - 1];
    char line[SCREEN_COLS + 1];
    char value[8];

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

static void draw_all()
{
    draw_row(ROW_TITLE, "DISPOSITIVO SENSOR");
    for (int id = 1; id <= N_SENSORS; id++) draw_sensor(id);
    draw_row(ROW_SENSOR(N_SENSORS, LINES_PER_SENSOR), "");
    draw_row(ROW_ACTION_TITLE, "Ultima accion:");
    draw_action();
    dirty = 0;
}

static void draw_dirty()
{
    if (screen_on) {
        for (int id = 1; id <= N_SENSORS; id++)
            if (dirty & DIRTY_SENSOR(id)) draw_sensor(id);
        if (dirty & DIRTY_ACTION) draw_action();
    }
    dirty = 0;
}

static void update_sensor(const sensor_state_t *s)
{
    if (s->sensor < 1 || s->sensor > N_SENSORS) return;
    state[s->sensor - 1] = *s;
    if (s->result != NO_MEASURE) {
        last_measure_t *m = &measure[s->sensor - 1];
        m->result = s->result;
        m->unit   = s->unit;
        m->value  = s->value;
    }
    dirty |= DIRTY_SENSOR(s->sensor);
    if (s->event != NO_SHOW_OPERATION) {
        last_sensor = s->sensor;
        dirty |= DIRTY_ACTION;
    }
}

static void apply_screen_command(uint8_t op)
{
    if (op == OP_OLED_ON) {
        screen_on = true;
        set_display(true);
        draw_all();
    } else {
        screen_on = false;
        set_display(false);
    }
    send_ack(0, op);
}

static THD_FUNCTION(screen, arg)
{
    (void)arg;

    for (int i = 0; i < N_SENSORS; i++) {
        state[i].sensor = i + 1;
        state[i].event = NO_SHOW_OPERATION;
        state[i].unit = PROTO_UNIT_CM;
        state[i].result = NO_MEASURE;
    }

    while (!check_screen()) chThdSleepMilliseconds(SCREEN_RETRY_MS);

    chMtxLock(&i2c_mtx);
    oled.begin(&Adafruit128x64, SCREEN_ADDR);
    oled.setFont(System5x7);
    chMtxUnlock(&i2c_mtx);
    draw_all();

    while (true) {
        sysinterval_t wait = dirty ? TIME_IMMEDIATE : TIME_INFINITE;

        screen_msg_t *p = nullptr;
        msg_t result = chMBFetchTimeout(&mb_screen, (msg_t *)&p, wait);

        if (result == MSG_OK) {
            if (p->op == SCREEN_SENSOR_STATE) update_sensor(&p->state);
            else                              apply_screen_command(p->op);
            chPoolFree(&screenMemoryPool, p);
        }
        else {
            draw_dirty();
        }
    }
}

void startScreen()
{
    chPoolLoadArray(&screenMemoryPool, screenPool, N_SLOTS);

    chThdCreateStatic(waScreen, sizeof(waScreen), SCREEN_THD_PRIO, screen, NULL);
}
