/*
 * SPDX-License-Identifier: MIT
 *
 * Zephyr Edge AI Demo - Sensor Hardware Abstraction Layer
 *
 * This header defines the interface for accelerometer sensor access,
 * providing a clean abstraction between the application and either
 * real hardware or mock sensor implementations.
 */

#ifndef SENSOR_HAL_H
#define SENSOR_HAL_H

#include <zephyr/kernel.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Accelerometer sample data structure
 *
 * Contains 3-axis acceleration data with timestamp.
 * Values are in raw sensor units (typically mg or LSB).
 */
struct accel_sample {
    /** X-axis acceleration (signed, raw units) */
    int16_t x;
    /** Y-axis acceleration (signed, raw units) */
    int16_t y;
    /** Z-axis acceleration (signed, raw units) */
    int16_t z;
    /** Timestamp in microseconds since boot */
    uint32_t timestamp_us;
};

/**
 * @brief Sensor status codes
 */
enum sensor_status {
    SENSOR_STATUS_OK = 0,
    SENSOR_STATUS_NOT_INITIALIZED,
    SENSOR_STATUS_NOT_READY,
    SENSOR_STATUS_ERROR,
    SENSOR_STATUS_TIMEOUT,
};

/**
 * @brief Sensor statistics for debugging
 */
struct sensor_stats {
    /** Total samples read since initialization */
    uint32_t samples_read;
    /** Number of read errors */
    uint32_t read_errors;
    /** Average sample rate (calculated) */
    uint32_t avg_sample_rate_hz;
    /** Time of last successful read (us since boot) */
    uint32_t last_read_time_us;
};

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/**
 * @brief Initialize the sensor hardware abstraction layer
 *
 * This function initializes the underlying sensor driver (real or mock)
 * and prepares it for sampling. Must be called before any other sensor
 * functions.
 *
 * @return SENSOR_STATUS_OK on success, error code otherwise
 */
int sensor_hal_init(void);

/**
 * @brief Read a single accelerometer sample
 *
 * Reads the current accelerometer values and populates the sample
 * structure. If no new data is available, returns SENSOR_STATUS_NOT_READY.
 *
 * @param[out] sample Pointer to sample structure to fill
 * @return SENSOR_STATUS_OK on success, error code otherwise
 */
int sensor_hal_read(struct accel_sample *sample);

/**
 * @brief Check if new sensor data is available
 *
 * Non-blocking check for data availability.
 *
 * @return true if new data is ready, false otherwise
 */
bool sensor_hal_data_ready(void);

/**
 * @brief Get sensor statistics
 *
 * Retrieves accumulated statistics about sensor operation.
 *
 * @param[out] stats Pointer to stats structure to fill
 * @return SENSOR_STATUS_OK on success, error code otherwise
 */
int sensor_hal_get_stats(struct sensor_stats *stats);

/**
 * @brief Reset sensor statistics
 *
 * Clears all accumulated statistics counters.
 */
void sensor_hal_reset_stats(void);

/**
 * @brief Convert raw sample to floating point (g)
 *
 * Utility function to convert raw sensor values to g-force.
 * Assumes typical accelerometer sensitivity.
 *
 * @param raw Raw sensor value
 * @return Acceleration in g
 */
static inline float sensor_raw_to_g(int16_t raw)
{
    /* Assuming 2g range with 16-bit signed resolution */
    return (float)raw / 16384.0f;
}

/**
 * @brief Get current timestamp in microseconds
 *
 * Returns a monotonic timestamp suitable for sample timestamping.
 *
 * @return Current time in microseconds since boot
 */
uint32_t sensor_get_timestamp_us(void);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_HAL_H */
