/*
 * SPDX-License-Identifier: MIT
 *
 * Zephyr Edge AI Demo - UART Protocol Header
 *
 * Defines the structured output protocol for inference results.
 */

#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include "inference.h"
#include "debug_monitor.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Output message types
 */
typedef enum {
    OUTPUT_TYPE_INFERENCE,      /* Inference result */
    OUTPUT_TYPE_DEBUG,          /* Debug information */
    OUTPUT_TYPE_HEARTBEAT,      /* Periodic heartbeat */
    OUTPUT_TYPE_ERROR,          /* Error message */
} output_type_t;

/**
 * @brief Complete output message
 */
typedef struct {
    output_type_t type;
    uint32_t timestamp_us;
    
    union {
        inference_result_t inference;
        debug_stats_t debug;
        struct {
            uint32_t uptime_ms;
            uint32_t inference_count;
        } heartbeat;
        struct {
            int code;
            const char *message;
        } error;
    } data;
} output_message_t;

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/**
 * @brief Initialize the UART output protocol
 *
 * @return 0 on success
 */
int uart_protocol_init(void);

/**
 * @brief Output an inference result
 *
 * Formats and sends the inference result via UART.
 *
 * @param result Inference result to output
 * @param debug Optional debug stats (can be NULL)
 */
void uart_output_inference(const inference_result_t *result,
                           const debug_stats_t *debug);

/**
 * @brief Output debug information
 *
 * @param stats Debug statistics
 */
void uart_output_debug(const debug_stats_t *stats);

/**
 * @brief Output heartbeat message
 *
 * Periodic message indicating the system is alive.
 */
void uart_output_heartbeat(void);

/**
 * @brief Output error message
 *
 * @param code Error code
 * @param message Error description
 */
void uart_output_error(int code, const char *message);

/**
 * @brief Output startup banner
 */
void uart_output_banner(void);

#ifdef __cplusplus
}
#endif

#endif /* UART_PROTOCOL_H */
