//------------------------------------------------------------------------------
// idle.cpp - Hebra idle
//
// loop() es la hebra idle (CH_CFG_NO_IDLE_THREAD = TRUE): solo se ejecuta cuando
// ninguna otra hebra quiere CPU, así que sus ticks son el tiempo libre.
//------------------------------------------------------------------------------
#include "config.h"
#include "threads.h"

// Se llama desde chSetup(), que se ejecuta en la propia hebra idle
void idleStart()
{
  sysLoad.threadLoad[IDLE_TID].thd = chThdGetSelfX();
}

void loop() {}
