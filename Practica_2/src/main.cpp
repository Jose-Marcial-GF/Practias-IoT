#include <math.h>
#include "config.h"
#include "report.h"
#include "threads.h"
#include "tuner.h"

#define OVERLOAD_CYCLES   3
#define DOUBLE_PIN        4
#define HISTERESIS_MS       500

#define RANDOM_WORK_PERIOD 10
#define RANDOM_LOAD_PCT_MAX   100

static volatile int peekWorker=0;
static volatile int overload_pct=0;

static int iterationsFor(int workerId, int load)
{
  if (workerId != peekWorker) return load;
  peekWorker = -1;
  return load + load * overload_pct / 100;
}

void changeLoad(int cycleNumber){
  digitalWrite(LED_BUILTIN, HIGH);
  if(cycleNumber % RANDOM_WORK_PERIOD != 0 || tuner.phase != PHASE_OPTIMAL) return;
  peekWorker = random(FIRST_WORKER, FIRST_WORKER + NUM_WORKERS);
  overload_pct = random(0, RANDOM_LOAD_PCT_MAX + 1);
  digitalWrite(LED_BUILTIN, LOW);

}

static volatile bool useDouble = false;

static void onDoublePin()
{
  static uint32_t lastMs = 0;
  uint32_t now = millis();
  if (now - lastMs < HISTERESIS_MS) return;
  lastMs = now;

  useDouble = !useDouble;
}

void appWork(int workerId, int load)
{
  int iterations = iterationsFor(workerId, load);

  if (useDouble) {
    double num = 10;
    for (int iter = 0; iter < iterations; iter++) num = exp(num) / (1 + exp(num));
    sink = (float)num;
  } else {
    float num = 10;
    for (int iter = 0; iter < iterations; iter++) num = expf(num) / (1 + expf(num));
    sink = num;
  }
}

static int badStreak = 0;




bool appVerdict(bool systemOk)
{
  static bool lastDouble = false;
  bool nowDouble = useDouble;
  if (nowDouble != lastDouble) {
    lastDouble = nowDouble;
    rprintf("\n*** Interrupcion del pin %d: los workers pasan a %s. Se repite el ajuste ***\n",
            DOUBLE_PIN, nowDouble ? "DOUBLE" : "FLOAT");
    tunerInit(OVERLOAD_CYCLES);
    badStreak = 0;
    return true;
  }

  badStreak = systemOk ? 0 : min(badStreak + 1, OVERLOAD_CYCLES);
  bool overload = (badStreak >= OVERLOAD_CYCLES);

  rprintf("  Ciclos malos seguidos: %d (sobrecarga a partir de %d)%s\n",
          badStreak, OVERLOAD_CYCLES, overload ? " -> SOBRECARGA" : "");

  return !overload;
}

void chSetup()
{
  configCheck();

  tunerInit(OVERLOAD_CYCLES);

  topStart();
  workersStart();
  idleStart();
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(DOUBLE_PIN, INPUT_PULLUP);
  SerialUSB.begin(9600);
  while (!SerialUSB) {;}

  attachInterrupt(DOUBLE_PIN, onDoublePin, FALLING);

  chBegin(chSetup);
}
