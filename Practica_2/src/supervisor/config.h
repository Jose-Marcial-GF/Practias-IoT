#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <ChRt.h>
#include "../common/protocol.h"

// Longitud máxima de una línea escrita en el monitor serie
#define CONSOLE_LINE_MAX  64
void consoleStart();    // hebra consola: lee los comandos y los envía al sensor
void receiverStart();   // hebra receptor: muestra lo que llega del sensor

// Cerrojo del monitor serie: lo usan la hebra consola y la hebra receptor
extern mutex_t usbMutex;

// ----- Textos del supervisor ---------------------------------------------
// Palabras de los comandos. Se escriben aquí una sola vez: receiver.cpp las
// usa para decir a qué comando corresponde una respuesta y console.cpp las
// puede usar para reconocer lo que escribe el usuario
#define WORD_ONESHOT  "one-shot"
#define WORD_ON       "on"
#define WORD_OFF      "off"
#define WORD_STATUS   "status"
#define WORD_DELAY    "delay"
#define WORD_UNIT     "unit"
#define WORD_INC      "inc"
#define WORD_CM       "cm"
#define WORD_MS       "ms"
#define WORD_OLED     "oled"

// Textos que solo se muestran
#define TXT_LIST              "lista"
#define TXT_UNKNOWN_OP        "?"
#define TXT_ERR_I2C_SENSOR1   "el sensor 1 no responde (I2C)"
#define TXT_ERR_I2C_SENSOR2   "el sensor 2 no responde (I2C)"
#define TXT_ERR_I2C_SENSORES  "los sensores no responden (I2C)"
#define TXT_ERR_I2C_PANTALLA  "la pantalla no responde (I2C)"
#define TXT_ERR_BUSY          "el dispositivo está ocupado, repite el comando"
#define TXT_ERR_UNKNOWN       "error desconocido"

#endif
