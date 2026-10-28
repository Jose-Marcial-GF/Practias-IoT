#ifndef TUNER_H
#define TUNER_H

#include "config.h"

typedef enum { PHASE_STABILIZE = 0, PHASE_TUNING, PHASE_OPTIMAL, PHASE_FAILED } phase_t;

typedef struct {
  phase_t phase;
  int cyclesToSet;
  int pendingCiclesToSet; 
  int trialTid; 
  int prevLoad; 
  int nextTid; 
  int step[NUM_THREADS];
  bool done[NUM_THREADS];
} tuner_t;

extern tuner_t tuner;
extern const char * phase_name[];

void tunerInit(int settleCycles);

void tunerUpdate(bool systemOk);

#endif
