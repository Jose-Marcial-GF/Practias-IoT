#ifndef REPORT_H
#define REPORT_H

// Informe por el puerto serie: se compone en un buffer interno con rprintf() y
// se envía de una sola vez con reportFlush().

// Añade texto al informe (igual que printf)
void rprintf(const char * fmt, ...);

// Envía el informe por SerialUSB y lo vacía
void reportFlush();

// Para imprimir un valor en por mil (1000 = 100,0 %) como porcentaje con una
// décima usando el formato "%d.%d". p.ej. 797 -> 79 y 7 -> "79.7"
#define P1(pm)  (pm) / 10, (pm) % 10

#endif  // REPORT_H
