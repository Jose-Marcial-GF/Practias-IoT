/* ----------------------------------------------------------------------------
 *  Práctica 1.B - Arduino MKR 1310 + ChibiOS
 *
 *  1. La hebra top (máxima prioridad) mide cada CYCLE_MS la carga de CPU de
 *     cada hebra a partir de los ticks consumidos.
 *  2. Cada hebra worker tiene un rango de intensidad [min, max] que top ajusta de
 *     forma cíclica, subiendo la carga mientras se cumpla que:
 *     a) los workers respetan su periodo, y
 *     b) la CPU no supera el 85 % (>= 15 % en la hebra idle/loop).
 *     Si no se cumple, se reduce la intensidad y se reevalúa.
 *  3. EXTRAS:
 *     - Carga aleatoria: cada RANDOM_WORK_PERIOD ciclos, un worker eleva su carga
 *       hasta un RANDOM_LOAD_PCT_MAX % para comprobar que el margen de CPU
 *       absorbe los picos. Solo hay sobrecarga tras OVERLOAD_CYCLES ciclos malos
 *       seguidos (histéresis).
 *     - Interrupción externa en DOUBLE_PIN (PULL_UP, flanco de bajada): alterna
 *       los workers entre float y double y repite el ajuste de carga.
 *
 * ----------------------------------------------------------------------------
 */


#include <math.h>
#include "config.h"
#include "threads.h"


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
  if(cycleNumber % RANDOM_WORK_PERIOD != 0) return;
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

void chSetup()
{
  configCheck();

  for (int tid = FIRST_WORKER; tid < FIRST_WORKER + NUM_WORKERS; tid++) {
    threadLoad[tid] = threadLoadMin[tid];
  }

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
