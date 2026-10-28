#include "config.h"
#include "report.h"
#include "threads.h"
#include "control.h"

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

static void printReport(uint32_t cycleNumber, const uint32_t late[],
                        bool periodsOk)
{

  for (int tid = 0; tid < NUM_THREADS; tid++) {
    int cpu = sysLoad.threadLoad[tid].loadPerCycle_pm;
    if (tid >= FIRST_WORKER && tid < FIRST_WORKER + NUM_WORKERS) {
      rprintf("%-9s load:%5d [%d-%d]  CPU:%3d.%d%%  exec:%3lu ms  "
              "periodo:%4lu/%lu ms  tarde:%lu\n",
              thread_name[tid], (int)threadLoad[tid],
              threadLoadMin[tid], threadLoadMax[tid], P1(cpu),
              (unsigned long)threadCycle_ms[tid],
              (unsigned long)threadEffectivePeriod_ms[tid],
              (unsigned long)threadPeriod_ms[tid],
              (unsigned long)late[tid]);
    } else {
      rprintf("%-9s CPU:%3d.%d%%\n", thread_name[tid], P1(cpu));
    }
  }

  rprintf("CPU ocupada: %d.%d%% (umbral %d.%d%%) | periodos: %s \n",
          P1(sysLoad.busy_pm), P1(REFERENCE_CPU),
          periodsOk ? "OK" : "FALLO");
}

static void topCycle()
{
  static uint32_t cycleNumber = 0;
  cycleNumber++;

  CPUstats();

  uint32_t late[NUM_THREADS] = { 0, 0, 0, 0, 0 };
  bool periodsOk = (sysLoad.busy_pm <= REFERENCE_CPU) && checkLateJobs(late);

  printReport(cycleNumber, late, periodsOk);

  controlLoad();

  changeLoad(cycleNumber);
  
  reportFlush();
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
                                                NORMALPRIO + 2, top, NULL);
}
