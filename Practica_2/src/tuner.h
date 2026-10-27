//------------------------------------------------------------------------------
// tuner.h - Controlador de carga
//
// Cada ciclo de top recibe un veredicto (¿el sistema va bien?) y decide qué
// intensidad (threadLoad) tiene que tener cada worker.
//------------------------------------------------------------------------------
#ifndef TUNER_H
#define TUNER_H

#include "config.h"

typedef enum { PHASE_STABILIZE = 0, PHASE_TUNING, PHASE_OPTIMAL, PHASE_FAILED } phase_t;

typedef struct {
  phase_t phase;
  int settleCycles;              // ciclos que se descartan tras cada cambio de carga
  int settle;                    // ciclos pendientes de descartar
  int trialTid;                  // hebra cuya subida se está probando (-1: ninguna)
  int prevLoad;                  // valor de esa hebra antes de la prueba
  int nextTid;                   // siguiente candidato (turno rotatorio)
  int step[NUM_THREADS];         // paso actual de cada hebra
  bool done[NUM_THREADS];        // true: la hebra no puede crecer más
} tuner_t;

extern tuner_t tuner;
extern const char * phase_name[];

// Deja el controlador en su estado inicial (todas las cargas en su mínimo).
// settleCycles: ciclos que se descartan tras cada cambio de carga
void tunerInit(int settleCycles);

// Se llama una vez por ciclo con el veredicto del ciclo:
// systemOk = (CPU ocupada <= umbral) Y (ningún worker con retrasos)
void tunerUpdate(bool systemOk);

#endif  // TUNER_H
