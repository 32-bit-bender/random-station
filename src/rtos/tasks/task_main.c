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

#include "periph/spi2.h"
#include "periph/tim.h"

LOG_MODULE("main");

#define SPI2_CS1_PC9  LL_GPIO_PIN_9

void task_main(void* arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    LOG_INF("task started");
    spi2_init();
    uint8_t tx_buff[1] = {0};
    uint8_t rx_buff[1] = {0};
    uint8_t cmd_list[4] = {0x01, 0x02, 0x03, 0x04};
    for (;;) {
        for (uint8_t i = 0; i<sizeof(cmd_list); i++)
        {
            tx_buff[0] = cmd_list[i];

            LOG_INF("Sedinding spi command 0x%02X", cmd_list[i]);
            bool ok = spi2_write(tx_buff, 1);
            if(ok)
            {
                vTaskDelay(1);
                ok = spi2_read(rx_buff, 1);
            }

            LOG_INF("Response: 0x%02X", rx_buff[0]);

            vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000));
        }

    }
}
