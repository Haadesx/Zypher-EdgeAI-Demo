/*
 * SPDX-License-Identifier: MIT
 *
 * Zephyr Edge AI Demo - High-Resolution Timing Header
 */

#ifndef TIMING_H
#define TIMING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Timing statistics
 */
typedef struct {
    uint32_t min_us;
    uint32_t max_us;
    uint32_t avg_us;
    uint32_t count;
    uint64_t total_us;
} timing_stats_t;

/**
 * @brief Initialize the timing module
 */
void profile_timing_init(void);

/**
 * @brief Start a timing measurement
 *
 * @return Start timestamp (cycles)
 */
uint32_t profile_timing_start(void);

/**
 * @brief End a timing measurement
 *
 * @param start_cycles Value returned from timing_start()
 * @return Elapsed time in microseconds
 */
uint32_t profile_timing_end(uint32_t start_cycles);

/**
 * @brief Record a timing measurement in stats
 *
 * @param stats Statistics structure to update
 * @param duration_us Measured duration
 */
void profile_timing_record(timing_stats_t *stats, uint32_t duration_us);

/**
 * @brief Reset timing statistics
 *
 * @param stats Statistics to reset
 */
void profile_timing_stats_reset(timing_stats_t *stats);

/**
 * @brief Get current timestamp in microseconds
 *
 * @return Microseconds since boot
 */
uint32_t profile_timing_get_us(void);

#ifdef __cplusplus
}
#endif

#endif /* TIMING_H */
