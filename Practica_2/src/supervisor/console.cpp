#include <stdlib.h>
#include <string.h>
#include "config.h"
#include <stdint.h>
#include <stdio.h>

static THD_WORKING_AREA(waConsole, 2048);

static char input[CONSOLE_LINE_MAX];
static int  inputLen = 0;

void outputBegin()
{
  chMtxLock(&usbMutex);
  if (inputLen > 0) SerialUSB.println();
}

void outputEnd()
{
  if (inputLen > 0) SerialUSB.write((const uint8_t *)input, inputLen);
  chMtxUnlock(&usbMutex);
}

static uint8_t say(const char * text)
{
  chMtxLock(&usbMutex);
  SerialUSB.print(text);
  chMtxUnlock(&usbMutex);
  return 1;
}

static uint8_t printHelp()
{
  say("Comandos:\n"
      "  help\n"
      "  us                         lista los sensores\n"
      "  us <n> one-shot            una medida del sensor n\n"
      "  us <n> on <ms>             medida periódica cada <ms>\n"
      "  us <n> off                 detiene la medida periódica\n"
      "  us <n> unit inc|cm|ms      unidad de la medida\n"
      "  us <n> delay <ms>          retardo mínimo entre medidas\n"
      "  us <n> status              estado del sensor\n"
      "  oled on                 borra la pantalla\n"
      "  oled off                 borra la pantalla\n"
      "  n = [1: first, 2: secound, both | 3: both ]\n"
  );
  return 0;
}
static uint8_t unExpectedArgumentError()
{
  say("Sobran argumentos");
  return 1 ;
}
static uint8_t ArgumentError(const char* message)
{
  say(message);
  return 1 ;
}

static bool is(const char * word, const char * text)
{
  return word != NULL && strcmp(word, text) == 0;
}

static uint16_t knownDelay[N_SENSORS] = { MIN_DELAY_SFR, MIN_DELAY_SFR };

static uint8_t request(uint8_t device, uint8_t op, uint8_t *body, uint8_t len,
                       uint32_t timeout_ms)
{
  expectResponse(device, op);
  if (len == 0) sendHead(OP, device << SENSOR_POSITION, NO_MESSAGE, op);
  else          sendMessage(OP, device << SENSOR_POSITION, MESSAGE, op, body, len);

  uint8_t left = waitResponse(timeout_ms);
  if (left > 0) {
    chMtxLock(&usbMutex);
    SerialUSB.print("Sin respuesta del dispositivo sensor en ");
    SerialUSB.print(timeout_ms);
    SerialUSB.println(" ms");
    chMtxUnlock(&usbMutex);
  }
  return left;
}

static uint8_t sendCommand(uint8_t device, uint8_t op)
{
  return request(device, op, NULL, 0, RESP_TIMEOUT_MS);
}

static uint8_t sendOneShot(uint8_t device)
{
  uint16_t delay = 0;
  for (int i = 1; i <= N_SENSORS; i++)
    if ((device == i || device == DEVICE_BOTH) && knownDelay[i - 1] > delay)
      delay = knownDelay[i - 1];
  return request(device, US_ONESHOT, NULL, 0, RESP_TIMEOUT_MS + delay);
}

static uint8_t sendUnit(uint8_t sensor, char* word){
  if(is(word, "inc")) return sendCommand(sensor, PROTO_UNIT_INC);
  if(is(word, "cm")) return sendCommand(sensor, PROTO_UNIT_CM);
  if(is(word, "ms")) return sendCommand(sensor, PROTO_UNIT_MS);
  return ArgumentError("unidad de medida no conocida");
}

static bool readMs(const char* word, uint16_t* ms){
  if (word[0] == '-') return false;
  if (strtoul(word, NULL, 10) > MAX_DELAY_SFR) return false;
  *ms = strtoul(word, NULL, 10);
  return *ms >= MIN_DELAY_SFR;
}

static uint8_t sendDelay(uint8_t sensor, char* word){
  uint16_t delay;
  if (!readMs(word, &delay)) return ArgumentError("Valor no esperado");
  uint8_t body[2];
  PROTO_PUT16(body, delay);
  uint8_t left = request(sensor, OP_US_SET_DELAY, body, sizeof(body), RESP_TIMEOUT_MS);

  for (int i = 1; i <= N_SENSORS; i++)
    if (sensor == i || sensor == DEVICE_BOTH) knownDelay[i - 1] = delay;
  return left;
}

static uint8_t sendOn(uint8_t sensor, char* word){
  uint16_t period;
  if (!readMs(word, &period)) return ArgumentError("Valor no esperado");
  uint8_t body[2];
  PROTO_PUT16(body, period);
  return request(sensor, OP_US_PERIODIC_ON, body, sizeof(body), RESP_TIMEOUT_MS);
}

static uint8_t handleLine(char * line)
{
  uint8_t sensor;

  char * word[5];
  word[0] = strtok(line, " ");
  word[1] = strtok(NULL, " ");
  word[2] = strtok(NULL, " ");
  word[3] = strtok(NULL, " ");
  word[4] = strtok(NULL, "\n");

  if (word[0] == NULL) return say("No se ha escrito nada");
  if (is(word[0], "help")) { return printHelp();  }

  if (is(word[0], "us")){
    if (word[1] == NULL) return sendCommand(DEVICE_BOTH, US_LIST);
    if (is(word[1], "both")) {sensor = DEVICE_BOTH;}

    else {
      if (word[1][0] == '-') return say("El sensor mencionado no existe");
      sensor = (uint8_t)strtol(word[1], NULL, 10);
      if (sensor < 1 || sensor > DEVICE_BOTH) return say("El sensor mencionado no existe");
      }

    if (is(word[2], "one-shot")) return word[3] == NULL ? sendOneShot(sensor) : unExpectedArgumentError();

    if (is(word[2], "off")) return word[3] == NULL ? sendCommand(sensor, OP_US_PERIODIC_OFF) : unExpectedArgumentError();

    if (is(word[2], "status")) return word[3] == NULL ? sendCommand(sensor, OP_STATUS) : unExpectedArgumentError();

    if (is(word[2], "unit")) return word[3] != NULL && word[4] == NULL ? sendUnit(sensor, word[3]) : unExpectedArgumentError();

    if (is(word[2], "delay")) return word[3] != NULL && word[4] == NULL ? sendDelay(sensor, word[3]) : unExpectedArgumentError();

    if (is(word[2], "on")) return word[3] != NULL && word[4] == NULL ? sendOn(sensor, word[3]) : unExpectedArgumentError();
  }
  if (is(word[0], "oled")){
    if (is(word[1], "off")) return word[2] == NULL ? sendCommand(DEVICE_SCREEN, OP_OLED_OFF) : unExpectedArgumentError();
    if (is(word[1], "on")) return word[2] == NULL ?  sendCommand(DEVICE_SCREEN, OP_OLED_ON) : unExpectedArgumentError();
  }
  return say("Comando no reconocido (help muestra los comandos)\n");
}

static THD_FUNCTION(consoleThread, arg)
{
  char line[CONSOLE_LINE_MAX];

  say("Supervisor listo. Escribe help para ver los comandos.\n");

  while (true) {
    while (SerialUSB.available() > 0) {
      char c = SerialUSB.read();
      bool run = false;

      chMtxLock(&usbMutex);
      if (c == '\r' || c == '\n') {
        if (inputLen > 0) {
          memcpy(line, input, inputLen);
          line[inputLen] = '\0';
          inputLen = 0;
          SerialUSB.print("\n");
          run = true;
        }
      }
      else if (c == '\b' || c == 127) {
        if (inputLen > 0) {
          inputLen--;
          SerialUSB.print("\b \b");
        }
      }
      else if (inputLen < CONSOLE_LINE_MAX - 1) {
        input[inputLen++] = c;
        SerialUSB.write(c);
      }
      chMtxUnlock(&usbMutex);

      if (run) handleLine(line);
    }
    chThdSleepMilliseconds(10);
  }
}

void consoleStart()
{
  chThdCreateStatic(waConsole, sizeof(waConsole),
                    NORMALPRIO + 1, consoleThread, NULL);
}
