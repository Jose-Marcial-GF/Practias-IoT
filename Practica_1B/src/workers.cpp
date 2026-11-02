#include "config.h"
#include "threads.h"

static THD_WORKING_AREA(waWorker1, 2048);
static THD_WORKING_AREA(waWorker2, 2048);
static THD_WORKING_AREA(waWorker3, 2048);
static THD_WORKING_AREA(waWorker4, 2048);

static THD_FUNCTION(worker, arg)
{
  int worker_ID = (int)arg;
  sysinterval_t period_i = TIME_MS2I(threadPeriod_ms[worker_ID]);
  systime_t deadline_i = chVTGetSystemTimeX();

  while (true) {
    deadline_i += period_i;
    appWork(worker_ID, threadLoad[worker_ID]);

    systime_t endTime_i = chVTGetSystemTimeX();

    if (deadline_i > endTime_i) {
      chThdSleepUntil(deadline_i);
    }
    else if (endTime_i > deadline_i) {
      threadLateJobs[worker_ID] = threadLateJobs[worker_ID] + 1;
      deadline_i = endTime_i;
    }
  }
}

static THD_FUNCTION(extraWorker, arg)
{
  int worker_ID = (int)arg;
  sysinterval_t period_i = TIME_MS2I(threadPeriod_ms[worker_ID]);
  while (true)
  {
    
    while (!extraworker) chThdSleepMilliseconds(100);
    systime_t deadline_i = chVTGetSystemTimeX();

    while (extraworker) {
      deadline_i += period_i;
      appWork(worker_ID, threadLoad[worker_ID]);
  
      systime_t endTime_i = chVTGetSystemTimeX();
  
      if (deadline_i > endTime_i) {
        chThdSleepUntil(deadline_i);
      }
      else if (endTime_i > deadline_i) {
        threadLateJobs[worker_ID] = threadLateJobs[worker_ID] + 1;
        deadline_i = endTime_i;
      }
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

  sysLoad.threadLoad[4].thd = chThdCreateStatic(waWorker4, sizeof(waWorker4),
                                                NORMALPRIO + 2, extraWorker, (void *)4);
}
