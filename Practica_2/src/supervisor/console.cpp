#include <stdlib.h>
#include <string.h>
#include "config.h"
#include <stdint.h>
#include <stdio.h>

static THD_WORKING_AREA(waConsole, 2048);

// Escribe un texto en el monitor serie. Se usa el cerrojo porque la hebra
// receptor también escribe en SerialUSB y los mensajes no deben mezclarse.
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
      "  n = [1: first, 2: secound, both | 3: both ]"
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

// true si la palabra existe y es igual al texto
static bool is(const char * word, const char * text)
{
  return word != NULL && strcmp(word, text) == 0;
}

static uint8_t sendUnit(uint8_t sensor, char* word){
  if(is(word, "inc")) return sendHead(OP, sensor << SENSOR_POSITION, NO_MESSAGE, PROTO_UNIT_INC);
  if(is(word, "cm")) return sendHead(OP, sensor << SENSOR_POSITION, NO_MESSAGE, PROTO_UNIT_CM);
  if(is(word, "ms")) return sendHead(OP, sensor << SENSOR_POSITION, NO_MESSAGE, PROTO_UNIT_MS);
  return ArgumentError("unidad de medida no conocida");
}

static uint8_t sendDelay(uint8_t sensor, char* word){
  uint16_t delay = (uint16_t)strtol(word, NULL, 10);
  if (delay < MIN_DELAY_SFR || delay > MAX_DELAY_SFR) return ArgumentError("Valor no esperado");
  sendMessage(OP, sensor << SENSOR_POSITION, MESSAGE, OP_US_SET_DELAY, (uint8_t *)&delay, 1);
  return 0;
}

static uint8_t sendOn(uint8_t sensor, char* word){
  long in_delay = strtol(word, NULL, 10);
  if (in_delay < MIN_DELAY_SFR || in_delay > MAX_DELAY_SFR) return ArgumentError("Valor no esperado");
  uint16_t delay = (uint16_t)in_delay;
  sendHead(OP, sensor << SENSOR_POSITION, MESSAGE, OP_US_PERIODIC_ON);
  writeMessage((uint8_t *)&delay, 2);
  return 0;
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
    if (word[1] == NULL) return sendHead(OP, SENSORES, NO_MESSAGE, US_LIST);
    if (is(word[1], "both")) {sensor = SENSORES;}
    
    else {
      sensor = (uint8_t)strtol(word[1], NULL, 10);
      if (sensor < 1 || sensor > 3) return say("El sensor mencionado no existe");
      sensor << SENSOR_POSITION;
      }
    
    if (is(word[2], "one-shot")) return word[3] == NULL ? sendHead(OP, sensor , NO_MESSAGE, US_ONESHOT) : unExpectedArgumentError();
    
    if (is(word[2], "off")) return word[3] == NULL ? sendHead(OP, sensor , NO_MESSAGE, OP_US_PERIODIC_OFF) : unExpectedArgumentError();
    
    if (is(word[2], "status")) return word[3] == NULL ? sendHead(OP, sensor , NO_MESSAGE, OP_STATUS) : unExpectedArgumentError();
    
    if (is(word[2], "unit")) return word[3] != NULL && word[4] == NULL ? sendUnit(sensor, word[3]) : unExpectedArgumentError();
    
    if (is(word[2], "delay")) return word[3] != NULL && word[4] == NULL ? sendDelay(sensor, word[3]) : unExpectedArgumentError();
    
    if (is(word[2], "on")) return word[3] != NULL && word[4] == NULL ? sendOn(sensor, word[3]) : unExpectedArgumentError();
  } 
  if (is(word[0], "oled")){
    if (is(word[1], "off")) return word[2] == NULL ? sendHead(OP, PANTALLA, NO_MESSAGE, OP_OLED_OFF) : unExpectedArgumentError();
    if (is(word[1], "on")) return word[2] == NULL ?  sendHead(OP, PANTALLA, NO_MESSAGE, OP_OLED_ON) : unExpectedArgumentError();
  }
  return say("Comando no reconocido (help muestra los comandos)\n");
}

static THD_FUNCTION(consoleThread, arg)
{
  char line[CONSOLE_LINE_MAX];
  int n = 0;                           // caracteres escritos en la línea

  say("Supervisor listo. Escribe help para ver los comandos.\n");

  while (true) {
    while (SerialUSB.available() > 0) {
      char c = SerialUSB.read();

      if (c == '\r' || c == '\n') {              // Enter: se ejecuta la línea
        if (n > 0) {
          line[n] = '\0';
          say("\n");
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
    chThdSleepMilliseconds(10);
  }
}

void consoleStart()
{
  chThdCreateStatic(waConsole, sizeof(waConsole),
                    NORMALPRIO + 1, consoleThread, NULL);
}
