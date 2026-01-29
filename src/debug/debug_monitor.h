/*
 * SPDX-License-Identifier: MIT
 *
 * Zephyr Edge AI Demo - Debug Monitor Header
 *
 * Defines the debug infrastructure for runtime monitoring.
 */

#ifndef DEBUG_MONITOR_H
#define DEBUG_MONITOR_H

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Runtime debug statistics
 */
typedef struct {
    /** System uptime in milliseconds */
    uint32_t uptime_ms;
    
    /** Heap memory used (bytes) */
    uint32_t heap_used;
    
    /** Heap memory free (bytes) */
    uint32_t heap_free;
    
    /** Main thread stack used (bytes) */
    uint32_t stack_used;
    
    /** Main thread stack size (bytes) */
    uint32_t stack_size;
    
    /** ML thread stack used (bytes) */
    uint32_t ml_stack_used;
    
    /** ML thread stack size (bytes) */
    uint32_t ml_stack_size;
    
    /** Approximate CPU usage (percent) */
    float cpu_usage_percent;
    
    /** Number of stack overflow warnings */
    uint32_t stack_warnings;
    
} debug_stats_t;

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/**
 * @brief Initialize the debug monitor
 *
 * Registers threads for monitoring and initializes counters.
 *
 * @return 0 on success
 */
int debug_monitor_init(void);

/**
 * @brief Collect current debug statistics
 *
 * @param stats Pointer to stats structure to fill
 * @return 0 on success
 */
int debug_monitor_get_stats(debug_stats_t *stats);

/**
 * @brief Run periodic monitoring check
 *
 * Should be called periodically (e.g., every 1 second) to
 * update statistics and check for issues.
 *
 * @return 0 if all checks pass, negative if issues detected
 */
int debug_monitor_check(void);

/**
 * @brief Register a thread for stack monitoring
 *
 * @param thread Thread to monitor
 * @param name Thread name for logging
 * @return 0 on success
 */
int debug_monitor_register_thread(struct k_thread *thread, const char *name);

/**
 * @brief Assert with logging
 *
 * Like assert() but logs the failure before stopping.
 *
 * @param condition Condition to check
 * @param file Source file
 * @param line Line number
 * @param message Failure message
 */
void debug_assert_handler(bool condition, const char *file, 
                          int line, const char *message);

/**
 * @brief Macro for debug assertions with automatic file/line
 */
#define DEBUG_ASSERT(cond, msg) \
    debug_assert_handler((cond), __FILE__, __LINE__, (msg))

/**
 * @brief Check if system is healthy
 *
 * @return true if no issues detected
 */
bool debug_monitor_healthy(void);

/**
 * @brief Get stack usage percentage for a thread
 *
 * @param thread Thread to check
 * @return Stack usage as percentage (0-100)
 */
int debug_get_stack_percent(struct k_thread *thread);

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_MONITOR_H */
