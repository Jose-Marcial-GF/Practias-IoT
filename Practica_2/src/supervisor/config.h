#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <ChRt.h>
#include "../common/protocol.h"

#define CONSOLE_LINE_MAX  64
void consoleStart();
void receiverStart();

#define N_SENSORS         2
#define DEVICE_SCREEN     0
#define DEVICE_BOTH       3

#define RESP_TIMEOUT_MS   500
void    expectResponse(uint8_t device, uint8_t op);
uint8_t waitResponse(uint32_t timeout_ms);

extern mutex_t usbMutex;

void outputBegin();
void outputEnd();

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

#define TXT_LIST              "lista"
#define TXT_UNKNOWN_OP        "?"
#define TXT_ERR_I2C_SENSOR1   "el sensor 1 no responde (I2C)"
#define TXT_ERR_I2C_SENSOR2   "el sensor 2 no responde (I2C)"
#define TXT_ERR_I2C_SENSORES  "los sensores no responden (I2C)"
#define TXT_ERR_I2C_PANTALLA  "la pantalla no responde (I2C)"
#define TXT_ERR_BUSY          "el dispositivo está ocupado, repite el comando"
#define TXT_ERR_UNKNOWN       "error desconocido"

#endif
