/**
 * @file    spi2.c
 * @brief   SPI2 master driver (LL), DMA only, shared between tasks.
 * @author  32-bit-bender
 * @date    2026-10-20
 *
 * Pins:  PC2 = MISO, PC3 = MOSI, PC7 = SCK (all AF5), PC9 = CS1 (GPIO).
 * DMA:   DMA1 stream 3 channel 0 = SPI2_RX, stream 4 channel 0 = SPI2_TX.
 *        Check both in the reference manual's DMA1 request mapping table.
 *
 * Synchronisation, two different kinds of semaphore:
 *  - bus_mutex: a mutex. A task takes it at the start of a transaction and
 *    MUST give it back at the end (bus_begin / bus_end).
 *  - done_sem: a binary signalling semaphore, created empty. The DMA
 *    interrupt gives it once, the waiting task takes it. Taking it consumes
 *    the signal, so nobody gives it back.
 *
 * The receive stream's transfer-complete interrupt ends a transfer: when the
 * last byte has been received, every clock pulse has been sent.
 */
#include "periph/spi2.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <stm32f4xx_ll_bus.h>
#include <stm32f4xx_ll_dma.h>
#include <stm32f4xx_ll_gpio.h>
#include <stm32f4xx_ll_spi.h>

#include "rtos/rtos.h"

#define SPI2_MISO_PC2 LL_GPIO_PIN_2
#define SPI2_MOSI_PC3 LL_GPIO_PIN_3
#define SPI2_SCK_PC7  LL_GPIO_PIN_7
#define SPI2_CS1_PC9  LL_GPIO_PIN_9

#define SPI2_RX_STREAM LL_DMA_STREAM_3
#define SPI2_TX_STREAM LL_DMA_STREAM_4

/* Sent when the caller only wants to read. 0xFF suits most devices. */
#define SPI2_DUMMY_BYTE 0xFFU

/* Bounded busy-waits, so a dead peripheral cannot hang the task forever. */
#define SPI2_SPIN_LIMIT 2000UL

/* How long a single DMA transfer may take before it is aborted. */
#define SPI2_XFER_TIMEOUT_MS 200U

/* How long a task waits for the bus before giving up. */
#define SPI2_LOCK_TIMEOUT_MS 200U

/* The DMA interrupt calls FreeRTOS ...FromISR functions, so its priority
 * number must be >= configMAX_SYSCALL_INTERRUPT_PRIORITY (5). */
#define SPI2_DMA_IRQ_PRIO 6U

static LL_GPIO_InitTypeDef gpio_init_spi2_sck_miso_mosi = {
    .Pin = (SPI2_MISO_PC2 | SPI2_MOSI_PC3 | SPI2_SCK_PC7),
    .Mode = LL_GPIO_MODE_ALTERNATE,
    .Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH,
    .OutputType = LL_GPIO_OUTPUT_PUSHPULL,
    .Pull = LL_GPIO_PULL_NO,
    .Alternate = LL_GPIO_AF_5,
};

static LL_GPIO_InitTypeDef gpio_init_spi2_cs = {
    .Pin = SPI2_CS1_PC9,
    .Mode = LL_GPIO_MODE_OUTPUT,
    .Speed = LL_GPIO_SPEED_FREQ_LOW,
    .OutputType = LL_GPIO_OUTPUT_PUSHPULL,
    .Pull = LL_GPIO_PULL_UP,
    .Alternate = LL_GPIO_AF_0,
};

static LL_SPI_InitTypeDef spi2_init_struct;

static SemaphoreHandle_t bus_mutex;
static SemaphoreHandle_t done_sem;
static volatile bool dma_error;

/* ---- initialisation ----------------------------------------------------- */

static void spi2_gpio_init(void)
{
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);

    /* Drive CS high before it becomes an output, so it never glitches low. */
    LL_GPIO_SetOutputPin(GPIOC, SPI2_CS1_PC9);

    LL_GPIO_Init(GPIOC, &gpio_init_spi2_sck_miso_mosi);
    LL_GPIO_Init(GPIOC, &gpio_init_spi2_cs);
}

static void dma_stream_config(uint32_t stream, uint32_t direction)
{
    LL_DMA_SetChannelSelection(DMA1, stream, LL_DMA_CHANNEL_0);
    LL_DMA_SetDataTransferDirection(DMA1, stream, direction);
    LL_DMA_SetStreamPriorityLevel(DMA1, stream, LL_DMA_PRIORITY_HIGH);
    LL_DMA_SetMode(DMA1, stream, LL_DMA_MODE_NORMAL);
    LL_DMA_SetPeriphIncMode(DMA1, stream, LL_DMA_PERIPH_NOINCREMENT);
    LL_DMA_SetPeriphSize(DMA1, stream, LL_DMA_PDATAALIGN_BYTE);
    LL_DMA_SetMemorySize(DMA1, stream, LL_DMA_MDATAALIGN_BYTE);
    LL_DMA_DisableFifoMode(DMA1, stream);
    LL_DMA_SetPeriphAddress(DMA1, stream, LL_SPI_DMA_GetRegAddr(SPI2));
}

static void spi2_dma_init(void)
{
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);

    dma_stream_config(SPI2_RX_STREAM, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    dma_stream_config(SPI2_TX_STREAM, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);

    /* Only the receive stream raises interrupts: when it is done, the whole
     * transfer is done. */
    LL_DMA_EnableIT_TC(DMA1, SPI2_RX_STREAM);
    LL_DMA_EnableIT_TE(DMA1, SPI2_RX_STREAM);
    NVIC_SetPriority(DMA1_Stream3_IRQn, SPI2_DMA_IRQ_PRIO);
    NVIC_EnableIRQ(DMA1_Stream3_IRQn);
}

void spi2_init(void)
{
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_SPI2);

    spi2_gpio_init();

    spi2_init_struct.TransferDirection = LL_SPI_FULL_DUPLEX;
    spi2_init_struct.Mode = LL_SPI_MODE_MASTER;
    spi2_init_struct.DataWidth = LL_SPI_DATAWIDTH_8BIT;
    spi2_init_struct.ClockPolarity = LL_SPI_POLARITY_LOW;
    spi2_init_struct.ClockPhase = LL_SPI_PHASE_1EDGE;
    spi2_init_struct.NSS = LL_SPI_NSS_SOFT;
    spi2_init_struct.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV16;
    spi2_init_struct.BitOrder = LL_SPI_MSB_FIRST;
    spi2_init_struct.CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;
    spi2_init_struct.CRCPoly = 10;
    LL_SPI_Init(SPI2, &spi2_init_struct);
    LL_SPI_SetStandard(SPI2, LL_SPI_PROTOCOL_MOTOROLA);

    spi2_dma_init();

    bus_mutex = xSemaphoreCreateMutex();
    done_sem = xSemaphoreCreateBinary(); /* created empty */
    configASSERT((bus_mutex != NULL) && (done_sem != NULL));

    LL_SPI_Enable(SPI2);
}

/* ---- bus ownership and chip select -------------------------------------- */

static void cs_low(void)
{
    LL_GPIO_ResetOutputPin(GPIOC, SPI2_CS1_PC9);
}

static void cs_high(void)
{
    LL_GPIO_SetOutputPin(GPIOC, SPI2_CS1_PC9);
}

/* Start of a transaction: take the bus, then select the device. */
static bool bus_begin(void)
{
    /* The DMA path sleeps on a semaphore, which needs a running scheduler. */
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return false;
    }
    if (xSemaphoreTake(bus_mutex, pdMS_TO_TICKS(SPI2_LOCK_TIMEOUT_MS)) !=
        pdPASS) {
        return false;
    }
    cs_low();
    return true;
}

/* End of a transaction: deselect the device, then give the bus back. */
static void bus_end(void)
{
    cs_high();
    (void)xSemaphoreGive(bus_mutex);
}

/* ---- DMA transfer ------------------------------------------------------- */

/* The last bit must be fully clocked out before CS may go high. */
static bool wait_not_busy(void)
{
    for (uint32_t i = 0; i < SPI2_SPIN_LIMIT; i++) {
        if (LL_SPI_IsActiveFlag_BSY(SPI2) == 0U) {
            return true;
        }
    }
    return false;
}

static void dma_clear_flags(void)
{
    LL_DMA_ClearFlag_TC3(DMA1);
    LL_DMA_ClearFlag_HT3(DMA1);
    LL_DMA_ClearFlag_TE3(DMA1);
    LL_DMA_ClearFlag_DME3(DMA1);
    LL_DMA_ClearFlag_FE3(DMA1);

    LL_DMA_ClearFlag_TC4(DMA1);
    LL_DMA_ClearFlag_HT4(DMA1);
    LL_DMA_ClearFlag_TE4(DMA1);
    LL_DMA_ClearFlag_DME4(DMA1);
    LL_DMA_ClearFlag_FE4(DMA1);
}

static void dma_stop(void)
{
    LL_SPI_DisableDMAReq_TX(SPI2);
    LL_SPI_DisableDMAReq_RX(SPI2);

    LL_DMA_DisableStream(DMA1, SPI2_TX_STREAM);
    LL_DMA_DisableStream(DMA1, SPI2_RX_STREAM);

    /* A stream stays "enabled" until its current transfer has wound down. */
    for (uint32_t i = 0; i < SPI2_SPIN_LIMIT; i++) {
        if ((LL_DMA_IsEnabledStream(DMA1, SPI2_TX_STREAM) == 0U) &&
            (LL_DMA_IsEnabledStream(DMA1, SPI2_RX_STREAM) == 0U)) {
            break;
        }
    }

    LL_SPI_ClearFlag_OVR(SPI2);
    dma_clear_flags();
}

/* One DMA transfer of at most 65535 bytes. Waits for completion. */
static bool dma_transfer(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    /* Used when the caller does not want to send or receive real data. */
    static uint8_t dummy_tx = SPI2_DUMMY_BYTE;
    static uint8_t dummy_rx;
    bool ok;

    /* Throw away a stale signal, for example from a transfer that timed out
     * and finished late. Without this, the wait below could return at once. */
    (void)xSemaphoreTake(done_sem, 0);
    dma_error = false;
    dma_clear_flags();

    LL_DMA_SetMemoryAddress(DMA1, SPI2_RX_STREAM,
                            (rx != NULL) ? (uint32_t)(uintptr_t)rx
                                         : (uint32_t)(uintptr_t)&dummy_rx);
    LL_DMA_SetMemoryIncMode(DMA1, SPI2_RX_STREAM,
                            (rx != NULL) ? LL_DMA_MEMORY_INCREMENT
                                         : LL_DMA_MEMORY_NOINCREMENT);
    LL_DMA_SetDataLength(DMA1, SPI2_RX_STREAM, len);

    LL_DMA_SetMemoryAddress(DMA1, SPI2_TX_STREAM,
                            (tx != NULL) ? (uint32_t)(uintptr_t)tx
                                         : (uint32_t)(uintptr_t)&dummy_tx);
    LL_DMA_SetMemoryIncMode(DMA1, SPI2_TX_STREAM,
                            (tx != NULL) ? LL_DMA_MEMORY_INCREMENT
                                         : LL_DMA_MEMORY_NOINCREMENT);
    LL_DMA_SetDataLength(DMA1, SPI2_TX_STREAM, len);

    /* Receiver first, so no incoming byte can be missed. */
    LL_DMA_EnableStream(DMA1, SPI2_RX_STREAM);
    LL_DMA_EnableStream(DMA1, SPI2_TX_STREAM);
    LL_SPI_EnableDMAReq_RX(SPI2);
    LL_SPI_EnableDMAReq_TX(SPI2); /* starts the clock */

    /* Sleep until the interrupt signals completion. Taking the semaphore
     * consumes the signal; there is nothing to give back. */
    ok = (xSemaphoreTake(done_sem, pdMS_TO_TICKS(SPI2_XFER_TIMEOUT_MS)) ==
          pdPASS) &&
         !dma_error;

    dma_stop();
    return ok && wait_not_busy();
}

/* Any length: splits into chunks the 16-bit DMA length register can take. */
static bool transfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    while (len > 0U) {
        const uint16_t chunk = (len > 0xFFFFU) ? 0xFFFFU : (uint16_t)len;

        if (!dma_transfer(tx, rx, chunk)) {
            return false;
        }
        if (tx != NULL) {
            tx += chunk;
        }
        if (rx != NULL) {
            rx += chunk;
        }
        len -= chunk;
    }
    return true;
}

/* Receive stream transfer-complete or transfer-error. */
void DMA1_Stream3_IRQHandler(void)
{
    BaseType_t woken = pdFALSE;

    if (LL_DMA_IsActiveFlag_TC3(DMA1) != 0U) {
        LL_DMA_ClearFlag_TC3(DMA1);
        (void)xSemaphoreGiveFromISR(done_sem, &woken);
    }
    if (LL_DMA_IsActiveFlag_TE3(DMA1) != 0U) {
        LL_DMA_ClearFlag_TE3(DMA1);
        dma_error = true;
        (void)xSemaphoreGiveFromISR(done_sem, &woken);
    }
    portYIELD_FROM_ISR(woken);
}

/* ---- public transactions ------------------------------------------------ */

bool spi2_trx(const uint8_t *tx, uint8_t *rx, size_t len)
{
    bool ok;

    if (len == 0U) {
        return true;
    }
    if (!bus_begin()) {
        return false;
    }
    ok = transfer(tx, rx, len);
    bus_end();
    return ok;
}

bool spi2_write_read(const uint8_t *tx, size_t tx_len, uint8_t *rx,
                     size_t rx_len)
{
    bool ok;

    if ((tx_len == 0U) && (rx_len == 0U)) {
        return true;
    }
    if (!bus_begin()) {
        return false;
    }
    ok = transfer(tx, NULL, tx_len) && transfer(NULL, rx, rx_len);
    bus_end();
    return ok;
}

bool spi2_write(const uint8_t *tx, size_t len)
{
    return spi2_trx(tx, NULL, len);
}

bool spi2_read(uint8_t *rx, size_t len)
{
    return spi2_trx(NULL, rx, len);
}
