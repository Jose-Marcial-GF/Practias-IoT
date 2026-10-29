#include <stdio.h>
#include "config.h"
#include "threads.h"
#include "control.h"

#define P1(pm)  (pm) / 10, (pm) % 10

static THD_WORKING_AREA(waTop, 4096);

static void CPUstats()
{
  uint32_t accumTicks = 0;
  for (int tid = 0; tid < NUM_THREADS; tid++) {
    threadLoad_t& thdLoad = sysLoad.threadLoad[tid];
    systime_t ticks = chThdGetTicksX(thdLoad.thd);
    thdLoad.ticksPerCycle = ticks - thdLoad.ticksTotal;
    thdLoad.ticksTotal = ticks;
    accumTicks += thdLoad.ticksPerCycle;
  }

  for (int tid = 0; tid < NUM_THREADS; tid++) {
    threadLoad_t& thdLoad = sysLoad.threadLoad[tid];
    thdLoad.loadPerCycle_pm =thdLoad.ticksPerCycle;
  }
  sysLoad.busy_pm = accumTicks - sysLoad.threadLoad[IDLE_TID].loadPerCycle_pm;
}

static bool checkLateJobs(uint32_t late[])
{
  static uint32_t lastLateJobs[NUM_THREADS];
  bool periodsOk = true;

  for (int tid = FIRST_WORKER; tid < FIRST_WORKER + NUM_WORKERS; tid++) {
    uint32_t total = threadLateJobs[tid];
    late[tid] = total - lastLateJobs[tid];
    lastLateJobs[tid] = total;
    if (late[tid]) periodsOk = false;
  }
  return periodsOk;
}



void reportStatus(int cycleNumber, uint32_t * late, bool periodOk){

  char report[160];

  sprintf(report, "cycle: %d\n", cycleNumber);
  SerialUSB.println(report);

  for (int8_t tid= FIRST_WORKER; tid < FIRST_WORKER+ NUM_WORKERS; tid++)
  {
    sprintf(report, "%-9s load:%5d  CPU:%3d.%d%% (%3lu ms)  late:%lu\n",
      thread_name[tid], (int)threadLoad[tid],
      P1(sysLoad.threadLoad[tid].loadPerCycle_pm),
      (unsigned long)TIME_I2MS(sysLoad.threadLoad[tid].ticksPerCycle),
      (unsigned long)late[tid]);
      SerialUSB.print(report);
    }
  sprintf(report, "CPU busy: %d.%d%%\nbadCycles: %d\nPeriod ok: %s\n", P1(sysLoad.busy_pm), badCycles,
          periodOk ? "OK" : "FAIL");
  SerialUSB.print(report);
}

static void topCycle()
{
  static uint32_t cycleNumber = 0;
  cycleNumber++;

  CPUstats();

  uint32_t late[NUM_THREADS] = { 0, 0, 0, 0, 0, 0};
  bool jobsOk = checkLateJobs(late);
  bool periodOk = (sysLoad.busy_pm <= REFERENCE_CPU) && jobsOk;

  reportStatus(cycleNumber, late, periodOk);

  controlLoad(periodOk);

  changeLoad(cycleNumber);
  
}

static THD_FUNCTION(top, arg)
{
  (void)arg;

  systime_t lastTime_i = 0;
  systime_t period_i = TIME_MS2I(CYCLE_MS);

  while (true) {
  
    systime_t deadline_i = lastTime_i + period_i;
    systime_t now_i = chVTGetSystemTimeX();
    if (deadline_i > now_i) {
      chThdSleepUntil(deadline_i);
    }
    lastTime_i = chVTGetSystemTimeX();

    topCycle();
  }
}

void topStart()
{
  sysLoad.threadLoad[0].thd = chThdCreateStatic(waTop, sizeof(waTop),
                                                NORMALPRIO + 3, top, NULL);
}
