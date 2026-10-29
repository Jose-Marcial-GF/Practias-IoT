#include "config.h"

char thread_name[NUM_THREADS][15] = { "top",
                                      "worker_1", "worker_2", "worker_3", "worker_4",
                                      "idle" };

volatile uint32_t threadPeriod_ms[NUM_THREADS] = { CYCLE_MS, 200, 100, 200, 150, 0 };

const int threadLoadMin[NUM_THREADS] = { 0, 10,   10,   10,  60, 0 };
const int threadLoadMax[NUM_THREADS] = { 0, 1500, 1000, 1500, 60, 0 };

volatile int threadLoad[NUM_THREADS] = { 0, 0, 0, 0, 0, 0 };

volatile uint32_t threadLateJobs[NUM_THREADS] = { 0, 0, 0, 0, 0, 0 };

volatile float sink;

volatile bool extraworker = true;

volatile int badCycles = 1;


systemLoad_t sysLoad;


void configCheck()
{
  if (CH_CFG_TIME_QUANTUM == 0) {
    SerialUSB.println("You must set CH_CFG_TIME_QUANTUM to a non-zero value in");
    SerialUSB.println("src/samd/chconf_samd.h to enable round-robin scheduling.");
    while (true) {}
  }
  SerialUSB.print("CH_CFG_TIME_QUANTUM: ");
  SerialUSB.println(CH_CFG_TIME_QUANTUM);

  if (CH_CFG_NO_IDLE_THREAD == FALSE) {
    SerialUSB.println("You must set CH_CFG_NO_IDLE_THREAD to TRUE");
    while (true) {}
  }
}
