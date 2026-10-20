/**
 * @file    task_main.c
 * @brief   Main task
 * @author  32-bit-bender
 * @date    2026-10-05
 */

#include <stm32f4xx_ll_gpio.h>
#include <stm32f4xx_ll_tim.h>

#include "driver/hc_sr04_us_range.h"
#include "log/log.h"
#include "periph/exti.h"
#include "rtos/rtos.h"
#include "rtos/tasks/task_main.h"

#include "periph/tim.h"

LOG_MODULE("main");


void task_main(void* arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    LOG_INF("task started");
    for (;;) {
        // LL_GPIO_TogglePin(GPIOA, LL_GPIO_PIN_5);
        LOG_INF("TIM3 COUNT %ld", (uint32_t) LL_TIM_GetCounter(TIM3));

        LOG_INF("Distance: %ld us",  exti_get_last_pulse_response_delay());
        LOG_INF("Distance: %ld cm",  hc_s04_get_last_recorded_range_cm());
        if (hc_s04_get_last_recorded_range_cm()<=20)
        {
            LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_5);
        }
        else
        {
            LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_5);
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(500));
    }
}
