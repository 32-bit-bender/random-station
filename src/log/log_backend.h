/**
 * @file    log_backend.h
 * @brief   Transport interface for the logger (UART, SWO, ...).
 * @author  32-bit-bender
 * @date    2026-10-08
 *
 * Both functions are only ever called from the log task (and
 * log_backend_init() once from log_init()), so a backend needs no locking.
 */
#ifndef LOG_BACKEND_H
#define LOG_BACKEND_H

#include <stddef.h>
#include "rtos/rtos.h"

#ifdef __cplusplus
extern "C" {
#endif

extern TaskHandle_t log_task_h;

/* Bring up the transport (clocks, pins, peripheral). */
void log_backend_init(void);

/* Send len bytes. May block the calling task, must not be called from an
 * ISR. */
void log_backend_write(const char* const data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* LOG_BACKEND_H */
