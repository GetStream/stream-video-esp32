#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

static portMUX_TYPE s_atomic_mux = portMUX_INITIALIZER_UNLOCKED;

#ifdef __cplusplus
extern "C" {
#endif

uint8_t __atomic_fetch_add_1(volatile void *ptr, uint8_t val, int memorder)
{
    (void)memorder;
    portENTER_CRITICAL(&s_atomic_mux);
    uint8_t old = *(volatile uint8_t *)ptr;
    *(volatile uint8_t *)ptr = (uint8_t)(old + val);
    portEXIT_CRITICAL(&s_atomic_mux);
    return old;
}

uint8_t __atomic_fetch_sub_1(volatile void *ptr, uint8_t val, int memorder)
{
    (void)memorder;
    portENTER_CRITICAL(&s_atomic_mux);
    uint8_t old = *(volatile uint8_t *)ptr;
    *(volatile uint8_t *)ptr = (uint8_t)(old - val);
    portEXIT_CRITICAL(&s_atomic_mux);
    return old;
}

#ifdef __cplusplus
}
#endif
