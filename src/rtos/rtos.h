/**
 * @file    rtos.h
 * @brief   RTOS initialization and wrappers
 * @author  32-bit-bender
 * @date    2026-10-04
 */
#ifndef RTOS_H
#define RTOS_H

#include <stdbool.h>
#include <stdint.h>

/* Order matters: FreeRTOS.h first, then the rest. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include "event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NOTIFICATION_NONE           0x00000000
#define NOTIFICATION_OK             0x00000001
#define NOTIFICATION_ERR            0x00000002

/* Stack sizes are given to FreeRTOS in words. Write them in bytes instead. */
#define RTOS_STACK_BYTES(bytes) ((uint16_t)((bytes) / sizeof(StackType_t)))

/* Central priority table. Higher number = higher priority.
 * Must stay below configMAX_PRIORITIES (5 in FreeRTOSConfig.h). */
enum {
    RTOS_PRIO_BACKGROUND = 1,
    RTOS_PRIO_LAMP       = 2,
    RTOS_PRIO_AUDIO      = 3,
};

static inline void rtos_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static inline uint32_t rtos_now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static inline bool rtos_task_create(TaskFunction_t fn, const char *name,
                                    uint16_t stack_bytes, UBaseType_t prio,
                                    TaskHandle_t *handle)
{
    return xTaskCreate(fn, name, RTOS_STACK_BYTES(stack_bytes), NULL, prio,
                       handle) == pdPASS;
}

uint32_t rtos_task_wait_notification(const uint32_t period_ms);
void rtos_task_notify_op_from_isr(const TaskHandle_t task_h, const uint32_t nv);
void rtos_init(void);

#ifdef __cplusplus
}
#endif

#endif /* RTOS_H */
