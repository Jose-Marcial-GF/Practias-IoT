/* ----------------------------------------------------------------------------
 *  Práctica 2 - Supervisor - Arduino MKR WAN 1310 + ChibiOS
 *
 *  1. Lee los comandos que escribe el usuario en el monitor serie
 *  2. Los convierte en mensajes binarios y los envía por Serial1 a traves del uart
 *     al dispositivo sensor.
 *  3. Espera la respuesta de cada comando como mucho RESP_TIMEOUT_MS (sin
 *     reintento) y avisa si no llega.
 *  4. Muestra todo lo que envía el sensor: respuestas y medidas periódicas,
 *     sin estropear la línea que el usuario esté escribiendo.
 *  5. Hebras:
 *     - consola  (NORMALPRIO + 1): lee el teclado, interpreta la línea, envía
 *                                  el comando y espera su respuesta
 *     - receptor (NORMALPRIO + 2): lee Serial1, muestra cada mensaje y avisa
 *                                  a la consola (semáforo) si es la respuesta
 *                                  que espera
 *     Las dos escriben en el monitor, por eso usan el cerrojo usbMutex.
 *
 *  Conexión de Serial1 con el sensor: TX con RX cruzados y GND común.
 *
 *  Autores:
 *  Juan Daniel López Melainine
 *  José Marcial Galván Franco
 * ----------------------------------------------------------------------------
 */

#include "config.h"

void chSetup()
{
  consoleStart();
  receiverStart();
}

void setup()
{
  SerialUSB.begin(9600);
  while (!SerialUSB) {;}

  Serial1.begin(PROTO_BAUD);

  chBegin(chSetup);
}

void loop() {}
