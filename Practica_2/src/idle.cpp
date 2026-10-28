#include "config.h"
#include "threads.h"

void idleStart()
{
  sysLoad.threadLoad[IDLE_TID].thd = chThdGetSelfX();
}

void loop() {}
