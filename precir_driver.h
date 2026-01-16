#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Initialize the driver (TIM resources etc)
 */
void precir_driver_init();

/**
 * @brief Deinitialize
 */
void precir_driver_deinit();

/**
 * @brief Transmit a raw byte frame using PP4 or PP16 encoding
 * 
 * @param frame Buffer of bytes to send
 * @param len length of buffer
 * @param pp16 Use PP16 encoding (true) or PP4 (false)
 */
void precir_driver_transmit(const uint8_t* frame, size_t len, bool pp16);
