#ifndef REPORT_H
#define REPORT_H

void rprintf(const char * fmt, ...);

void reportFlush();

#define P1(pm)  (pm) / 10, (pm) % 10

#endif
