/**
 * @file    log.h
 * @brief   Leveled, tagged logging. Output goes through a backend (UART).
 * @author  32-bit-bender
 * @date    2026-10-08
 *
 * The logger is compiled only when LOG_ENABLE is defined, for example with
 * -DLOG_ENABLE. Without it, LOG_MODULE, the LOG_* macros, log_init() and
 * log_dropped_count() still exist but do nothing and generate no code, so
 * application files need no #ifdefs.
 *
 * Usage in a .c file:
 *
 *     #include "log.h"
 *     LOG_MODULE("lamp");
 *     ...
 *     LOG_INF("brightness set to %u", level);
 *     LOG_ERR("pwm init failed: %d", status);
 *
 * Set the level at build time, for example -DLOG_LEVEL=4 for a debug build.
 * Messages above that level generate no code at all.
 */
#ifndef LOG_H
#define LOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_LEVEL_NONE 0
#define LOG_LEVEL_ERR  1
#define LOG_LEVEL_WRN  2
#define LOG_LEVEL_INF  3
#define LOG_LEVEL_DBG  4

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INF
#endif

#ifdef LOG_ENABLE

/* Call once before the scheduler starts. */
void log_init(void);

/* The printf attribute makes the compiler check the format string. */
void log_write(int level, const char *module, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

/* Number of messages dropped because the log buffer was full. */
uint32_t log_dropped_count(void);

/* One per .c file. Gives the file a short tag shown in every message. */
#define LOG_MODULE(name) static const char log_module_[] = name

/* The level test is a compile-time constant, so a disabled message is
 * removed by the compiler, but its format string is still type-checked and
 * its arguments still count as used. ##__VA_ARGS__ is a GNU extension,
 * which is fine with the default gnu11. */
#define LOG_(level, fmt, ...)                                                 \
    do {                                                                      \
        if ((level) <= LOG_LEVEL) {                                           \
            log_write((level), log_module_, fmt, ##__VA_ARGS__);              \
        }                                                                     \
    } while (0)

#else /* !LOG_ENABLE */

static inline void log_init(void)
{
}

static inline uint32_t log_dropped_count(void)
{
    return 0U;
}

/* Never called at run time. It only lets the compiler keep type-checking
 * format strings and counting arguments as used while logging is off. */
static inline __attribute__((format(printf, 1, 2))) void
log_check_(const char *fmt, ...)
{
    (void)fmt;
}

/* Expands to a harmless declaration so that "LOG_MODULE("x");" still
 * compiles, without creating an unused variable. */
#define LOG_MODULE(name) _Static_assert(1, "LOG_MODULE")

#define LOG_(level, fmt, ...)                                                 \
    do {                                                                      \
        if (0) {                                                              \
            log_check_(fmt, ##__VA_ARGS__);                                   \
        }                                                                     \
    } while (0)

#endif /* LOG_ENABLE */

#define LOG_ERR(fmt, ...) LOG_(LOG_LEVEL_ERR, fmt, ##__VA_ARGS__)
#define LOG_WRN(fmt, ...) LOG_(LOG_LEVEL_WRN, fmt, ##__VA_ARGS__)
#define LOG_INF(fmt, ...) LOG_(LOG_LEVEL_INF, fmt, ##__VA_ARGS__)
#define LOG_DBG(fmt, ...) LOG_(LOG_LEVEL_DBG, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* LOG_H */
