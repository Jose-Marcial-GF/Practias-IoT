//------------------------------------------------------------------------------
// tuner.cpp - Controlador de carga
//
// STABILIZE -> TUNING -> OPTIMAL (o FAILED). Ver main.cpp para la descripción.
//------------------------------------------------------------------------------
#include "tuner.h"
#include "report.h"

tuner_t tuner;
const char * phase_name[] = { "STABILIZE", "TUNING", "OPTIMAL", "FAILED" };

void tunerInit(int settleCycles)
{
  tuner.phase = PHASE_STABILIZE; 
  tuner.cyclesToSet = settleCycles;
  tuner.pendingCiclesToSet = settleCycles;
  tuner.trialTid = -1;
  tuner.prevLoad = 0;
  tuner.nextTid = FIRST_WORKER;

  for (int tid = FIRST_WORKER; tid < FIRST_WORKER + NUM_WORKERS; tid++) {
    threadLoad[tid] = threadLoadMin[tid];
    tuner.step[tid] = max((threadLoadMax[tid] - threadLoadMin[tid]) / 2, MIN_STEP);
    tuner.done[tid] = false;
  }
}

// Sube la intensidad de la siguiente hebra que pueda crecer. Si ninguna puede,
// se alcanza el estado OPTIMAL.
static void startNextTrial()
{
  for (int n = 0; n < NUM_WORKERS; n++) {
    int tid = FIRST_WORKER + ((tuner.nextTid - FIRST_WORKER + n) % NUM_WORKERS);
    if (tuner.done[tid]) continue;

    int newLoad = min(threadLoad[tid] + tuner.step[tid], threadLoadMax[tid]);
    if (newLoad == threadLoad[tid]) { tuner.done[tid] = true; continue; }

    rprintf("  -> Prueba: %s load %d -> %d (paso %d)\n",
            thread_name[tid], (int)threadLoad[tid], newLoad, tuner.step[tid]);

    tuner.prevLoad = threadLoad[tid];
    threadLoad[tid] = newLoad;
    tuner.trialTid = tid;
    tuner.nextTid = FIRST_WORKER + ((tid - FIRST_WORKER + 1) % NUM_WORKERS);
    tuner.pendingCiclesToSet = tuner.cyclesToSet;
    return;
  }

  // Ninguna hebra puede crecer más: configuración óptima
  tuner.phase = PHASE_OPTIMAL;
  tuner.trialTid = -1;
  tuner.pendingCiclesToSet = tuner.cyclesToSet;
  rprintf("  -> CONVERGENCIA: ninguna hebra puede aumentar mas su carga\n");
}

// Sobrecarga en estado OPTIMAL: se reduce la hebra que más CPU consume
static void tunerReduce()
{
  int worst = -1;
  int worstCpu = -1;
  for (int tid = FIRST_WORKER; tid < FIRST_WORKER + NUM_WORKERS; tid++) {
    int cpu = sysLoad.threadLoad[tid].loadPerCycle_pm;
    if (threadLoad[tid] > threadLoadMin[tid] && cpu > worstCpu) {
      worst = tid;
      worstCpu = cpu;
    }
  }

  if (worst < 0) {
    tuner.phase = PHASE_FAILED;
    rprintf("  -> FALLO: sobrecarga con todas las hebras en su carga minima\n");
    return;
  }

  int dec = max((threadLoad[worst] - threadLoadMin[worst]) / REDUCE_DIV, MIN_STEP);
  int newLoad = max(threadLoad[worst] - dec, threadLoadMin[worst]);

  rprintf("  -> SOBRECARGA: se reduce %s load %d -> %d\n",
          thread_name[worst], (int)threadLoad[worst], newLoad);

  threadLoad[worst] = newLoad;
  tuner.pendingCiclesToSet = tuner.cyclesToSet;
}

void tunerUpdate(bool systemOk)
{
  // Tras cambiar la carga, se descartan unos ciclos
  if (tuner.pendingCiclesToSet > 0) {
    tuner.pendingCiclesToSet--;
    return;
  }

  switch (tuner.phase) {

    case PHASE_STABILIZE:
      if (systemOk) {
        rprintf("  -> Requisitos cumplidos con la carga minima: comienza el ajuste\n");
        tuner.phase = PHASE_TUNING;
        startNextTrial();
      } else {
        tuner.phase = PHASE_FAILED;
        rprintf("  -> FALLO: no se cumplen los requisitos ni con la carga minima.\n"
                "     Reduzca threadLoadMin o revise los periodos.\n");
      }
      break;

    case PHASE_TUNING:
      if (tuner.trialTid >= 0) {
        int t = tuner.trialTid;
        if (systemOk) {
          rprintf("  -> Prueba aceptada\n");
        } else {
          rprintf("  -> Prueba rechazada: %s vuelve a load %d\n",thread_name[t], tuner.prevLoad);
          threadLoad[t] = tuner.prevLoad;
        }
        tuner.step[t] /= 2;
        if (tuner.step[t] < MIN_STEP || threadLoad[t] >= threadLoadMax[t]) tuner.done[t] = true;
      }
      startNextTrial();
      break;

    case PHASE_OPTIMAL:
      if (!systemOk) tunerReduce();
      break;

    case PHASE_FAILED:
    default:
      break;
  }
}
