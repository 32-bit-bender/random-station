#include "stm32f4xx.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_gpio.h"

#include "periph/adc1.h"
#include "periph/dma.h"
#include "periph/rcc.h"
#include "periph/timers.h"
#include "periph/usart2.h"
#include "rtos/rtos.h"

#include "log/log.h"

int main(void)
{
    rcc_init();
    tim6_init();
    uart2_init();
    dma_init();
    adc1_init();

    log_init();               /* creates the queue and the log task */

    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_5, LL_GPIO_MODE_OUTPUT);

    rtos_init();

    vTaskStartScheduler();

    /* Only reached if the scheduler could not start (not enough heap). */
    for (;;) {
    }
}

