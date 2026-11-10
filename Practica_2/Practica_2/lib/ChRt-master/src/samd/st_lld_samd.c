#if defined(__SAMD21G18A__) || defined(__SAMD21J18A__)
#include "../hal/osal.h"
#if OSAL_ST_MODE == OSAL_ST_MODE_PERIODIC
#if OSAL_ST_FREQUENCY != 1000
#error "SAMD only supports CH_CFG_ST_FREQUENCY == 1000"
#endif

static int sysTickEnabled = 0;

/*
 * Boards whose bootloader starts SysTick (e.g. MKR WAN 1310: board_init()
 * calls SysTick_Config(1000) and jumps to the sketch with IRQs enabled) can
 * deliver SysTick interrupts while Reset_Handler is still copying .data and
 * zeroing .bss. sysTickEnabled may then hold a stale value from the previous
 * run. Only trust it once the core's init() has programmed the 1 kHz tick:
 * by then .bss has been zeroed.
 */
#define ST_LLD_SAMD_RELOAD ((F_CPU / OSAL_ST_FREQUENCY) - 1u)

int sysTickHook(void) { 
  if (sysTickEnabled && SysTick->LOAD == ST_LLD_SAMD_RELOAD) {
  CH_IRQ_PROLOGUE();

  chSysLockFromISR();
  chSysTimerHandlerI();
  chSysUnlockFromISR();

  CH_IRQ_EPILOGUE();
  }
  return 0;
}
void st_lld_init(void) {
  sysTickEnabled = 1;  
}
#endif  // OSAL_ST_MODE == OSAL_ST_MODE_PERIODIC
#endif  // #if defined(__SAMD21G18A__) || defined(__SAMD21J18A__)