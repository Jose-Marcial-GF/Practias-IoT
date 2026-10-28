//------------------------------------------------------------------------------
// threads.h - Las hebras del sistema y lo que la aplicación debe definir
//------------------------------------------------------------------------------
#ifndef THREADS_H
#define THREADS_H

#include "config.h"

// Creación de las hebras. Cada una registra su puntero en sysLoad para que top
// pueda medir su CPU. Se llaman desde chSetup(), en este orden.
void topStart();       // top.cpp:     hebra top (NORMALPRIO + 2)
void workersStart();   // workers.cpp: tres hebras worker (NORMALPRIO + 1)
void idleStart();      // idle.cpp:    loop() como hebra idle (IDLEPRIO)

void changeLoad(int cycleNumber);

// Funciones que define la aplicación (main.cpp o extra.cpp) y usan los módulos:

// Trabajo de una vuelta de un worker. La llama workers.cpp con la intensidad
// que ha fijado top (load). Solo debe gastar CPU.
void appWork(int workerId, int load);

// Veredicto que recibe el tuner en cada ciclo. La llama top.cpp con el
// veredicto del propio ciclo (CPU <= umbral y sin retrasos).
bool appVerdict(bool systemOk);

#endif  // THREADS_H
