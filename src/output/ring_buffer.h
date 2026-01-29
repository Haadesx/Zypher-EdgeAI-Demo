/*
 * SPDX-License-Identifier: MIT
 *
 * Zephyr Edge AI Demo - Ring Buffer Header
 */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include "inference.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the result ring buffer
 */
void result_buffer_init(void);

/**
 * @brief Push an inference result to the buffer
 *
 * @param result Result to store
 * @return 0 on success, -ENOSPC if buffer full
 */
int result_buffer_push(const inference_result_t *result);

/**
 * @brief Pop an inference result from the buffer
 *
 * @param result Buffer to store popped result
 * @return 0 on success, -EAGAIN if buffer empty
 */
int result_buffer_pop(inference_result_t *result);

/**
 * @brief Check if buffer is empty
 */
bool result_buffer_empty(void);

/**
 * @brief Check if buffer is full
 */
bool result_buffer_full(void);

/**
 * @brief Get number of items in buffer
 */
size_t result_buffer_count(void);

#ifdef __cplusplus
}
#endif

#endif /* RING_BUFFER_H */
