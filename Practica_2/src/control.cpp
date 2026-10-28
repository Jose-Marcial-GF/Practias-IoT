#include "config.h"
#include "control.h"
#include <Arduino.h>


#define KP 2


static void loadAdjusting(int error){
    for (int tid = FIRST_WORKER; tid < NUM_WORKERS + FIRST_WORKER; tid++)
    {
        threadLoad[tid] = min(threadLoadMax[tid], max(threadLoadMin[tid], threadLoad[tid] + (KP * error)/10));
    }
}

void controlLoad(){
    int error = REFERENCE_CPU - sysLoad.busy_pm;
    if ( abs(error) >= 10){
        loadAdjusting(error);
    }
    
}

