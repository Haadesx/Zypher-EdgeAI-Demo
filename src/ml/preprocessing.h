/*
 * SPDX-License-Identifier: MIT
 *
 * Zephyr Edge AI Demo - Preprocessing Header
 */

#ifndef PREPROCESSING_H
#define PREPROCESSING_H

#include "sensor_hal.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the preprocessing module
 */
void preprocessing_init(void);

/**
 * @brief Add a new accelerometer sample to the window
 *
 * @param sample Pointer to new sample
 * @return 0 on success, negative error code otherwise
 */
int preprocessing_add_sample(const struct accel_sample *sample);

/**
 * @brief Check if the sample window is ready for inference
 *
 * @return true if window is complete
 */
bool preprocessing_window_ready(void);

/**
 * @brief Get the preprocessed input data for inference
 *
 * Retrieves the quantized input tensor data from the current window.
 * Clears the window ready flag after reading.
 *
 * @param output Buffer to fill with INT8 quantized data
 * @param output_size Size of output buffer (must be >= ML_INPUT_SIZE)
 * @return 0 on success, -EAGAIN if window not ready
 */
int preprocessing_get_input(int8_t *output, size_t output_size);

/**
 * @brief Clear the sample window
 */
void preprocessing_clear_window(void);

/**
 * @brief Get current window fill level
 *
 * @return Number of samples currently in the window
 */
size_t preprocessing_get_window_fill(void);

#ifdef __cplusplus
}
#endif

#endif /* PREPROCESSING_H */
