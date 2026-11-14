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

#endif
