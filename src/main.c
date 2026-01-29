/*
 * SPDX-License-Identifier: MIT
 *
 * Zephyr Edge AI Demo - Main Application
 *
 * This is the main entry point for the gesture recognition demo.
 * It implements a multi-threaded architecture with separate threads for:
 *   - Sensor sampling (100 Hz)
 *   - ML inference (triggered when window is ready)
 *   - Debug monitoring (1 Hz)
 *
 * Architecture:
 *   ┌─────────────────┐     ┌─────────────────┐
 *   │  Sensor Thread  │────▶│  Preprocessing  │
 *   │    (100 Hz)     │     │  Window Buffer  │
 *   └─────────────────┘     └────────┬────────┘
 *                                    │
 *                                    ▼
 *                           ┌─────────────────┐
 *                           │   ML Thread     │
 *                           │   (on demand)   │
 *                           └────────┬────────┘
 *                                    │
 *                                    ▼
 *                           ┌─────────────────┐
 *                           │  UART Output    │
 *                           │  (JSON format)  │
 *                           └─────────────────┘
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

/* Application headers */
#include "sensor/sensor_hal.h"
#include "ml/inference.h"
#include "ml/preprocessing.h"
#include "output/uart_protocol.h"
#include "output/ring_buffer.h"
#include "debug/debug_monitor.h"
#include "debug/timing.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

/* ============================================================================
 * Configuration
 * ============================================================================ */

/** Sensor sampling period in milliseconds */
#define SENSOR_SAMPLE_PERIOD_MS (1000 / CONFIG_SENSOR_SAMPLE_RATE_HZ)

/** Debug monitor period in milliseconds */
#define DEBUG_MONITOR_PERIOD_MS CONFIG_DEBUG_MONITOR_INTERVAL_MS

/** Stack sizes - IMPORTANT: See DEBUGGING.md for stack overflow fix */
#define SENSOR_STACK_SIZE 1024
#define ML_STACK_SIZE     4096  /* Increased from 1024 due to TFLite stack usage */
#define OUTPUT_STACK_SIZE 2048  /* Increased from 1024 for logging/UART output */
#define DEBUG_STACK_SIZE  2048  /* Increased from 1024 due to deep stack usage */

/** Thread priorities (lower number = higher priority) */
#define SENSOR_PRIORITY   5
#define ML_PRIORITY       7
#define OUTPUT_PRIORITY   9
#define DEBUG_PRIORITY    11

/* ============================================================================
 * Thread Stacks
 * ============================================================================ */

K_THREAD_STACK_DEFINE(sensor_stack, SENSOR_STACK_SIZE);
K_THREAD_STACK_DEFINE(ml_stack, ML_STACK_SIZE);
K_THREAD_STACK_DEFINE(output_stack, OUTPUT_STACK_SIZE);
K_THREAD_STACK_DEFINE(debug_stack, DEBUG_STACK_SIZE);

/* ============================================================================
 * Thread Data
 * ============================================================================ */

static struct k_thread sensor_thread_data;
static struct k_thread ml_thread_data;
static struct k_thread output_thread_data;
static struct k_thread debug_thread_data;

/** Semaphore to signal ML thread that window is ready */
K_SEM_DEFINE(ml_sem, 0, 1);

/** Flag to signal threads to exit */
static volatile bool running = true;

/* ============================================================================
 * Sensor Thread
 *
 * Continuously samples the accelerometer at the configured rate and
 * feeds samples to the preprocessing module.
 * ============================================================================ */

static void sensor_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    struct accel_sample sample;
    int ret;
    uint32_t sample_count = 0;
    
    LOG_INF("Sensor thread started (period: %d ms)", SENSOR_SAMPLE_PERIOD_MS);
    
    while (running) {
        /* Read sensor sample */
        ret = sensor_hal_read(&sample);
        
        if (ret == SENSOR_STATUS_OK) {
            /* Add to preprocessing window */
            preprocessing_add_sample(&sample);
            sample_count++;
            
            /* Check if window is ready for inference */
            if (preprocessing_window_ready()) {
                LOG_DBG("Window ready, signaling ML thread");
                k_sem_give(&ml_sem);
            }
        } else {
            LOG_WRN("Sensor read failed: %d", ret);
        }
        
        /* Sleep until next sample */
        k_msleep(SENSOR_SAMPLE_PERIOD_MS);
    }
    
    LOG_INF("Sensor thread exiting (samples: %u)", sample_count);
}

/* ============================================================================
 * ML Inference Thread
 *
 * Waits for the preprocessing window to fill, then runs inference
 * and queues the result for output.
 * ============================================================================ */

static void ml_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    int8_t input_buffer[ML_INPUT_SIZE];
    inference_result_t result;
    int ret;
    
    LOG_INF("ML thread started");
    
    while (running) {
        /* Wait for window to be ready */
        ret = k_sem_take(&ml_sem, K_MSEC(1000));
        
        if (ret == 0) {
            /* Get preprocessed input */
            ret = preprocessing_get_input(input_buffer, sizeof(input_buffer));
            
            if (ret == 0) {
                /* Run inference */
                ret = ml_run_inference(input_buffer, &result);
                
                if (ret == ML_STATUS_OK) {
                    LOG_INF("Detected: %s (%.2f) in %u us",
                            ml_gesture_to_string(result.gesture),
                            (double)result.confidence,
                            result.inference_time_us);
                    
                    /* Queue result for output */
                    result_buffer_push(&result);
                } else {
                    LOG_ERR("Inference failed: %d", ret);
                }
            } else {
                LOG_WRN("Failed to get preprocessed input: %d", ret);
            }
        }
        /* Timeout is normal - just keep waiting */
    }
    
    LOG_INF("ML thread exiting");
}

/* ============================================================================
 * Output Thread
 *
 * Drains the result buffer and outputs results via UART.
 * ============================================================================ */

static void output_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    inference_result_t result;
    debug_stats_t debug_stats;
    int ret;
    
    LOG_INF("Output thread started");
    
    while (running) {
        /* Check for results to output */
        ret = result_buffer_pop(&result);
        
        if (ret == 0) {
            /* Get current debug stats */
            debug_monitor_get_stats(&debug_stats);
            
            /* Output the result */
            uart_output_inference(&result, &debug_stats);
        }
        
        /* Small sleep to prevent busy-waiting */
        k_msleep(10);
    }
    
    LOG_INF("Output thread exiting");
}

/* ============================================================================
 * Debug Monitor Thread
 *
 * Periodically collects and outputs system health statistics.
 * ============================================================================ */

static void debug_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    debug_stats_t stats;
    ml_stats_t ml_stats;
    int check_result;
    
    LOG_INF("Debug thread started (period: %d ms)", DEBUG_MONITOR_PERIOD_MS);
    
    while (running) {
        /* Run health checks */
        check_result = debug_monitor_check();
        
        if (check_result != 0) {
            LOG_WRN("Health check detected issues");
        }
        
        /* Collect and output debug stats */
        debug_monitor_get_stats(&stats);
        
        /* Also get ML stats */
        ml_get_stats(&ml_stats);
        
        LOG_INF("Stats: heap=%u/%u, stack=%u/%u, inferences=%u",
                stats.heap_used, stats.heap_used + stats.heap_free,
                stats.stack_used, stats.stack_size,
                ml_stats.inference_count);
        
#ifdef CONFIG_DEBUG_MONITOR_ENABLE
        uart_output_debug(&stats);
#endif
        
        k_msleep(DEBUG_MONITOR_PERIOD_MS);
    }
    
    LOG_INF("Debug thread exiting");
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

int main(void)
{
    int ret;
    
    /* Print startup banner */
    uart_protocol_init();
    uart_output_banner();
    
    LOG_INF("Zephyr Edge AI Demo starting...");
    LOG_INF("Board: %s", CONFIG_BOARD);
    
    /* Initialize timing subsystem */
    profile_timing_init();
    
    /* Initialize debug monitor */
    ret = debug_monitor_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize debug monitor: %d", ret);
    }
    
    /* Initialize result buffer */
    result_buffer_init();
    
    /* Initialize sensor HAL */
    LOG_INF("Initializing sensor...");
    ret = sensor_hal_init();
    if (ret != SENSOR_STATUS_OK) {
        LOG_ERR("Failed to initialize sensor: %d", ret);
        uart_output_error(ret, "Sensor init failed");
        return -1;
    }
    
    /* Initialize preprocessing */
    preprocessing_init();
    
    /* Initialize ML inference engine */
    LOG_INF("Initializing ML inference engine...");
    ret = ml_inference_init();
    if (ret != ML_STATUS_OK) {
        LOG_ERR("Failed to initialize ML engine: %d", ret);
        uart_output_error(ret, "ML init failed");
        return -1;
    }
    
    LOG_INF("Tensor arena used: %zu bytes", ml_get_arena_used());
    
    /* Create sensor thread */
    k_thread_create(&sensor_thread_data, sensor_stack,
                    K_THREAD_STACK_SIZEOF(sensor_stack),
                    sensor_thread_fn,
                    NULL, NULL, NULL,
                    SENSOR_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&sensor_thread_data, "sensor");
    
    /* Create ML thread */
    k_thread_create(&ml_thread_data, ml_stack,
                    K_THREAD_STACK_SIZEOF(ml_stack),
                    ml_thread_fn,
                    NULL, NULL, NULL,
                    ML_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&ml_thread_data, "ml_thread");
    
    /* Register ML thread for stack monitoring */
    debug_monitor_register_thread(&ml_thread_data, "ml_thread");
    
    /* Create output thread */
    k_thread_create(&output_thread_data, output_stack,
                    K_THREAD_STACK_SIZEOF(output_stack),
                    output_thread_fn,
                    NULL, NULL, NULL,
                    OUTPUT_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&output_thread_data, "output");
    
    /* Create debug thread */
    k_thread_create(&debug_thread_data, debug_stack,
                    K_THREAD_STACK_SIZEOF(debug_stack),
                    debug_thread_fn,
                    NULL, NULL, NULL,
                    DEBUG_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&debug_thread_data, "debug");
    
    LOG_INF("All threads started successfully");
    LOG_INF("System ready - waiting for gestures...");
    
    /* Main thread can now sleep or monitor */
    while (running) {
        k_sleep(K_SECONDS(10));
        
        /* Periodic heartbeat from main */
        uart_output_heartbeat();
    }
    
    return 0;
}
