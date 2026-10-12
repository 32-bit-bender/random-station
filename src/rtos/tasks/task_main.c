/**
 * @file    task_main.c
 * @brief   Main task
 * @author  32-bit-bender
 * @date    2026-10-05
 */

#include <stm32f4xx_ll_gpio.h>

#include "log/log.h"
#include "rtos/rtos.h"
#include "rtos/tasks/task_main.h"

LOG_MODULE("main");


void task_main(void* arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    LOG_INF("task started");
    for (;;) {
        LL_GPIO_TogglePin(GPIOA, LL_GPIO_PIN_5);
        LOG_INF("yoyoyo");
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000));
    }
}
