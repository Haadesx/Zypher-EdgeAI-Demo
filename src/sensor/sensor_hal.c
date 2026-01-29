/*
 * SPDX-License-Identifier: MIT
 *
 * Zephyr Edge AI Demo - Sensor HAL Implementation
 *
 * This file implements the sensor hardware abstraction layer,
 * routing to either real hardware drivers or mock implementation
 * based on Kconfig settings.
 */

#include "sensor_hal.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sensor_hal, CONFIG_LOG_DEFAULT_LEVEL);

/* ============================================================================
 * Private Data
 * ============================================================================ */

/** Sensor initialization flag */
static bool initialized = false;

/** Statistics tracking */
static struct sensor_stats stats = {0};

/** Mutex for thread-safe access */
static K_MUTEX_DEFINE(sensor_mutex);

/** Last sample time for rate calculation */
static uint32_t last_sample_time = 0;
static uint32_t sample_interval_sum = 0;
static uint32_t sample_interval_count = 0;

/* ============================================================================
 * External Mock Sensor Functions (defined in mock_accel.c)
 * ============================================================================ */

#ifdef CONFIG_SENSOR_USE_MOCK
extern int mock_accel_init(void);
extern int mock_accel_read(struct accel_sample *sample);
extern bool mock_accel_data_ready(void);
#endif

/* ============================================================================
 * Timestamp Implementation
 * ============================================================================ */

uint32_t sensor_get_timestamp_us(void)
{
    return (uint32_t)(k_uptime_get() * 1000);
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

int sensor_hal_init(void)
{
    int ret;

    k_mutex_lock(&sensor_mutex, K_FOREVER);

    if (initialized) {
        LOG_WRN("Sensor HAL already initialized");
        k_mutex_unlock(&sensor_mutex);
        return SENSOR_STATUS_OK;
    }

    LOG_INF("Initializing sensor HAL...");

#ifdef CONFIG_SENSOR_USE_MOCK
    LOG_INF("Using mock accelerometer");
    ret = mock_accel_init();
#else
    /* Real sensor initialization would go here */
    /* For now, return error if mock is disabled */
    LOG_ERR("Real sensor driver not implemented");
    ret = -ENOTSUP;
#endif

    if (ret == 0) {
        initialized = true;
        stats.samples_read = 0;
        stats.read_errors = 0;
        stats.avg_sample_rate_hz = 0;
        stats.last_read_time_us = 0;
        LOG_INF("Sensor HAL initialized successfully");
    } else {
        LOG_ERR("Failed to initialize sensor (err %d)", ret);
    }

    k_mutex_unlock(&sensor_mutex);
    return (ret == 0) ? SENSOR_STATUS_OK : SENSOR_STATUS_ERROR;
}

int sensor_hal_read(struct accel_sample *sample)
{
    int ret;
    uint32_t now;

    if (sample == NULL) {
        return SENSOR_STATUS_ERROR;
    }

    if (!initialized) {
        LOG_ERR("Sensor not initialized");
        return SENSOR_STATUS_NOT_INITIALIZED;
    }

    k_mutex_lock(&sensor_mutex, K_FOREVER);

#ifdef CONFIG_SENSOR_USE_MOCK
    ret = mock_accel_read(sample);
#else
    ret = -ENOTSUP;
#endif

    if (ret == 0) {
        stats.samples_read++;
        
        /* Update timestamp */
        now = sensor_get_timestamp_us();
        sample->timestamp_us = now;
        
        /* Calculate average sample rate */
        if (last_sample_time > 0) {
            uint32_t interval = now - last_sample_time;
            sample_interval_sum += interval;
            sample_interval_count++;
            
            if (sample_interval_count >= 100) {
                uint32_t avg_interval = sample_interval_sum / sample_interval_count;
                if (avg_interval > 0) {
                    stats.avg_sample_rate_hz = 1000000 / avg_interval;
                }
                sample_interval_sum = 0;
                sample_interval_count = 0;
            }
        }
        
        last_sample_time = now;
        stats.last_read_time_us = now;
        
        LOG_DBG("Sample: x=%d, y=%d, z=%d", sample->x, sample->y, sample->z);
    } else {
        stats.read_errors++;
        LOG_WRN("Sensor read failed (err %d)", ret);
    }

    k_mutex_unlock(&sensor_mutex);
    
    return (ret == 0) ? SENSOR_STATUS_OK : SENSOR_STATUS_ERROR;
}

bool sensor_hal_data_ready(void)
{
    if (!initialized) {
        return false;
    }

#ifdef CONFIG_SENSOR_USE_MOCK
    return mock_accel_data_ready();
#else
    return false;
#endif
}

int sensor_hal_get_stats(struct sensor_stats *out_stats)
{
    if (out_stats == NULL) {
        return SENSOR_STATUS_ERROR;
    }

    k_mutex_lock(&sensor_mutex, K_FOREVER);
    *out_stats = stats;
    k_mutex_unlock(&sensor_mutex);

    return SENSOR_STATUS_OK;
}

void sensor_hal_reset_stats(void)
{
    k_mutex_lock(&sensor_mutex, K_FOREVER);
    
    stats.samples_read = 0;
    stats.read_errors = 0;
    stats.avg_sample_rate_hz = 0;
    stats.last_read_time_us = 0;
    
    last_sample_time = 0;
    sample_interval_sum = 0;
    sample_interval_count = 0;
    
    k_mutex_unlock(&sensor_mutex);
    
    LOG_INF("Sensor statistics reset");
}
