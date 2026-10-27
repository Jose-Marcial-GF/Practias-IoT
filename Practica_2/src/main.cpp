/* ======================================================================= *\
 *  Práctica 1.2 - Parte opcional (ChibiOS / Arduino MKR 1310)
 *  Carga variable (aleatoria) y tolerancia a picos
 *
 *  Es una alternativa a main.cpp: usa los mismos módulos (config, report,
 *  tuner, top, workers, idle) y solo cambia lo que define la aplicación.
 *  Para quedarse con ella basta con borrar main.cpp y renombrar este fichero.
 *
 *  Qué añade respecto a main.cpp:
 *    1. Carga aleatoria: en cada vuelta, un worker hace entre +0 % y
 *       +RANDOM_LOAD_PCT % de iteraciones más que la intensidad fijada por top.
 *    2. Tolerancia a picos: un ciclo aislado por encima del umbral es un pico y
 *       se absorbe con el margen de CPU libre. Solo se considera sobrecarga
 *       permanente cuando hay OVERLOAD_CYCLES ciclos malos seguidos; entonces
 *       el tuner reduce la carga.
 *    3. Interrupción en el pin DOUBLE_PIN: cada interrupción alterna los
 *       workers entre float y double (mucho más pesado). Como el coste del
 *       trabajo cambia, el óptimo anterior deja de valer y el tuner repite la
 *       búsqueda desde las cargas mínimas.
 *       El pin tiene pull-up interno: se activa conectándolo a GND. Las
 *       interrupciones más juntas que DEBOUNCE_MS se ignoran (rebotes).
 *
 *  Requiere la misma configuración de ChibiOS que main.cpp.
 *
 *  Asignatura (GII-IoT)
\* ======================================================================= */
#include <math.h>
#include "config.h"
#include "report.h"
#include "threads.h"
#include "tuner.h"

//------------------------------------------------------------------------------
// Parámetros del extra
//------------------------------------------------------------------------------
#define RANDOM_LOAD_PCT   25    // Cada vuelta suma entre 0 y +25 % de iteraciones
#define OVERLOAD_CYCLES   3     // Ciclos malos seguidos => sobrecarga permanente
#define DOUBLE_PIN        4     // Su interrupción alterna entre float y double
#define DEBOUNCE_MS       200   // Interrupciones más juntas que esto se ignoran (rebotes)

//------------------------------------------------------------------------------
// 1. Carga aleatoria
//------------------------------------------------------------------------------

// Cada worker tiene su propio estado (rngState[workerId]), así que dos hebras
// nunca comparten datos y el generador es reentrante (rand() usa un estado global)
static uint32_t rngState[NUM_THREADS];

// Generador xorshift32: devuelve un número pseudoaleatorio y actualiza el estado
static uint32_t nextRandom(uint32_t * state)
{
  uint32_t x = *state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return *state = x;
}

// Iteraciones que hará el worker en esta vuelta: la intensidad fijada por top
// más un extra aleatorio de entre 0 y RANDOM_LOAD_PCT por ciento
static int iterationsFor(int workerId, int load)
{
  int extraPct = nextRandom(&rngState[workerId]) % (RANDOM_LOAD_PCT + 1);
  return load + load * extraPct / 100;
}

//------------------------------------------------------------------------------
// 2. Interrupción del pin DOUBLE_PIN: alterna entre float y double
//------------------------------------------------------------------------------
static volatile bool useDouble = false;

// Cada interrupción invierte la bandera (en una interrupción no se debe hacer
// nada más). Se ignoran las que llegan a menos de DEBOUNCE_MS de la anterior:
// al tocar el pin con un cable el contacto rebota y genera varias seguidas
static void onDoublePin()
{
  static uint32_t lastMs = 0;
  uint32_t now = millis();
  if (now - lastMs < DEBOUNCE_MS) return;
  lastMs = now;

  useDouble = !useDouble;
}

// Trabajo de una vuelta: la intensidad fijada más el extra aleatorio, en float
// o en double según la bandera
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

//------------------------------------------------------------------------------
// 3. Veredicto tolerante a picos
//------------------------------------------------------------------------------

// Ciclos malos seguidos hasta ahora (un ciclo bueno la pone a 0)
static int badStreak = 0;

// Recibe el veredicto del ciclo y devuelve el que se pasa al tuner. Un ciclo
// malo aislado es un pico y se tolera: solo hay sobrecarga permanente cuando
// hay OVERLOAD_CYCLES ciclos malos seguidos
bool appVerdict(bool systemOk)
{
  // Si una interrupción ha cambiado entre float y double, el óptimo anterior ya
  // no vale: las cargas vuelven al mínimo y el tuner repite la búsqueda
  static bool lastDouble = false;
  bool nowDouble = useDouble;
  if (nowDouble != lastDouble) {
    lastDouble = nowDouble;
    rprintf("\n*** Interrupcion del pin %d: los workers pasan a %s. Se repite el ajuste ***\n",
            DOUBLE_PIN, nowDouble ? "DOUBLE" : "FLOAT");
    tunerInit(OVERLOAD_CYCLES);
    badStreak = 0;
    return true;            // este ciclo mezcla los dos cálculos: no cuenta
  }

  badStreak = systemOk ? 0 : min(badStreak + 1, OVERLOAD_CYCLES);
  bool overload = (badStreak >= OVERLOAD_CYCLES);

  rprintf("  Ciclos malos seguidos: %d (sobrecarga a partir de %d)%s\n",
          badStreak, OVERLOAD_CYCLES, overload ? " -> SOBRECARGA" : "");

  return !overload;
}

//------------------------------------------------------------------------------
// Arranque
//------------------------------------------------------------------------------
void chSetup()
{
  configCheck();

  // Semilla distinta para cada worker (nunca 0: el generador se quedaría en 0)
  for (int id = 0; id < NUM_THREADS; id++) rngState[id] = 0x9E3779B9u * (id + 1);

  // El tuner descarta tantos ciclos como OVERLOAD_CYCLES tras cada cambio de
  // carga: cuando decide, la racha de ciclos malos solo contiene ciclos
  // posteriores al cambio
  tunerInit(OVERLOAD_CYCLES);

  topStart();
  workersStart();
  idleStart();
}

void setup()
{
  startSerial();

  pinMode(DOUBLE_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(DOUBLE_PIN), onDoublePin, FALLING);

  // chBegin() inicia ChibiOS, llama a chSetup() y nunca retorna.
  // loop() se invoca directamente desde chBegin() (ver idle.cpp)
  chBegin(chSetup);

  while (true) { ; }
}
