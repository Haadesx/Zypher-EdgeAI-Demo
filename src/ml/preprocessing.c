/*
 * SPDX-License-Identifier: MIT
 *
 * Zephyr Edge AI Demo - Data Preprocessing
 *
 * This file handles data preprocessing for the gesture recognition model,
 * including:
 *   - Sliding window accumulation
 *   - INT8 quantization
 *   - Mean removal for DC offset compensation
 */

#include "preprocessing.h"
#include "sensor_hal.h"
#include "inference.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(preprocessing, CONFIG_LOG_DEFAULT_LEVEL);

/* ============================================================================
 * Configuration
 * ============================================================================ */

/** Quantization scale (input range to INT8) */
#define QUANT_SCALE (127.0f / 16384.0f)  /* Map ±16384 to ±127 */

/** DC offset filter coefficient (exponential moving average) */
#define DC_FILTER_ALPHA 0.95f

/* ============================================================================
 * Private Data
 * ============================================================================ */

/** Sliding window buffer for accelerometer samples */
static struct accel_sample sample_window[CONFIG_ML_INFERENCE_WINDOW_SIZE];

/** Current position in the window */
static size_t window_pos = 0;

/** Window is full flag */
static bool window_ready = false;

/** DC offset estimates (exponential moving average) */
static float dc_offset[3] = {0.0f, 0.0f, 8192.0f};  /* Initial Z = 1g */

/** Mutex for thread safety */
static K_MUTEX_DEFINE(preprocess_mutex);

/** Initialization flag */
static bool initialized = false;

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void preprocessing_init(void)
{
    k_mutex_lock(&preprocess_mutex, K_FOREVER);
    
    window_pos = 0;
    window_ready = false;
    
    /* Reset DC offset estimates */
    dc_offset[0] = 0.0f;
    dc_offset[1] = 0.0f;
    dc_offset[2] = 8192.0f;  /* Assume 1g on Z-axis initially */
    
    /* Clear the window */
    memset(sample_window, 0, sizeof(sample_window));
    
    initialized = true;
    
    LOG_INF("Preprocessing initialized (window size: %d samples)",
            CONFIG_ML_INFERENCE_WINDOW_SIZE);
    
    k_mutex_unlock(&preprocess_mutex);
}

int preprocessing_add_sample(const struct accel_sample *sample)
{
    if (!initialized) {
        return -EINVAL;
    }
    
    if (sample == NULL) {
        return -EINVAL;
    }
    
    k_mutex_lock(&preprocess_mutex, K_FOREVER);
    
    /* Update DC offset estimate using exponential moving average */
    dc_offset[0] = DC_FILTER_ALPHA * dc_offset[0] + 
                   (1.0f - DC_FILTER_ALPHA) * (float)sample->x;
    dc_offset[1] = DC_FILTER_ALPHA * dc_offset[1] + 
                   (1.0f - DC_FILTER_ALPHA) * (float)sample->y;
    dc_offset[2] = DC_FILTER_ALPHA * dc_offset[2] + 
                   (1.0f - DC_FILTER_ALPHA) * (float)sample->z;
    
    /* Add sample to window */
    sample_window[window_pos] = *sample;
    window_pos++;
    
    /* Check if window is complete */
    if (window_pos >= CONFIG_ML_INFERENCE_WINDOW_SIZE) {
        window_ready = true;
        window_pos = 0;  /* Reset for next window */
        
        LOG_DBG("Window complete, ready for inference");
    }
    
    k_mutex_unlock(&preprocess_mutex);
    
    return 0;
}

bool preprocessing_window_ready(void)
{
    bool ready;
    
    k_mutex_lock(&preprocess_mutex, K_FOREVER);
    ready = window_ready;
    k_mutex_unlock(&preprocess_mutex);
    
    return ready;
}

int preprocessing_get_input(int8_t *output, size_t output_size)
{
    if (output == NULL) {
        return -EINVAL;
    }
    
    if (output_size < ML_INPUT_SIZE) {
        LOG_ERR("Output buffer too small: %zu < %d", output_size, ML_INPUT_SIZE);
        return -ENOSPC;
    }
    
    k_mutex_lock(&preprocess_mutex, K_FOREVER);
    
    if (!window_ready) {
        k_mutex_unlock(&preprocess_mutex);
        return -EAGAIN;
    }
    
    /* Convert samples to quantized format */
    size_t out_idx = 0;
    
    for (size_t i = 0; i < CONFIG_ML_INFERENCE_WINDOW_SIZE; i++) {
        /* Remove DC offset and quantize */
        float x = (float)sample_window[i].x - dc_offset[0];
        float y = (float)sample_window[i].y - dc_offset[1];
        float z = (float)sample_window[i].z - dc_offset[2];
        
        /* Quantize to INT8 */
        int32_t qx = (int32_t)(x * QUANT_SCALE);
        int32_t qy = (int32_t)(y * QUANT_SCALE);
        int32_t qz = (int32_t)(z * QUANT_SCALE);
        
        /* Clamp to INT8 range */
        qx = (qx < -128) ? -128 : (qx > 127) ? 127 : qx;
        qy = (qy < -128) ? -128 : (qy > 127) ? 127 : qy;
        qz = (qz < -128) ? -128 : (qz > 127) ? 127 : qz;
        
        output[out_idx++] = (int8_t)qx;
        output[out_idx++] = (int8_t)qy;
        output[out_idx++] = (int8_t)qz;
    }
    
    /* Mark window as consumed */
    window_ready = false;
    
    k_mutex_unlock(&preprocess_mutex);
    
    return 0;
}

void preprocessing_clear_window(void)
{
    k_mutex_lock(&preprocess_mutex, K_FOREVER);
    
    window_pos = 0;
    window_ready = false;
    memset(sample_window, 0, sizeof(sample_window));
    
    LOG_DBG("Window cleared");
    
    k_mutex_unlock(&preprocess_mutex);
}

size_t preprocessing_get_window_fill(void)
{
    size_t fill;
    
    k_mutex_lock(&preprocess_mutex, K_FOREVER);
    fill = window_pos;
    k_mutex_unlock(&preprocess_mutex);
    
    return fill;
}
