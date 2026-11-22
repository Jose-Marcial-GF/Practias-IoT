#include "config.h"

#define BYTE_TIMEOUT_MS  100
#define MAX_LIST         8

static THD_WORKING_AREA(waReceiver, 2048);

static uint8_t body[1 + 2 * MAX_LIST];

static bool readByte(uint8_t *byte, uint32_t timeout_ms)
{
  systime_t start = chVTGetSystemTimeX();
  while (Serial1.available() == 0) {
    if (timeout_ms != 0 && chVTTimeElapsedSinceX(start) > TIME_MS2I(timeout_ms))
      return false;
    chThdSleepMilliseconds(1);
  }
  *byte = Serial1.read();
  return true;
}

static bool readBody(uint8_t from, uint8_t len)
{
  for (uint8_t i = from; i < from + len; i++)
    if (!readByte(&body[i], BYTE_TIMEOUT_MS)) return false;
  return true;
}

typedef struct {
  uint8_t code;
  const char * text;
} name_t;

static const name_t opNames[] = {
  { US_LIST,            TXT_LIST },
  { US_ONESHOT,         WORD_ONESHOT },
  { OP_US_PERIODIC_ON,  WORD_ON },
  { OP_US_PERIODIC_OFF, WORD_OFF },
  { OP_STATUS,          WORD_STATUS },
  { OP_US_SET_DELAY,    WORD_DELAY },
  { PROTO_UNIT_INC,     WORD_UNIT " " WORD_INC },
  { PROTO_UNIT_CM,      WORD_UNIT " " WORD_CM },
  { PROTO_UNIT_MS,      WORD_UNIT " " WORD_MS },
  { OP_OLED_ON,         WORD_OLED " " WORD_ON },
  { OP_OLED_OFF,        WORD_OLED " " WORD_OFF },
  { 0, NULL }
};

static const name_t errorNames[] = {
  { PROTO_ERR_I2C_SENSOR1,  TXT_ERR_I2C_SENSOR1 },
  { PROTO_ERR_I2C_SENSOR2,  TXT_ERR_I2C_SENSOR2 },
  { PROTO_ERR_I2C_SENSORES, TXT_ERR_I2C_SENSORES },
  { PROTO_ERR_I2C_PANTALLA, TXT_ERR_I2C_PANTALLA },
  { PROTO_ERROR_BUSY,       TXT_ERR_BUSY },
  { 0, NULL }
};

static const char * nameOf(const name_t * table, uint8_t code, const char * notFound)
{
  for (int i = 0; table[i].text != NULL; i++) {
    if (table[i].code == code) return table[i].text;
  }
  return notFound;
}

static const char * opName(uint8_t op)      { return nameOf(opNames, op, TXT_UNKNOWN_OP); }
static const char * errorName(uint8_t code) { return nameOf(errorNames, code, TXT_ERR_UNKNOWN); }

static void printDevice(uint8_t device)
{
  if (device == 0) SerialUSB.print("Pantalla");
  else { SerialUSB.print("Sensor "); SerialUSB.print(device); }
}

static bool handleMessage(uint8_t head)
{
  bool    resp    = head & RESP;
  bool    message = head & MESSAGE;
  uint8_t device  = (head & SENSORES) >> SENSOR_POSITION;
  uint8_t op      = head & 0x0F;

  if (!resp) {
    if (!message || !readBody(0, 1)) return false;
    outputBegin();
    SerialUSB.print("ERROR ");
    printDevice(device);
    SerialUSB.print(" (");
    SerialUSB.print(opName(op));
    SerialUSB.print("): ");
    SerialUSB.println(errorName(body[0]));
    outputEnd();
    return true;
  }

  if (!message) {
    outputBegin();
    printDevice(device);
    SerialUSB.print(" ");
    SerialUSB.print(opName(op));
    SerialUSB.println(": OK");
    outputEnd();
    return true;
  }

  switch (op) {
    case US_LIST: {
      if (!readBody(0, 1) || body[0] > MAX_LIST || !readBody(1, 2 * body[0])) return false;
      outputBegin();
      SerialUSB.print("Sensores disponibles: ");
      SerialUSB.println(body[0]);
      for (int i = 0; i < body[0]; i++) {
        SerialUSB.print("  Sensor ");
        SerialUSB.print(body[1 + 2 * i]);
        SerialUSB.print(": dirección I2C 0x");
        SerialUSB.println(body[2 + 2 * i] << 1, HEX);
      }
      outputEnd();
      return true;
    }

    case OP_STATUS:
      if (!readBody(0, 7)) return false;
      outputBegin();
      printDevice(device);
      SerialUSB.println(":");
      SerialUSB.print("  dirección I2C: 0x");
      SerialUSB.println(body[0] << 1, HEX);
      SerialUSB.print("  unidad: ");
      SerialUSB.println(unitName(body[1]));
      SerialUSB.print("  retardo mínimo: ");
      SerialUSB.print(PROTO_GET16(&body[2]));
      SerialUSB.println(" ms");
      if (body[4]) {
        SerialUSB.print("  medida periódica cada ");
        SerialUSB.print(PROTO_GET16(&body[5]));
        SerialUSB.println(" ms");
      } else {
        SerialUSB.println("  medida periódica: no");
      }
      outputEnd();
      return true;

    case US_ONESHOT:
    case OP_US_PERIODIC_ON:
      if (!readBody(0, 3)) return false;
      outputBegin();
      printDevice(device);
      SerialUSB.print(op == US_ONESHOT ? " (one-shot): " : " (periódica): ");
      SerialUSB.print(PROTO_GET16(&body[1]));
      SerialUSB.print(" ");
      SerialUSB.println(unitName(body[0]));
      outputEnd();
      return true;

    default:
      return false;
  }
}

static SEMAPHORE_DECL(respSem, 0);
static MUTEX_DECL(respMutex);
static uint8_t pendingDevice;
static uint8_t pendingOp;
static uint8_t pendingLeft = 0;

void expectResponse(uint8_t device, uint8_t op)
{
  chMtxLock(&respMutex);
  pendingDevice = device;
  pendingOp     = op;
  pendingLeft   = (device == DEVICE_BOTH && op != US_LIST) ? N_SENSORS : 1;
  chSemReset(&respSem, 0);
  chMtxUnlock(&respMutex);
}

uint8_t waitResponse(uint32_t timeout_ms)
{
  systime_t start = chVTGetSystemTimeX();
  sysinterval_t timeout = TIME_MS2I(timeout_ms);

  while (true) {
    chMtxLock(&respMutex);
    uint8_t left = pendingLeft;
    chMtxUnlock(&respMutex);
    if (left == 0) break;

    sysinterval_t elapsed = chVTTimeElapsedSinceX(start);
    if (elapsed >= timeout) break;
    chSemWaitTimeout(&respSem, timeout - elapsed);
  }

  chMtxLock(&respMutex);
  uint8_t left = pendingLeft;
  pendingLeft = 0;
  chMtxUnlock(&respMutex);
  return left;
}

static void notifyResponse(uint8_t head)
{
  bool    resp    = head & RESP;
  bool    message = head & MESSAGE;
  uint8_t device  = (head & SENSORES) >> SENSOR_POSITION;
  uint8_t op      = head & 0x0F;

  if (op == OP_US_PERIODIC_ON) {
    if (resp && message) return;
    if (!resp && body[0] != PROTO_ERROR_BUSY) return;
  }

  chMtxLock(&respMutex);
  if (pendingLeft > 0 && op == pendingOp) {
    bool match = true;
    if (device == pendingDevice)
      pendingLeft = 0;
    else if (pendingDevice == DEVICE_BOTH && device >= 1 && device <= N_SENSORS)
      pendingLeft--;
    else
      match = false;
    if (match) chSemSignal(&respSem);
  }
  chMtxUnlock(&respMutex);
}

static THD_FUNCTION(receiverThread, arg)
{
  (void)arg;
  uint8_t head;

  while (true) {
    readByte(&head, 0);
    if (handleMessage(head)) notifyResponse(head);
  }
}

void receiverStart()
{
  chThdCreateStatic(waReceiver, sizeof(waReceiver),
                    NORMALPRIO + 2, receiverThread, NULL);
}
