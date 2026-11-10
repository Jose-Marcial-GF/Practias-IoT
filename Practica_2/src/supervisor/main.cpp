/* ----------------------------------------------------------------------------
 *  Práctica 2 - Supervisor - Arduino MKR WAN 1310 + ChibiOS
 *
 *  El supervisor lee comandos escritos en el monitor serie, los convierte en
 *  mensajes binarios (src/common/protocol.h) y los envía por Serial1 al
 *  dispositivo sensor. Todo lo que el sensor envía (respuestas y medidas
 *  periódicas) se muestra en el monitor según llega.
 *
 *  Hebras:
 *    - consola   (NORMALPRIO + 1): lee el teclado, muestra lo que se escribe,
 *                                  interpreta la línea y envía el mensaje.
 *    - receptor  (NORMALPRIO + 2): lee Serial1 y muestra cada mensaje completo.
 *    - loop()    hebra idle, siempre vacía.
 *
 *  Las dos hebras escriben en el monitor, por eso usan el cerrojo usbMutex.
 *
 *  Conexión de Serial1 con el sensor: TX con RX cruzados y GND común.
 * ----------------------------------------------------------------------------
 */

#include "config.h"
#include "threads.h"

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

  // chBegin() nunca retorna. loop() se invoca directamente desde chBegin()
  chBegin(chSetup);
}

void loop() {}
