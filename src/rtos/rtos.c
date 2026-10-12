/**
 * @file    rtos.c
 * @brief   RTOS initialization and wrappers
 * @author  32-bit-bender
 * @date    2026-10-04
 */

#include <stm32f4xx.h>

#include "rtos/tasks/task_main.h"
#include "rtos.h"

TaskHandle_t task_main_h = NULL;

void rtos_init(void)
{
    /* All priority bits are preemption bits, as FreeRTOS requires. */
    NVIC_SetPriorityGrouping(3U);

    (void) xTaskCreate(task_main, "main task", 256, NULL, 1, &task_main_h);
}

uint32_t rtos_task_wait_notification(const uint32_t period_ms)
{
    uint32_t nv;

    nv = 0U;

    (void)xTaskNotifyWaitIndexed(
        1U,
        0U,
        UINT32_MAX,
        &nv,
        pdMS_TO_TICKS(period_ms)
    );

    return nv;
}

void rtos_task_notify_op_from_isr(const TaskHandle_t task_h, const uint32_t nv)
{
    BaseType_t need_yield;

    need_yield = pdFALSE;

    (void)xTaskNotifyIndexedFromISR(task_h, 1U, nv, eSetBits, &need_yield);

    portYIELD_FROM_ISR(need_yield);
}


/* Required because configCHECK_FOR_STACK_OVERFLOW and
 * configUSE_MALLOC_FAILED_HOOK are enabled. */
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}
