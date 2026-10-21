/**
 * @file    spi2.h
 * @brief   SPI 2 DRIVER
 * @author  32-bit-bender
 * @date    2026-10-20
 */

#ifndef SPI2_H
#define SPI2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void spi2_init(void);

bool spi2_trx(const uint8_t *tx, uint8_t *rx, size_t len);

bool spi2_write_read(const uint8_t *tx, size_t tx_len, uint8_t *rx,
                     size_t rx_len);

bool spi2_write(const uint8_t *tx, size_t len);
bool spi2_read(uint8_t *rx, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* SPI2_H */
