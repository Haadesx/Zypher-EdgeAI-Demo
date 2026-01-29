/*
 * SPDX-License-Identifier: MIT
 *
 * Zephyr Edge AI Demo - UART Protocol Implementation
 *
 * Implements structured JSON output for inference results and debug info.
 * Designed for easy parsing by Python analysis scripts.
 */

#include "uart_protocol.h"
#include "ring_buffer.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <stdio.h>
#include <string.h>

LOG_MODULE_REGISTER(uart_protocol, CONFIG_LOG_DEFAULT_LEVEL);

/* ============================================================================
 * Configuration
 * ============================================================================ */

/** Maximum output line length */
#define MAX_OUTPUT_LEN 256

/** Application version */
#define APP_VERSION "1.0.0"

/* ============================================================================
 * Private Data
 * ============================================================================ */

static bool initialized = false;
static uint32_t output_sequence = 0;

/* ============================================================================
 * Private Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in microseconds
 */
static uint32_t get_timestamp_us(void)
{
    return (uint32_t)(k_uptime_get() * 1000);
}

/**
 * @brief Output a line (adds newline)
 */
static void output_line(const char *line)
{
    printk("%s\n", line);
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

int uart_protocol_init(void)
{
    if (initialized) {
        return 0;
    }
    
    output_sequence = 0;
    initialized = true;
    
    LOG_INF("UART protocol initialized");
    return 0;
}

void uart_output_inference(const inference_result_t *result,
                           const debug_stats_t *debug)
{
    char buf[MAX_OUTPUT_LEN];
    int len;
    
    if (!initialized || result == NULL) {
        return;
    }
    
    output_sequence++;
    
#ifdef CONFIG_OUTPUT_JSON_FORMAT
    /* JSON format for easy parsing */
    if (debug != NULL) {
        len = snprintf(buf, sizeof(buf),
            "{\"type\":\"inference\","
            "\"seq\":%u,"
            "\"ts\":%u,"
            "\"gesture\":\"%s\","
            "\"conf\":%.3f,"
            "\"latency_us\":%u,"
            "\"heap\":%u,"
            "\"stack\":%u}",
            output_sequence,
            result->timestamp_us,
            ml_gesture_to_string(result->gesture),
            (double)result->confidence,
            result->inference_time_us,
            debug->heap_used,
            debug->stack_used);
    } else {
        len = snprintf(buf, sizeof(buf),
            "{\"type\":\"inference\","
            "\"seq\":%u,"
            "\"ts\":%u,"
            "\"gesture\":\"%s\","
            "\"conf\":%.3f,"
            "\"latency_us\":%u}",
            output_sequence,
            result->timestamp_us,
            ml_gesture_to_string(result->gesture),
            (double)result->confidence,
            result->inference_time_us);
    }
#else
    /* Human-readable format */
    len = snprintf(buf, sizeof(buf),
        "[%u] GESTURE: %s (conf=%.2f, lat=%uus)",
        output_sequence,
        ml_gesture_to_string(result->gesture),
        (double)result->confidence,
        result->inference_time_us);
#endif
    
    (void)len;  /* Suppress unused warning */
    output_line(buf);
}

void uart_output_debug(const debug_stats_t *stats)
{
    char buf[MAX_OUTPUT_LEN];
    
    if (!initialized || stats == NULL) {
        return;
    }
    
#ifdef CONFIG_OUTPUT_JSON_FORMAT
    snprintf(buf, sizeof(buf),
        "{\"type\":\"debug\","
        "\"ts\":%u,"
        "\"uptime_ms\":%u,"
        "\"heap_used\":%u,"
        "\"heap_free\":%u,"
        "\"stack_used\":%u,"
        "\"stack_size\":%u,"
        "\"cpu_usage\":%.1f}",
        (uint32_t)(k_uptime_get() * 1000),
        stats->uptime_ms,
        stats->heap_used,
        stats->heap_free,
        stats->stack_used,
        stats->stack_size,
        (double)stats->cpu_usage_percent);
#else
    snprintf(buf, sizeof(buf),
        "[DEBUG] Heap: %u/%u, Stack: %u/%u, CPU: %.1f%%",
        stats->heap_used,
        stats->heap_used + stats->heap_free,
        stats->stack_used,
        stats->stack_size,
        (double)stats->cpu_usage_percent);
#endif
    
    output_line(buf);
}

void uart_output_heartbeat(void)
{
    char buf[MAX_OUTPUT_LEN];
    
    if (!initialized) {
        return;
    }
    
#ifdef CONFIG_OUTPUT_JSON_FORMAT
    snprintf(buf, sizeof(buf),
        "{\"type\":\"heartbeat\","
        "\"ts\":%u,"
        "\"uptime_ms\":%llu}",
        get_timestamp_us(),
        k_uptime_get());
#else
    snprintf(buf, sizeof(buf),
        "[HEARTBEAT] Uptime: %llu ms",
        k_uptime_get());
#endif
    
    output_line(buf);
}

void uart_output_error(int code, const char *message)
{
    char buf[MAX_OUTPUT_LEN];
    
    if (!initialized) {
        return;
    }
    
#ifdef CONFIG_OUTPUT_JSON_FORMAT
    snprintf(buf, sizeof(buf),
        "{\"type\":\"error\","
        "\"ts\":%u,"
        "\"code\":%d,"
        "\"message\":\"%s\"}",
        get_timestamp_us(),
        code,
        message ? message : "unknown");
#else
    snprintf(buf, sizeof(buf),
        "[ERROR] Code %d: %s",
        code,
        message ? message : "unknown");
#endif
    
    output_line(buf);
}

void uart_output_banner(void)
{
    char buf[MAX_OUTPUT_LEN];
    
    printk("\n");
    printk("╔══════════════════════════════════════════════════════════╗\n");
    printk("║     Zephyr Edge AI Demo - Gesture Recognition           ║\n");
    printk("║     Version: %-10s                                  ║\n", APP_VERSION);
    printk("╚══════════════════════════════════════════════════════════╝\n");
    printk("\n");
    
#ifdef CONFIG_OUTPUT_JSON_FORMAT
    snprintf(buf, sizeof(buf),
        "{\"type\":\"startup\","
        "\"version\":\"%s\","
        "\"board\":\"%s\","
        "\"ts\":%u}",
        APP_VERSION,
        CONFIG_BOARD,
        get_timestamp_us());
    output_line(buf);
#endif
    
    LOG_INF("UART output initialized");
}
