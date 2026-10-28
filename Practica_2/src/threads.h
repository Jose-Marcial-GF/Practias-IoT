//------------------------------------------------------------------------------
// threads.h - Las hebras del sistema y lo que la aplicación debe definir
//------------------------------------------------------------------------------
#ifndef THREADS_H
#define THREADS_H

#include "config.h"

void topStart();
void workersStart();
void idleStart();

void changeLoad(int cycleNumber);

void appWork(int workerId, int load);

bool appVerdict(bool systemOk);

#endif
