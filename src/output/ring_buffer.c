/*
 * SPDX-License-Identifier: MIT
 *
 * Zephyr Edge AI Demo - Ring Buffer Implementation
 *
 * Thread-safe ring buffer for queuing inference results.
 */

#include "ring_buffer.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(ring_buffer, CONFIG_LOG_DEFAULT_LEVEL);

/* ============================================================================
 * Configuration
 * ============================================================================ */

#ifndef CONFIG_OUTPUT_RING_BUFFER_SIZE
#define CONFIG_OUTPUT_RING_BUFFER_SIZE 16
#endif

/* ============================================================================
 * Private Data
 * ============================================================================ */

static inference_result_t buffer[CONFIG_OUTPUT_RING_BUFFER_SIZE];
static size_t head = 0;
static size_t tail = 0;
static size_t count = 0;

static K_MUTEX_DEFINE(buffer_mutex);

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void result_buffer_init(void)
{
    k_mutex_lock(&buffer_mutex, K_FOREVER);
    
    head = 0;
    tail = 0;
    count = 0;
    memset(buffer, 0, sizeof(buffer));
    
    LOG_INF("Result buffer initialized (size: %d)", CONFIG_OUTPUT_RING_BUFFER_SIZE);
    
    k_mutex_unlock(&buffer_mutex);
}

int result_buffer_push(const inference_result_t *result)
{
    if (result == NULL) {
        return -EINVAL;
    }
    
    k_mutex_lock(&buffer_mutex, K_FOREVER);
    
    if (count >= CONFIG_OUTPUT_RING_BUFFER_SIZE) {
        LOG_WRN("Result buffer full, dropping oldest");
        /* Overwrite oldest entry */
        tail = (tail + 1) % CONFIG_OUTPUT_RING_BUFFER_SIZE;
        count--;
    }
    
    buffer[head] = *result;
    head = (head + 1) % CONFIG_OUTPUT_RING_BUFFER_SIZE;
    count++;
    
    k_mutex_unlock(&buffer_mutex);
    
    return 0;
}

int result_buffer_pop(inference_result_t *result)
{
    if (result == NULL) {
        return -EINVAL;
    }
    
    k_mutex_lock(&buffer_mutex, K_FOREVER);
    
    if (count == 0) {
        k_mutex_unlock(&buffer_mutex);
        return -EAGAIN;
    }
    
    *result = buffer[tail];
    tail = (tail + 1) % CONFIG_OUTPUT_RING_BUFFER_SIZE;
    count--;
    
    k_mutex_unlock(&buffer_mutex);
    
    return 0;
}

bool result_buffer_empty(void)
{
    bool empty;
    
    k_mutex_lock(&buffer_mutex, K_FOREVER);
    empty = (count == 0);
    k_mutex_unlock(&buffer_mutex);
    
    return empty;
}

bool result_buffer_full(void)
{
    bool full;
    
    k_mutex_lock(&buffer_mutex, K_FOREVER);
    full = (count >= CONFIG_OUTPUT_RING_BUFFER_SIZE);
    k_mutex_unlock(&buffer_mutex);
    
    return full;
}

size_t result_buffer_count(void)
{
    size_t cnt;
    
    k_mutex_lock(&buffer_mutex, K_FOREVER);
    cnt = count;
    k_mutex_unlock(&buffer_mutex);
    
    return cnt;
}
