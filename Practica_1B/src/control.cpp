#include "config.h"
#include "control.h"
#include <Arduino.h>


#define KP 2
#define OVERLOAD_CYCLES 4
#define ABSOLUTE_ERROR 10
#define KP(error)  (2 * error)/10



static void loadAdjusting(int error){
    for (int tid = FIRST_WORKER; tid < NUM_WORKERS + FIRST_WORKER; tid++)
    {
        threadLoad[tid] = min(threadLoadMax[tid], max(threadLoadMin[tid], threadLoad[tid] + KP(error)));
    }
}

void controlLoad(bool periodOk){

    if (periodOk) {
        badCycles = 0;
    } else {
        badCycles++;
        if (badCycles < OVERLOAD_CYCLES) return;
    }

    int error = REFERENCE_CPU - sysLoad.busy_pm;
    if ( abs(error) >= ABSOLUTE_ERROR){
        loadAdjusting(error);
    }
    
}

