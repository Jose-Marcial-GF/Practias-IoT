#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <ChRt.h>
#include "../common/protocol.h"

// Longitud máxima de una línea escrita en el monitor serie
#define CONSOLE_LINE_MAX  64

// Cerrojo del monitor serie: lo usan la hebra consola y la hebra receptor
extern mutex_t usbMutex;

#endif
