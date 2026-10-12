/**
 * @file    log.c
 * @brief   FreeRTOS logger: format in the caller, queue lines, print in a
 *          low-priority task.
 * @author  32-bit-bender
 * @date    2026-10-08
 *
 * Flow:  LOG_INF(...) -> log_write() formats one line into a local struct
 *        -> xQueueSendToBack(0 ticks) -> log task -> log_backend_write().
 *
 * Rules this file relies on:
 *  - Never block the caller: a full queue drops the line and counts it.
 *  - Not usable from ISRs (the line is dropped and counted).
 *  - Do not use LOG_* inside this file or inside a backend.
 */
#include "log.h"

#ifdef LOG_ENABLE /* the whole file is empty unless logging is enabled */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "log_backend.h"
#include "rtos/rtos.h"
#include "stm32f4xx.h"

#ifndef LOG_LINE_MAX
#define LOG_LINE_MAX 255 /* bytes per line, including the trailing CR LF */
#endif
#ifndef LOG_QUEUE_LEN
#define LOG_QUEUE_LEN 16 /* lines buffered before new ones are dropped */
#endif

#define LOG_TASK_STACK_BYTES 768U
#define LOG_LOCK_TIMEOUT_MS  5U
#define LOG_IDLE_POLL_MS     200U

_Static_assert(LOG_LINE_MAX <= 255, "line length must fit in uint8_t");

TaskHandle_t log_task_h;

typedef struct {
    uint8_t len;
    char text[LOG_LINE_MAX];
} log_line_t;

static QueueHandle_t log_queue;
static SemaphoreHandle_t fmt_mutex;
static uint32_t dropped_total;

static void count_drop(void)
{
    __atomic_fetch_add(&dropped_total, 1U, __ATOMIC_RELAXED);
}

static char level_char(const int level)
{
    switch (level) {
    case LOG_LEVEL_ERR:
        return 'E';
    case LOG_LEVEL_WRN:
        return 'W';
    case LOG_LEVEL_INF:
        return 'I';
    case LOG_LEVEL_DBG:
        return 'D';
    default:
        return '?';
    }
}

/* Build "[tick] L module: message\r\n" into line. Longer lines are
 * truncated, but always end with CR LF. */
static void format_line(log_line_t* const line, const int level,
                       const char *module, const char* const fmt, va_list ap)
{
    const size_t cap = sizeof line->text - 2U; /* room for CR LF */
    int n = snprintf(line->text, cap, "[%lu] %c %s: ",
                     (unsigned long)xTaskGetTickCount(), level_char(level),
                     module);
    int m;
    size_t len;

    if (n < 0) {
        n = 0;
    }
    if ((size_t)n >= cap) {
        n = (int)cap - 1; /* even the prefix was truncated */
    }

    m = vsnprintf(line->text + n, cap - (size_t)n, fmt, ap);
    if (m < 0) {
        m = 0;
    }

    len = (size_t)n + (size_t)m;
    if (len > cap - 1U) {
        len = cap - 1U; /* message was truncated */
    }
    line->text[len++] = '\r';
    line->text[len++] = '\n';
    line->len = (uint8_t)len;
}

void log_write(int level, const char *module, const char *fmt, ...)
{
    log_line_t line;
    va_list ap;
    bool locked = false;

    if (log_queue == NULL) {
        return; /* log_init() not called, or it failed */
    }
    if (__get_IPSR() != 0U) {
        count_drop(); /* ISR context is not supported */
        return;
    }

    /* newlib's vsnprintf is not guaranteed to be thread-safe without the
     * reentrancy support, so only one task formats at a time. Before the
     * scheduler runs there is only one thread, so no lock is needed. */
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        if (xSemaphoreTake(fmt_mutex, pdMS_TO_TICKS(LOG_LOCK_TIMEOUT_MS)) !=
            pdPASS) {
            count_drop();
            return;
        }
        locked = true;
    }

    va_start(ap, fmt);
    format_line(&line, level, module, fmt, ap);
    va_end(ap);

    if (locked) {
        xSemaphoreGive(fmt_mutex);
    }

    if (xQueueSendToBack(log_queue, &line, 0) != pdPASS) {
        count_drop();
    }
}

uint32_t log_dropped_count(void)
{
    return __atomic_load_n(&dropped_total, __ATOMIC_RELAXED);
}

/* ---- log task ----------------------------------------------------------- */

static void backend_puts(const char *s)
{
    log_backend_write(s, strlen(s));
}

static size_t u32_to_dec(char *dst, uint32_t v)
{
    char tmp[10];
    size_t n = 0U;
    size_t i = 0U;

    do {
        tmp[n++] = (char)('0' + (v % 10U));
        v /= 10U;
    } while (v != 0U);

    while (n != 0U) {
        dst[i++] = tmp[--n];
    }
    return i;
}

/* Formatted by hand so the log task never needs stdio or its stack. */
static void report_dropped(uint32_t count)
{
    char num[12];
    const size_t n = u32_to_dec(num, count);

    backend_puts("W log: dropped ");
    log_backend_write(num, n);
    backend_puts(" message(s)\r\n");
}

static void log_task(void *arg)
{
    log_line_t line;
    uint32_t reported = 0U;

    (void)arg;

    for (;;) {
        if (xQueueReceive(log_queue, &line, pdMS_TO_TICKS(LOG_IDLE_POLL_MS)) ==
            pdPASS) {
            log_backend_write(line.text, line.len);
        }

        /* Report drops once the queue has been emptied, so the warning
         * does not itself compete with pending lines. */
        if (uxQueueMessagesWaiting(log_queue) == 0U) {
            const uint32_t total = log_dropped_count();
            if (total != reported) {
                report_dropped(total - reported);
                reported = total;
            }
        }
    }
}

void log_init(void)
{
    log_backend_init();

    log_queue = xQueueCreate(LOG_QUEUE_LEN, sizeof(log_line_t));
    fmt_mutex = xSemaphoreCreateMutex();

    if ((log_queue == NULL) || (fmt_mutex == NULL) ||
        !rtos_task_create(log_task, "log", LOG_TASK_STACK_BYTES,
                          RTOS_PRIO_BACKGROUND, &log_task_h)) {
        log_queue = NULL; /* disable logging instead of crashing later */
    }
}

#endif /* LOG_ENABLE */
