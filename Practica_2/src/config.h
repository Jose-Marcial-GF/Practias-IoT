//------------------------------------------------------------------------------
// config.h - Parámetros y datos compartidos por todos los módulos
//------------------------------------------------------------------------------
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <ChRt.h>

//------------------------------------------------------------------------------
// Parametrización
//------------------------------------------------------------------------------
#define USE_DOUBLE        FALSE   // TRUE para usar doble precisión (solo lo usa main.cpp)

#define CYCLE_MS          1000    // Ciclo de monitorización de la hebra top
#define NUM_THREADS       5       // top + 3 workers + loop() (hebra idle)
#define NUM_WORKERS       3
#define FIRST_WORKER      1       // índices: 0 = top, 1..3 = workers, 4 = idle
#define IDLE_TID          4

#define MAX_CPU_PM        850     // Umbral de ocupación de CPU (por mil: 850 = 85,0 %)
#define LATE_TOLERANCE_MS 5       // Retraso admisible al terminar una iteración
#define SETTLE_CYCLES     1       // Ciclos que se descartan tras cambiar la carga
#define MIN_STEP          4       // Resolución: por debajo, la hebra se da por cerrada
#define REDUCE_DIV        8       // En sobrecarga se reduce (load - min) / REDUCE_DIV

//------------------------------------------------------------------------------
// Configuración (se define en config.cpp)
//------------------------------------------------------------------------------
extern char thread_name[NUM_THREADS][15];

// Periodos nominales de cada hebra (ms)
extern volatile uint32_t threadPeriod_ms[NUM_THREADS];

// Rango de intensidad (nº de iteraciones del bucle de cálculo) de cada worker
extern const int threadLoadMin[NUM_THREADS];
extern const int threadLoadMax[NUM_THREADS];

//------------------------------------------------------------------------------
// Estado compartido entre hebras (se define en config.cpp)
//------------------------------------------------------------------------------
// Intensidad actual. SOLO la modifica la hebra top; los workers solo la leen
extern volatile int threadLoad[NUM_THREADS];

// Medidas hechas por los workers (top solo las lee)
extern volatile uint32_t threadEffectivePeriod_ms[NUM_THREADS];
extern volatile uint32_t threadCycle_ms[NUM_THREADS];
// Contador monótono de iteraciones terminadas con retraso (solo lo incrementa
// el worker; top calcula diferencias entre ciclos, así no hace falta bloquear)
extern volatile uint32_t threadLateJobs[NUM_THREADS];

// Evita que el compilador elimine el bucle de cálculo
extern volatile float sink;

// Estructuras para medir la carga de CPU usando los ticks de cada hebra
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

//------------------------------------------------------------------------------
// Arranque
//------------------------------------------------------------------------------
void configCheck();   // Comprueba la configuración de ChibiOS (si no es válida, se detiene)

#endif  // CONFIG_H
