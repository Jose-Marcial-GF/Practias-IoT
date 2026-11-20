//------------------------------------------------------------------------------
// receiver.cpp - Lee lo que manda el sensor por Serial1 y lo muestra en el
// monitor. Formato de las respuestas en sensor/writer.cpp:
//   ACK      RESP  | disp | NO_MESSAGE | op                    (sin cuerpo)
//   ERROR    ERROR | disp | MESSAGE    | op   + código(1)
//   MEDIDA   RESP  | disp | MESSAGE    | US_ONESHOT o OP_US_PERIODIC_ON
//                                           + unidad(1) valor(2)
//   STATUS   RESP  | disp | MESSAGE    | OP_STATUS  + 7 bytes
//   LISTA    RESP  | SENSORES | MESSAGE | US_LIST   + n(1) (num, dir) * n
// La longitud del cuerpo no viaja: se deduce de la cabecera
//------------------------------------------------------------------------------
#include "config.h"

#define BYTE_TIMEOUT_MS  100   // espera máxima entre bytes de un mismo mensaje
#define MAX_LIST         8     // sensores como mucho en una lista

static THD_WORKING_AREA(waReceiver, 2048);

static uint8_t body[1 + 2 * MAX_LIST];

// Espera el siguiente byte de Serial1 como mucho timeout_ms (0: sin límite).
// Devuelve false si no llega a tiempo
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

static const char * unitName(uint8_t unit)
{
  if (unit == PROTO_UNIT_INC) return "in";
  if (unit == PROTO_UNIT_CM)  return "cm";
  return "us";
}

// Pareja código -> texto. El texto NULL marca el final de cada tabla
typedef struct {
  uint8_t code;
  const char * text;
} name_t;

// Nombre del comando según el código de operación de la cabecera
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

// Mensaje según el código de error que envía el sensor
static const name_t errorNames[] = {
  { PROTO_ERR_I2C_SENSOR1,  TXT_ERR_I2C_SENSOR1 },
  { PROTO_ERR_I2C_SENSOR2,  TXT_ERR_I2C_SENSOR2 },
  { PROTO_ERR_I2C_SENSORES, TXT_ERR_I2C_SENSORES },
  { PROTO_ERR_I2C_PANTALLA, TXT_ERR_I2C_PANTALLA },
  { PROTO_ERROR_BUSY,       TXT_ERR_BUSY },
  { 0, NULL }
};

// Texto que corresponde al código en la tabla, o notFound si no está
static const char * nameOf(const name_t * table, uint8_t code, const char * notFound)
{
  for (int i = 0; table[i].text != NULL; i++) {
    if (table[i].code == code) return table[i].text;
  }
  return notFound;
}

static const char * opName(uint8_t op)      { return nameOf(opNames, op, TXT_UNKNOWN_OP); }
static const char * errorName(uint8_t code) { return nameOf(errorNames, code, TXT_ERR_UNKNOWN); }

// "Sensor 1" o "Pantalla" según el campo dispositivo de la cabecera
static void printDevice(uint8_t device)
{
  if (device == 0) SerialUSB.print("Pantalla");
  else { SerialUSB.print("Sensor "); SerialUSB.print(device); }
}

// Lee el cuerpo de una respuesta y la muestra. Devuelve false si la cabecera
// no es válida o el cuerpo no llega completo (se descarta)
static bool handleMessage(uint8_t head)
{
  bool    resp    = head & RESP;
  bool    message = head & MESSAGE;
  uint8_t device  = (head & SENSORES) >> SENSOR_POSITION;
  uint8_t op      = head & 0x0F;

  // ERROR: siempre lleva el código de error
  if (!resp) {
    if (!message || !readBody(0, 1)) return false;
    chMtxLock(&usbMutex);
    SerialUSB.print("ERROR ");
    printDevice(device);
    SerialUSB.print(" (");
    SerialUSB.print(opName(op));
    SerialUSB.print("): ");
    SerialUSB.println(errorName(body[0]));
    chMtxUnlock(&usbMutex);
    return true;
  }

  // ACK: sin cuerpo
  if (!message) {
    chMtxLock(&usbMutex);
    printDevice(device);
    SerialUSB.print(" ");
    SerialUSB.print(opName(op));
    SerialUSB.println(": OK");
    chMtxUnlock(&usbMutex);
    return true;
  }

  switch (op) {
    case US_LIST: {
      if (!readBody(0, 1) || body[0] > MAX_LIST || !readBody(1, 2 * body[0])) return false;
      chMtxLock(&usbMutex);
      SerialUSB.print("Sensores disponibles: ");
      SerialUSB.println(body[0]);
      for (int i = 0; i < body[0]; i++) {
        SerialUSB.print("  Sensor ");
        SerialUSB.print(body[1 + 2 * i]);
        SerialUSB.print(": dirección I2C 0x");
        SerialUSB.println(body[2 + 2 * i] << 1, HEX);   // 8 bits, como en el SRF02
      }
      chMtxUnlock(&usbMutex);
      return true;
    }

    case OP_STATUS:   // dirección, unidad, retardo(2), periódico, periodo(2)
      if (!readBody(0, 7)) return false;
      chMtxLock(&usbMutex);
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
      chMtxUnlock(&usbMutex);
      return true;

    case US_ONESHOT:
    case OP_US_PERIODIC_ON:   // unidad, valor(2)
      if (!readBody(0, 3)) return false;
      chMtxLock(&usbMutex);
      printDevice(device);
      SerialUSB.print(op == US_ONESHOT ? " (one-shot): " : " (periódica): ");
      SerialUSB.print(PROTO_GET16(&body[1]));
      SerialUSB.print(" ");
      SerialUSB.println(unitName(body[0]));
      chMtxUnlock(&usbMutex);
      return true;

    default:
      return false;   // cabecera que no corresponde a ninguna respuesta
  }
}

static THD_FUNCTION(receiverThread, arg)
{
  (void)arg;
  uint8_t head;

  while (true) {
    readByte(&head, 0);   // la cabecera se espera sin límite
    // Si la cabecera no es válida o el cuerpo no llega, se descarta y se
    // sigue con el siguiente byte: así se recupera el sincronismo
    handleMessage(head);
  }
}

void receiverStart()
{
  chThdCreateStatic(waReceiver, sizeof(waReceiver),
                    NORMALPRIO + 2, receiverThread, NULL);
}
