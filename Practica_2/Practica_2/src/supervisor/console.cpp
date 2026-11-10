#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "threads.h"

static THD_WORKING_AREA(waConsole, 2048);

// Escribe un texto en el monitor serie. Se usa el cerrojo porque la hebra
// receptor también escribe en SerialUSB y los mensajes no deben mezclarse.
static void say(const char * text)
{
  chMtxLock(&usbMutex);
  SerialUSB.print(text);
  chMtxUnlock(&usbMutex);
}

static void printHelp()
{
  say("Comandos:\r\n"
      "  us                         lista los sensores\r\n"
      "  us <n> one-shot            una medida del sensor n\r\n"
      "  us <n> on <ms>             medida periódica cada <ms>\r\n"
      "  us <n> off                 detiene la medida periódica\r\n"
      "  us <n> unit inc|cm|ms      unidad de la medida\r\n"
      "  us <n> delay <ms>          retardo mínimo entre medidas\r\n"
      "  us <n> status              estado del sensor\r\n"
      "  oled text <línea> <texto>  escribe en la pantalla (línea 0 = la primera)\r\n"
      "  oled clear                 borra la pantalla\r\n"
      "  oled mode auto|manual      auto: la pantalla muestra el estado de los sensores\r\n");
}

// true si la palabra existe y es igual al texto
static bool is(const char * word, const char * text)
{
  return word != NULL && strcmp(word, text) == 0;
}

// Envía un mensaje al sensor: código, longitud y cuerpo
static void sendMessage(uint8_t op, uint8_t * body, uint8_t len)
{
  Serial1.write(op);
  Serial1.write(len);
  Serial1.write(body, len);
}

// Convierte la línea escrita en un mensaje y lo envía. El supervisor solo
// traduce el texto: los valores (sensor, periodo...) los valida el sensor.
static void handleLine(char * line)
{
  uint8_t body[PROTO_MAX_BODY];
  uint8_t len = 0;
  uint8_t op = 0;                      // 0 = comando no reconocido

  // Tres palabras separadas por espacios y, de la cuarta, el resto de la línea
  // (así el texto de "oled text" puede llevar espacios)
  char * word[4];
  word[0] = strtok(line, " ");
  word[1] = strtok(NULL, " ");
  word[2] = strtok(NULL, " ");
  word[3] = strtok(NULL, "\r\n");

  if (word[0] == NULL) return;         // línea vacía

  if (is(word[0], "help")) {
    printHelp();
    return;
  }

  if (is(word[0], "us") && word[1] == NULL) {
    op = OP_US_LIST;
  }
  else if (is(word[0], "us") && word[2] != NULL) {
    body[0] = atoi(word[1]);           // número del sensor
    len = 1;

    if (is(word[2], "one-shot"))       op = OP_US_ONESHOT;
    else if (is(word[2], "off"))       op = OP_US_PERIODIC_OFF;
    else if (is(word[2], "status"))    op = OP_US_STATUS;
    else if (word[3] != NULL && (is(word[2], "on") || is(word[2], "delay"))) {
      int value = atoi(word[3]);       // milisegundos, en 2 bytes
      op = is(word[2], "on") ? OP_US_PERIODIC_ON : OP_US_SET_DELAY;
      body[1] = value & 0xFF;          // byte bajo primero
      body[2] = value >> 8;
      len = 3;
    }
    else if (word[3] != NULL && is(word[2], "unit")) {
      op = OP_US_SET_UNIT;
      len = 2;
      if (is(word[3], "inc"))          body[1] = PROTO_UNIT_INC;
      else if (is(word[3], "cm"))      body[1] = PROTO_UNIT_CM;
      else if (is(word[3], "ms"))      body[1] = PROTO_UNIT_MS;
      else                             op = 0;
    }
  }
  else if (is(word[0], "oled") && is(word[1], "clear")) {
    op = OP_OLED_CLEAR;
  }
  else if (is(word[0], "oled") && is(word[1], "mode") && word[2] != NULL) {
    op = OP_OLED_MODE;
    len = 1;
    if (is(word[2], "auto"))           body[0] = PROTO_OLED_AUTO;
    else if (is(word[2], "manual"))    body[0] = PROTO_OLED_MANUAL;
    else                               op = 0;
  }
  else if (is(word[0], "oled") && is(word[1], "text") && word[2] != NULL && word[3] != NULL) {
    int textLen = strlen(word[3]);
    if (textLen > PROTO_OLED_TEXT_MAX) textLen = PROTO_OLED_TEXT_MAX;
    op = OP_OLED_TEXT;
    body[0] = atoi(word[2]);           // número de línea
    memcpy(&body[1], word[3], textLen);
    len = 1 + textLen;
  }

  if (op == 0) say("Comando no reconocido (help muestra los comandos)\r\n");
  else         sendMessage(op, body, len);
}

static THD_FUNCTION(consoleThread, arg)
{
  char line[CONSOLE_LINE_MAX];
  int n = 0;                           // caracteres escritos en la línea

  say("Supervisor listo. Escribe help para ver los comandos.\r\n");

  while (true) {
    while (SerialUSB.available() > 0) {
      char c = SerialUSB.read();

      if (c == '\r' || c == '\n') {              // Enter: se ejecuta la línea
        if (n > 0) {
          line[n] = '\0';
          say("\r\n");
          handleLine(line);
          n = 0;
        }
      }
      else if (c == '\b' || c == 127) {          // borrar el último carácter
        if (n > 0) {
          n--;
          say("\b \b");
        }
      }
      else if (n < CONSOLE_LINE_MAX - 1) {       // carácter normal: se guarda y se muestra
        line[n++] = c;
        char text[2] = { c, '\0' };
        say(text);
      }
    }

    // La hebra duerme y deja la CPU a las demás
    chThdSleepMilliseconds(10);
  }
}

void consoleStart()
{
  chThdCreateStatic(waConsole, sizeof(waConsole),
                    NORMALPRIO + 1, consoleThread, NULL);
}
