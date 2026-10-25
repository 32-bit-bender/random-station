/**
 * @file    task_main.c
 * @brief   Main task
 * @author  32-bit-bender
 * @date    2026-10-05
 */

#include <stm32f4xx_ll_gpio.h>
#include <stm32f4xx_ll_tim.h>
#include <string.h>

#include "driver/hc_sr04_us_range.h"
#include "log/log.h"
#include "periph/exti.h"
#include "rtos/rtos.h"
#include "rtos/tasks/task_main.h"

#include "driver/ili9341.h"
#include "periph/spi2.h"
#include "periph/tim.h"

LOG_MODULE("main");

void task_main(void* arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    LOG_INF("task started");
    spi2_init();
    ili9341_init();
    uint8_t tx_buff[16] = {0};
    uint8_t rx_buff[16] = {0};
    tx_buff[0] = 0x0A;
    memset(&tx_buff[1], 0xFF, 8);
    uint16_t colors[5] = {0x0000, 0x001F, 0x07E0, 0xF800, 0xFFFF};
    ili9341_fill(0xFFFF);
    for (;;) {
        for (uint8_t i=0; i<25; i++)
        {
            ili9341_fill_square(10*i,10*i,50,colors[4]);
            ili9341_fill_square(10*(i+1),10*(i+1),50,colors[i%3 + 1]);
            vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100));
        }
    }
}
