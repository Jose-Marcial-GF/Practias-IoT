//------------------------------------------------------------------------------
// workers.cpp - Hebras de trabajo
//
// Tres hebras idénticas que se ejecutan periódicamente. En cada vuelta hacen
// el trabajo que define la aplicación (appWork) con la intensidad que les asigna
// top, y duermen hasta el comienzo del siguiente periodo.
//------------------------------------------------------------------------------
#include "config.h"
#include "threads.h"

static THD_WORKING_AREA(waWorker1, 2048);
static THD_WORKING_AREA(waWorker2, 2048);
static THD_WORKING_AREA(waWorker3, 2048);

static THD_FUNCTION(worker, arg)
{
  int worker_ID = (int)(uintptr_t)arg;
  sysinterval_t period_i = TIME_MS2I(threadPeriod_ms[worker_ID]);
  sysinterval_t tolerance_i = TIME_MS2I(LATE_TOLERANCE_MS);
  systime_t deadline_i = chVTGetSystemTimeX();
  systime_t lastBeginTime_i = deadline_i;

  while (!chThdShouldTerminateX()) {
    systime_t beginTime_i = chVTGetSystemTimeX();
    threadEffectivePeriod_ms[worker_ID] = TIME_I2MS(beginTime_i - lastBeginTime_i);
    lastBeginTime_i = beginTime_i;

    appWork(worker_ID, threadLoad[worker_ID]);    // intensidad vigente (la fija top)

    deadline_i += period_i;

    systime_t endTime_i = chVTGetSystemTimeX();
    threadCycle_ms[worker_ID] = TIME_I2MS(endTime_i - beginTime_i);

    if (deadline_i > endTime_i) {
      chThdSleepUntil(deadline_i);
    }
    else if ((endTime_i - deadline_i) > tolerance_i) {
      // No hemos terminado a tiempo: no se respeta el periodo
      threadLateJobs[worker_ID] = threadLateJobs[worker_ID] + 1;

      // Se olvida el retraso acumulado y se empieza un ritmo nuevo desde ahora.
      // Si no, la hebra encadena vueltas sin dormir para recuperar el tiempo
      // perdido: tras una sobrecarga puede tardar decenas de ciclos, y mientras
      // tanto la CPU marca 100 % y el tuner rechaza todas las pruebas
      deadline_i = endTime_i;
    }
  }
}

void workersStart()
{
  sysLoad.threadLoad[1].thd = chThdCreateStatic(waWorker1, sizeof(waWorker1),
                                                NORMALPRIO + 1, worker, (void *)1);

  sysLoad.threadLoad[2].thd = chThdCreateStatic(waWorker2, sizeof(waWorker2),
                                                NORMALPRIO + 1, worker, (void *)2);

  sysLoad.threadLoad[3].thd = chThdCreateStatic(waWorker3, sizeof(waWorker3),
                                                NORMALPRIO + 1, worker, (void *)3);
}
