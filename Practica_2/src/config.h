#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <ChRt.h>

#define USE_DOUBLE        FALSE

#define CYCLE_MS          1000
#define NUM_THREADS       5
#define NUM_WORKERS       3
#define FIRST_WORKER      1
#define IDLE_TID          4

#define MAX_CPU_PM        850
#define LATE_TOLERANCE_MS 5
#define SETTLE_CYCLES     1
#define MIN_STEP          4
#define REDUCE_DIV        8

extern char thread_name[NUM_THREADS][15];

extern volatile uint32_t threadPeriod_ms[NUM_THREADS];

extern const int threadLoadMin[NUM_THREADS];
extern const int threadLoadMax[NUM_THREADS];

extern volatile int threadLoad[NUM_THREADS];

extern volatile uint32_t threadEffectivePeriod_ms[NUM_THREADS];
extern volatile uint32_t threadCycle_ms[NUM_THREADS];
extern volatile uint32_t threadLateJobs[NUM_THREADS];

extern volatile float sink;

typedef struct {
  thread_t * thd;
  sysinterval_t ticksTotal;
  sysinterval_t ticksPerCycle;
  int loadPerCycle_pm;
} threadLoad_t;

typedef struct {
  threadLoad_t threadLoad[NUM_THREADS];
  int busy_pm;
} systemLoad_t;

extern systemLoad_t sysLoad;

void configCheck();

#endif
