/**
 * @file    log_backend_uart.c
 * @brief   Logger backend: USART2 (PA2/PA3), polled transmit, LL drivers.
 * @author  32-bit-bender
 * @date    2026-10-08
 *
 * Simple first version. The log task spins on the TXE flag, which costs
 * about 11 us per byte at 921600 baud, i.e. roughly 1.1 ms for a 100-byte
 * line. The task runs at the lowest application priority, so anything more
 * urgent preempts it. To stop burning those cycles, replace this one file
 * with an interrupt-driven or DMA version; log.c does not change.
 */

#include <stdint.h>

#include "log/log_backend.h"
#include "periph/usart2.h"

#ifdef LOG_ENABLE /* the whole file is empty unless logging is enabled */

#include "stm32f4xx.h"
#include "stm32f4xx_ll_usart.h"

#define USART_NOTIFICATION_TIMEOUT_MS           50

#define USART_ERROR_TIMEOUT                     0x01
#define USART_ERROR_TRANS_ERR                   0x02

static uint32_t usart_transmissio_error = 0;


/* Your UART module's init (enables clocks, configures PA2/PA3 and USART2).
 * Replace with the include of your own header if you have one. */
void uart2_init(void);

void log_backend_init(void)
{
    /* The baud rate divisor is computed from the current PCLK1, so the
     * system clock must already be configured when this runs. */
    uart2_init();
}

void log_backend_write(const char* const data, const size_t len)
{
    uint32_t nv;

    usart2_start_dma_tx((uint8_t *)data, len);

    nv = rtos_task_wait_notification(USART_NOTIFICATION_TIMEOUT_MS);

    if(nv != NOTIFICATION_ERR)
    {
        if(nv == NOTIFICATION_NONE)
        {
            usart_transmissio_error |= USART_ERROR_TIMEOUT;
        }
    }
    else
    {
        usart_transmissio_error |= USART_ERROR_TRANS_ERR;
    }
}

/* DMA1 STREAM 6 Transmission Complete Callback */
void dma1_s6_usart2_tc_callback(void)
{
    rtos_task_notify_op_from_isr(log_task_h, NOTIFICATION_OK);
}

/* DMA1 STREAM 6 Transmission Error Callback */
void dma1_s6_usart2_te_callback(void)
{
    rtos_task_notify_op_from_isr(log_task_h, NOTIFICATION_ERR);
}

#endif /* LOG_ENABLE */
