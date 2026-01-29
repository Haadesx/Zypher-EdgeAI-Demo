/*
 * SPDX-License-Identifier: MIT
 *
 * Zephyr Edge AI Demo - High-Resolution Timing Implementation
 *
 * Provides cycle-accurate timing measurements for inference profiling.
 */

#include "timing.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(timing, CONFIG_LOG_DEFAULT_LEVEL);

/* ============================================================================
 * Private Data
 * ============================================================================ */

static uint32_t cycles_per_us = 0;
static bool timing_initialized = false;

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void profile_timing_init(void)
{
    if (timing_initialized) {
        return;
    }
    
    uint32_t freq = sys_clock_hw_cycles_per_sec();
    cycles_per_us = freq / 1000000;
    
    if (cycles_per_us == 0) {
        cycles_per_us = 1;  /* Prevent division by zero */
        LOG_WRN("Low CPU frequency, timing may be inaccurate");
    }
    
    timing_initialized = true;
    
    LOG_INF("Timing initialized: %u cycles/us (CPU @ %u Hz)",
            cycles_per_us, freq);
}

uint32_t profile_timing_start(void)
{
    return k_cycle_get_32();
}

uint32_t profile_timing_end(uint32_t start_cycles)
{
    uint32_t end_cycles = k_cycle_get_32();
    uint32_t elapsed_cycles;
    
    /* Handle wrap-around */
    if (end_cycles >= start_cycles) {
        elapsed_cycles = end_cycles - start_cycles;
    } else {
        elapsed_cycles = (UINT32_MAX - start_cycles) + end_cycles + 1;
    }
    
    /* Convert to microseconds */
    if (cycles_per_us > 0) {
        return elapsed_cycles / cycles_per_us;
    }
    
    return elapsed_cycles;  /* Return cycles if conversion not possible */
}

void profile_timing_record(timing_stats_t *stats, uint32_t duration_us)
{
    if (stats == NULL) {
        return;
    }
    
    stats->count++;
    stats->total_us += duration_us;
    
    if (duration_us < stats->min_us || stats->min_us == 0) {
        stats->min_us = duration_us;
    }
    
    if (duration_us > stats->max_us) {
        stats->max_us = duration_us;
    }
    
    if (stats->count > 0) {
        stats->avg_us = (uint32_t)(stats->total_us / stats->count);
    }
}

void profile_timing_stats_reset(timing_stats_t *stats)
{
    if (stats == NULL) {
        return;
    }
    
    stats->min_us = 0;
    stats->max_us = 0;
    stats->avg_us = 0;
    stats->count = 0;
    stats->total_us = 0;
}

uint32_t profile_timing_get_us(void)
{
    return (uint32_t)(k_uptime_get() * 1000);
}
