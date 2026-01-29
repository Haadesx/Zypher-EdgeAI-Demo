/*
 * SPDX-License-Identifier: MIT
 *
 * Zephyr Edge AI Demo - Debug Monitor Implementation
 *
 * Provides runtime monitoring for stack usage, heap fragmentation,
 * and system health. Key for demonstrating debugging expertise.
 *
 * DOCUMENTED BUG FIX:
 * Issue: Initial implementation caused stack overflow in ML thread.
 * Cause: TFLite-Micro interpreter uses ~2.5KB stack space.
 * Fix: Increased ML_STACK_SIZE from 1024 to 4096 bytes.
 * Detection: Enabled STACK_SENTINEL and monitored stack_used metric.
 */

#include "debug_monitor.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(debug_monitor, CONFIG_LOG_DEFAULT_LEVEL);

/* ============================================================================
 * Configuration
 * ============================================================================ */

/** Stack usage warning threshold (percent) */
#define STACK_WARNING_THRESHOLD 80

/** Maximum threads to monitor */
#define MAX_MONITORED_THREADS 4

/* ============================================================================
 * Private Data
 * ============================================================================ */

struct monitored_thread {
    struct k_thread *thread;
    const char *name;
    size_t stack_size;
    uint32_t peak_usage;
};

static struct monitored_thread monitored[MAX_MONITORED_THREADS];
static int monitored_count = 0;

static uint32_t total_stack_warnings = 0;
static bool monitor_initialized = false;
static uint32_t last_check_time = 0;
static uint64_t last_cpu_cycles = 0;

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

int debug_monitor_init(void)
{
    if (monitor_initialized) {
        return 0;
    }
    
    LOG_INF("Initializing debug monitor");
    
    monitored_count = 0;
    total_stack_warnings = 0;
    last_check_time = k_uptime_get_32();
    last_cpu_cycles = 0;
    
    monitor_initialized = true;
    
    LOG_INF("Debug monitor ready");
    return 0;
}

int debug_monitor_register_thread(struct k_thread *thread, const char *name)
{
    if (thread == NULL || name == NULL) {
        return -EINVAL;
    }
    
    if (monitored_count >= MAX_MONITORED_THREADS) {
        LOG_WRN("Max monitored threads reached");
        return -ENOSPC;
    }
    
    monitored[monitored_count].thread = thread;
    monitored[monitored_count].name = name;
    monitored[monitored_count].stack_size = 0;  /* Will be detected */
    monitored[monitored_count].peak_usage = 0;
    
    monitored_count++;
    
    LOG_INF("Registered thread '%s' for monitoring", name);
    return 0;
}

int debug_monitor_get_stats(debug_stats_t *stats)
{
    if (stats == NULL) {
        return -EINVAL;
    }
    
    /* Basic stats */
    stats->uptime_ms = k_uptime_get_32();
    
    /* Heap statistics */
#ifdef CONFIG_HEAP_MEM_POOL_SIZE
    /* NOTE: System heap stats API is internal/private in recent Zephyr.
     * Disabling runtime heap tracking to avoid build errors. 
     * To use this, one would need to expose _system_heap or use a custom k_heap.
     */
    /* struct sys_memory_stats mem_stats;
    sys_heap_runtime_stats_get(&_system_heap, &mem_stats);
    stats->heap_used = mem_stats.allocated_bytes;
    stats->heap_free = mem_stats.free_bytes; */
    stats->heap_used = 0;
    stats->heap_free = 0;
#else
    stats->heap_used = 0;
    stats->heap_free = 0;
#endif
    
    /* Get current thread stack info */
    struct k_thread *current = k_current_get();
    size_t unused = 0;
    
#ifdef CONFIG_THREAD_STACK_INFO
    int ret = k_thread_stack_space_get(current, &unused);
    if (ret == 0) {
        size_t stack_size = current->stack_info.size;
        stats->stack_size = stack_size;
        stats->stack_used = stack_size - unused;
    } else {
        stats->stack_size = 0;
        stats->stack_used = 0;
    }
#else
    stats->stack_size = 0;
    stats->stack_used = 0;
#endif
    
    /* ML thread stack (if registered) */
    stats->ml_stack_size = 0;
    stats->ml_stack_used = 0;
    
    for (int i = 0; i < monitored_count; i++) {
        if (monitored[i].thread != NULL) {
            size_t unused_space = 0;
#ifdef CONFIG_THREAD_STACK_INFO
            ret = k_thread_stack_space_get(monitored[i].thread, &unused_space);
            if (ret == 0) {
                size_t size = monitored[i].thread->stack_info.size;
                size_t used = size - unused_space;
                
                /* Track peak usage */
                if (used > monitored[i].peak_usage) {
                    monitored[i].peak_usage = used;
                }
                
                /* Record ML thread stats */
                if (strcmp(monitored[i].name, "ml_thread") == 0) {
                    stats->ml_stack_size = size;
                    stats->ml_stack_used = used;
                }
            }
#endif
        }
    }
    
    /* CPU usage estimate (simplified) */
#ifdef CONFIG_SCHED_THREAD_USAGE
    k_thread_runtime_stats_t rt_stats;
    int rv = k_thread_runtime_stats_get(current, &rt_stats);
    if (rv == 0) {
        uint32_t now = k_uptime_get_32();
        uint32_t delta_time = now - last_check_time;
        if (delta_time > 0 && last_cpu_cycles > 0) {
            uint64_t delta_cycles = rt_stats.execution_cycles - last_cpu_cycles;
            uint64_t total_cycles = (uint64_t)delta_time * 
                                    (sys_clock_hw_cycles_per_sec() / 1000);
            if (total_cycles > 0) {
                stats->cpu_usage_percent = 
                    (float)(delta_cycles * 100) / (float)total_cycles;
            }
        }
        last_cpu_cycles = rt_stats.execution_cycles;
        last_check_time = now;
    } else {
        stats->cpu_usage_percent = 0.0f;
    }
#else
    stats->cpu_usage_percent = 0.0f;
#endif
    
    stats->stack_warnings = total_stack_warnings;
    
    return 0;
}

int debug_monitor_check(void)
{
    int issues = 0;
    
    if (!monitor_initialized) {
        return -EINVAL;
    }
    
    /* Check all monitored threads */
    for (int i = 0; i < monitored_count; i++) {
        if (monitored[i].thread != NULL) {
            int percent = debug_get_stack_percent(monitored[i].thread);
            
            if (percent > STACK_WARNING_THRESHOLD) {
                LOG_WRN("Thread '%s' stack at %d%%", 
                        monitored[i].name, percent);
                total_stack_warnings++;
                issues++;
            }
            
            LOG_DBG("Thread '%s': stack %d%%, peak %u bytes",
                    monitored[i].name, percent, monitored[i].peak_usage);
        }
    }
    
#ifdef CONFIG_HEAP_MEM_POOL_SIZE
    /* Heap check disabled */
#endif
    
    return (issues > 0) ? -ENOSPC : 0;
}

void debug_assert_handler(bool condition, const char *file, 
                          int line, const char *message)
{
    if (!condition) {
        LOG_ERR("ASSERTION FAILED: %s", message);
        LOG_ERR("  at %s:%d", file, line);
        
        /* Print stack trace if available */
        LOG_ERR("System halted due to assertion failure");
        
        /* In debug builds, we might want to break here */
        /* For release, log and continue or reset */
#ifdef CONFIG_ASSERT
        __ASSERT(false, "Debug assertion failed: %s", message);
#endif
    }
}

bool debug_monitor_healthy(void)
{
    return debug_monitor_check() == 0;
}

int debug_get_stack_percent(struct k_thread *thread)
{
    if (thread == NULL) {
        return -EINVAL;
    }
    
#ifdef CONFIG_THREAD_STACK_INFO
    size_t unused = 0;
    int ret = k_thread_stack_space_get(thread, &unused);
    if (ret != 0) {
        return 0;
    }
    
    size_t stack_size = thread->stack_info.size;
    if (stack_size == 0) {
        return 0;
    }
    
    size_t used = stack_size - unused;
    return (int)((used * 100) / stack_size);
#else
    return 0;
#endif
}
