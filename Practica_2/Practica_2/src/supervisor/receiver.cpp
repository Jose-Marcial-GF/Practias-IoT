#include "config.h"
#include "threads.h"

static THD_WORKING_AREA(waReceiver, 2048);

static uint8_t body[255];              // la longitud es un byte: nunca más de 255

// Espera y devuelve el siguiente byte de Serial1
static uint8_t readByte()
{
  while (Serial1.available() == 0) chThdSleepMilliseconds(1);
  return Serial1.read();
}

static const char * unitName(int unit)
{
  if (unit == PROTO_UNIT_INC) return "in";
  if (unit == PROTO_UNIT_CM)  return "cm";
  return "µs";
}

// Escribe en el monitor un mensaje completo del sensor
static void showMessage(int op, int len)
{
  switch (op) {

    case OP_RSP_ACK:
      SerialUSB.println("OK");
      break;

    case OP_RSP_ERROR:                 // cuerpo: código de la petición, código de error
      SerialUSB.print("ERROR (op 0x");
      SerialUSB.print(body[0], HEX);   // código de la petición que falló
      SerialUSB.print("): ");
      if (body[1] == PROTO_ERR_UNKNOWN_OP)    SerialUSB.println("operación desconocida");
      else if (body[1] == PROTO_ERR_NO_SENSOR) SerialUSB.println("el sensor no existe");
      else if (body[1] == PROTO_ERR_BAD_PARAM) SerialUSB.println("parámetro inválido");
      else if (body[1] == PROTO_ERR_I2C)       SerialUSB.println("fallo de I2C");
      else                                     SerialUSB.println("mensaje mal formado");
      break;

    case OP_RSP_LIST:                  // cuerpo: cantidad y, por sensor, número y dirección
      SerialUSB.print("Sensores disponibles: ");
      SerialUSB.println(body[0]);
      for (int i = 1; i + 1 < len; i += 2) {
        SerialUSB.print("  Sensor ");
        SerialUSB.print(body[i]);
        SerialUSB.print(": dirección I2C 0x");
        SerialUSB.println(body[i + 1], HEX);
      }
      break;

    case OP_RSP_STATUS:                // cuerpo: sensor, dirección, unidad, retardo, periódico, periodo
      SerialUSB.print("Sensor ");
      SerialUSB.println(body[0]);
      SerialUSB.print("  dirección I2C: 0x");
      SerialUSB.println(body[1], HEX);
      SerialUSB.print("  unidad: ");
      SerialUSB.println(unitName(body[2]));
      SerialUSB.print("  retardo mínimo: ");
      SerialUSB.print(PROTO_GET16(&body[3]));
      SerialUSB.println(" ms");
      if (body[5]) {
        SerialUSB.print("  medida periódica cada ");
        SerialUSB.print(PROTO_GET16(&body[6]));
        SerialUSB.println(" ms");
      } else {
        SerialUSB.println("  medida periódica: no");
      }
      break;

    case OP_RSP_MEASURE:               // cuerpo: sensor, unidad, valor
      SerialUSB.print("Sensor ");
      SerialUSB.print(body[0]);
      SerialUSB.print(": ");
      SerialUSB.print(PROTO_GET16(&body[2]));
      SerialUSB.print(" ");
      SerialUSB.println(unitName(body[1]));
      break;
  }
}

static THD_FUNCTION(receiverThread, arg)
{
  while (true) {
    int op = readByte();

    // El primer byte siempre es un código de respuesta. Si no lo es, es un
    // byte suelto y se ignora: así se recupera el sincronismo
    if (op < OP_RSP_ACK || op > OP_RSP_MEASURE) continue;

    int len = readByte();
    for (int i = 0; i < len; i++) body[i] = readByte();

    chMtxLock(&usbMutex);
    showMessage(op, len);
    chMtxUnlock(&usbMutex);
  }
}

void receiverStart()
{
  chThdCreateStatic(waReceiver, sizeof(waReceiver),
                    NORMALPRIO + 2, receiverThread, NULL);
}
