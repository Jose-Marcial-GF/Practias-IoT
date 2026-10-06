/* ----------------------------------------------------------------------------
 *  Práctica 1.A - Ejercicio propuesto (GII-IoT) - Arduino MKR 1310
 *
 *  1. Ajusta el RTC con la fecha y hora de compilación.
 *  2. Alarma periódica del RTC cada 10 s.
 *  3. La cadena se guarda en un fichero sobre la FLASH.
 *  4. El micro se pone en modo descanso y lo despierta el RTC o las interrupciónes externas.
 *  5. EXTRAS: interrupción externas con resistencias de PULL_UP
 *     - WRITE_PIN añade una línea al fichero que se debe a una interrupción externa
 *     - DUMP_PIN  vuelca el fichero por SerialUSB
 *
 * ----------------------------------------------------------------------------
 */

#include <time.h>
#include <RTCZero.h>
#include <Arduino_MKRMEM.h>
#include <ArduinoLowPower.h>

RTCZero rtc;

Arduino_W25Q16DV flash(SPI1, FLASH_CS);

const char filename[] = "fechas.txt";
const char external_msg[] = "linea anadida por interrupcion externa (pin 4)";

// Parámetros
#define PERIOD_SEC            10      // periodo de la alarma
#define OFFSET_SEC            2       // delay de la primera alarma
#define WRITE_PIN             4       // escribe línea de la fecha por interrupción externa 
#define DUMP_PIN              5       // volcado del fichero por SerialUSB
#define INIBITIONTIME_MS      400     // histéresis


// Variables las ISR
volatile uint32_t _period_sec   = 0;
volatile bool     _rtcFlag      = false;
volatile bool     _externalFlag = false;
volatile bool     _dumpFlag     = false;


// Funciones necesarias
bool setDateTime(const char * date_str, const char * time_str);
const char* getDateTime();
void setPeriodicAlarm(uint32_t period_sec, uint32_t offsetFromNow_sec);
void alarmCallback();
void writeExternalISR();
void dumpISR();
bool prepareFile();
void writeLine(const char *message);
void dumpFile();
void on_exit_with_error_do();




void setup()
{
  // Apagamos el módulo LoRa
  pinMode(LORA_RESET, OUTPUT);
  digitalWrite(LORA_RESET, LOW);

  // LED encendido mientras se ejecuta el setup
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  SerialUSB.begin(9600);
  while (!SerialUSB) { ; }

  SerialUSB.print("Starting at: ");
  SerialUSB.print(__DATE__);
  SerialUSB.print(" ");
  SerialUSB.println(__TIME__);
  
  rtc.begin();
  LowPower.attachInterruptWakeup(RTC_ALARM_WAKEUP, alarmCallback, CHANGE);
  
  if (!setDateTime(__DATE__, __TIME__)) {
    SerialUSB.println("setDateTime() failed!\nExiting ...");
    while (1) { ; }
  }
  
  flash.begin();
  if (!prepareFile()) {
    while (1) { ; }
  }
  
  setPeriodicAlarm(PERIOD_SEC, OFFSET_SEC);
  _rtcFlag = false;
  
  pinMode(WRITE_PIN, INPUT_PULLUP);
  LowPower.attachInterruptWakeup(WRITE_PIN, writeExternalISR, FALLING);
  
  pinMode(DUMP_PIN, INPUT_PULLUP);
  LowPower.attachInterruptWakeup(DUMP_PIN, dumpISR, FALLING);
  
  SerialUSB.println("started ");
  //Apagamos la luz cuando se termina de preparar
  digitalWrite(LED_BUILTIN, LOW);
}

void loop()
{

  if (_rtcFlag) {
    _rtcFlag = false;
    char message[64];
    snprintf(message, sizeof(message), "%s\n", getDateTime());
    writeLine(message);
    digitalWrite(LED_BUILTIN, LOW);

  }

  if (_externalFlag) {
    char message[96];
    snprintf(message, sizeof(message), "%s - %s\n", getDateTime(), external_msg);
    writeLine(message);

    delay(INIBITIONTIME_MS);
    _externalFlag = false;
    digitalWrite(LED_BUILTIN, LOW);
  }

  if (_dumpFlag) {
    dumpFile();
    delay(INIBITIONTIME_MS);
    _dumpFlag = false;
    digitalWrite(LED_BUILTIN, LOW);
  }

  noInterrupts();
  if (!(_rtcFlag || _externalFlag || _dumpFlag)) LowPower.sleep();
  interrupts();
}

/* Fija fecha y hora del rtc a partir de dos cadenas con el formato de __DATE__ y __TIME__ */
bool setDateTime(const char * date_str, const char * time_str)
{
  char month_str[4];
  char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug",
                        "Sep", "Oct", "Nov", "Dec"};
  uint16_t i, mday, month = 0, hour, min, sec, year;

  if (sscanf(date_str, "%3s %hu %hu", month_str, &mday, &year) != 3) return false;
  if (sscanf(time_str, "%hu:%hu:%hu", &hour, &min, &sec) != 3) return false;

  for (i = 0; i < 12; i++) {
    if (!strncmp(month_str, months[i], 3)) {
      month = i + 1;
      break;
    }
  }
  if (i == 12) return false;

  rtc.setTime((uint8_t)hour, (uint8_t)min, (uint8_t)sec);
  rtc.setDate((uint8_t)mday, (uint8_t)month, (uint8_t)(year - 2000));
  return true;
}

/* Devuelve la fecha y hora en formato estándar (lectura atómica vía getEpoch) */
const char* getDateTime()
{
  const char *weekDay[7] = { "Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat" };

  time_t epoch = rtc.getEpoch();

  struct tm stm;
  gmtime_r(&epoch, &stm);

  static char dateTime[32];
  snprintf(dateTime, sizeof(dateTime), "%s %4u/%02u/%02u %02u:%02u:%02u",
           weekDay[stm.tm_wday],
           stm.tm_year + 1900, stm.tm_mon + 1, stm.tm_mday,
           stm.tm_hour, stm.tm_min, stm.tm_sec);

  return dateTime;
}

/*
 Programa la alarma del rtc, Indica los campos que tiene que comparar (MATCH_YYMMDDHHMMSS) 
 y asigna un valor periodico
*/ 
void setPeriodicAlarm(uint32_t period_sec, uint32_t offsetFromNow_sec)
{
  _period_sec = period_sec;
  rtc.setAlarmEpoch(rtc.getEpoch() + offsetFromNow_sec);
  rtc.enableAlarm(rtc.MATCH_YYMMDDHHMMSS);
}

/*
 ISR de la alarma del RTC
 Ativa la _rtcFlag y asigna la siguiente alarma
*/

void alarmCallback()
{
  digitalWrite(LED_BUILTIN, HIGH);
  _rtcFlag = true;
  rtc.setAlarmEpoch(rtc.getEpoch() + _period_sec);
}

/*
  ISR de las interrupciones de escritura
  Activa el flag de interrupción externa de escritura
*/
void writeExternalISR()
{
  digitalWrite(LED_BUILTIN, HIGH);
  _externalFlag = true;
}


/*
  ISR de las interrupciones de volcado de fichero
  Activa el flag de interrupción externa de lectura
*/
void dumpISR()
{
  digitalWrite(LED_BUILTIN, HIGH);
  _dumpFlag = true;
}

/*
Monta el sistema de archivos (formateando si hace falta) y crea o trunca el fichero.
*/
bool prepareFile()
{
  SerialUSB.println("Preparing filesystem...");
  int res = filesystem.mount();

  if (res == SPIFFS_ERR_NOT_A_FS) {
    SerialUSB.println("No filesystem found, formatting ...");
    filesystem.unmount();
    res = filesystem.format();
    if (res == SPIFFS_OK) res = filesystem.mount();
  }

  if (res != SPIFFS_OK) {
    SerialUSB.print("mount()/format() failed with error code ");
    SerialUSB.println(res);
    return false;
  }

  File file = filesystem.open(filename, CREATE | TRUNCATE);
  if (!file) {
    SerialUSB.print("Creation of file ");
    SerialUSB.print(filename);
    SerialUSB.println(" failed. Aborting ...");
    return false;
  }
  file.close();
  return true;
}

/*
  Añade una línea al final del fichero (se abre el fichero y se cierra en cada llamada)
*/
void writeLine(const char *message)
{
  File file = filesystem.open(filename, WRITE_ONLY | APPEND);

  if (!file) {
    SerialUSB.print("Opening file ");
    SerialUSB.print(filename);
    SerialUSB.println(" failed for appending. Aborting ...");
    on_exit_with_error_do();
  }

  int const bytes_to_write = strlen(message);
  int const bytes_written  = file.write((void *)message, bytes_to_write);

  if (bytes_to_write != bytes_written) {
    SerialUSB.print("write() failed with error code ");
    SerialUSB.println(filesystem.err());
    file.close();
    on_exit_with_error_do();
  }

  file.close();
}

/*
Abre el fichero en modo lectura, vuelca el contenido en el SeriaUSB y cierra el fichero
*/
void dumpFile()
{
  SerialUSB.begin(9600);
  while (!SerialUSB) { ; }

  File file = filesystem.open(filename, READ_ONLY);
  if (!file) {
    SerialUSB.print(filename);
    SerialUSB.println(" failed for reading. Aborting ...");
    on_exit_with_error_do();
  }

  SerialUSB.print("Reading file contents:\n\t ");
  while (!file.eof()) {
    char character;
    int const bytes_read = file.read(&character, sizeof(character));
    if (bytes_read) {
      SerialUSB.print(character);
      if (character == '\n') SerialUSB.print("\t ");
    }
  }
  file.close();

  SerialUSB.println("\n--- FIN VOLCADO ---");
}

/*Desmonta el sistema de archivos y termina en caso de error */
void on_exit_with_error_do()
{
  filesystem.unmount();
  exit(EXIT_FAILURE);
}
