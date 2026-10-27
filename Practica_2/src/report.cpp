//------------------------------------------------------------------------------
// Envía el informe por el puerto serie
//------------------------------------------------------------------------------
#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include "report.h"

#define REPORT_SIZE 1024
static char report[REPORT_SIZE];
static size_t reportLen = 0;

void rprintf(const char * fmt, ...)
{
  if (reportLen >= REPORT_SIZE - 1) return;

  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(report + reportLen, REPORT_SIZE - reportLen, fmt, ap);
  va_end(ap);

  if (n > 0) reportLen = min(reportLen + n, (size_t)(REPORT_SIZE - 1));
}

void reportFlush()
{
  if (reportLen) SerialUSB.write((const uint8_t *)report, reportLen);
  reportLen = 0;
}
