// Planta v2: simula milisegundo a milisegundo las vueltas y los plazos de los workers
// (reparto round-robin con quantum de CH_CFG_TIME_QUANTUM ticks entre los listos).
// Replica a mano la logica de workers.cpp. Definir antes: ITER(w), IS_DOUBLE, RESYNC.
struct SimWorker { long deadline; long wakeAt; double remaining; bool running; };
static SimWorker g_sw[NUM_THREADS];
static bool g_swInit = false;
static int  g_current = -1;
static int  g_quantumLeft = 0;
static long g_now = 0;                       // ms desde el arranque

static double lapWork(int w, double cost, double dblFactor, double scale)
{
  return ITER(w) * cost * scale * (IS_DOUBLE ? dblFactor : 1.0);
}

static void plant2Cycle(double cost, double dblFactor, double scale)
{
  const int QUANTUM = CH_CFG_TIME_QUANTUM;
  if (!g_swInit) {
    g_swInit = true;
    for (int w = FIRST_WORKER; w < FIRST_WORKER + NUM_WORKERS; w++) {
      g_sw[w].deadline = 0; g_sw[w].wakeAt = 0; g_sw[w].running = true;
      g_sw[w].remaining = lapWork(w, cost, dblFactor, scale);
    }
  }
  g_ticks[0] += 3;                           // top trabaja ~3 ms al inicio del ciclo
  for (int ms = 0; ms < 1000; ms++, g_now++) {
    // despertar a los que toca
    for (int w = FIRST_WORKER; w < FIRST_WORKER + NUM_WORKERS; w++)
      if (!g_sw[w].running && g_now >= g_sw[w].wakeAt) {
        g_sw[w].running = true; g_sw[w].remaining = lapWork(w, cost, dblFactor, scale);
      }
    if (ms < 3) { continue; }                // esos 3 ms los usa top
    // elegir quien corre (round-robin con quantum)
    bool curReady = (g_current >= 0 && g_sw[g_current].running && g_sw[g_current].remaining > 0);
    if (!curReady || g_quantumLeft <= 0) {
      int start = g_current < 0 ? FIRST_WORKER : g_current, pick = -1;
      for (int k = 1; k <= NUM_WORKERS; k++) {
        int w = FIRST_WORKER + ((start - FIRST_WORKER + k) % NUM_WORKERS);
        if (g_sw[w].running && g_sw[w].remaining > 0) { pick = w; break; }
      }
      if (pick < 0 && curReady) pick = g_current;
      g_current = pick; g_quantumLeft = QUANTUM;
    }
    if (g_current < 0) { g_ticks[IDLE_TID]++; continue; }
    int w = g_current;
    g_ticks[w]++; g_quantumLeft--; g_sw[w].remaining -= 1.0;
    if (g_sw[w].remaining <= 0) {            // termina la vuelta
      long end = g_now + 1;
      g_sw[w].deadline += threadPeriod_ms[w];
      if (g_sw[w].deadline > end) {          // adelantado: duerme hasta el plazo
        g_sw[w].running = false; g_sw[w].wakeAt = g_sw[w].deadline;
      } else {                               // no duerme: empieza otra vuelta ya
        if (end - g_sw[w].deadline > LATE_TOLERANCE_MS) {
          threadLateJobs[w]++;
          if (RESYNC) g_sw[w].deadline = end;   // la correccion: olvidar el retraso acumulado
        }
        g_sw[w].remaining = lapWork(w, cost, dblFactor, scale);
      }
    }
  }
}
