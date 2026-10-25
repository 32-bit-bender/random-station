/**
 * @file    ili9341.h
 * @brief   TFT display driver, based on ILI9341
 * @author  32-bit-bender
 * @date    2026-10-22
 */

#ifndef ILI9341_H
#define ILI9341_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#define ILI9341_CMD_READ_ID             0x04
#define ILI9341_RSP_LEN_READ_ID         4

void ili9341_init(void);
void ili9341_reset(void);
void ili9341_fill(uint16_t color);
void ili9341_fill_square(int16_t x, int16_t y, int16_t size, uint16_t color);
void ili9341_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
bool ili9341_write_cmd(uint8_t cmd, const uint8_t *data, size_t len);


#ifdef __cplusplus
}
#endif

#endif /* ILI9341_H */
