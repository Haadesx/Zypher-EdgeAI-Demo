/*
 * SPDX-License-Identifier: MIT
 *
 * Zephyr Edge AI Demo - Mock Accelerometer
 *
 * This file implements a simulated accelerometer that generates
 * realistic gesture patterns for testing without real hardware.
 * Designed to run on QEMU or any platform without a physical sensor.
 *
 * Gesture Patterns Generated:
 *   - IDLE: Small random noise around baseline
 *   - WAVE: Sinusoidal motion on X/Y axes
 *   - TAP:  Sharp impulse followed by decay
 *   - CIRCLE: Circular motion pattern
 */

#include "sensor_hal.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <math.h>

LOG_MODULE_REGISTER(mock_accel, CONFIG_LOG_DEFAULT_LEVEL);

/* ============================================================================
 * Configuration
 * ============================================================================ */

#ifndef CONFIG_SENSOR_SAMPLE_RATE_HZ
#define CONFIG_SENSOR_SAMPLE_RATE_HZ 100
#endif

#ifndef CONFIG_SENSOR_MOCK_GESTURE_INTERVAL_MS
#define CONFIG_SENSOR_MOCK_GESTURE_INTERVAL_MS 3000
#endif

/** Duration of gesture patterns in milliseconds */
#define GESTURE_DURATION_MS 500

/** Noise amplitude for idle state */
#define NOISE_AMPLITUDE 100

/** Gesture amplitude (raw units, ~0.5g at 2g range) */
#define GESTURE_AMPLITUDE 4000

/** Gravity offset for Z-axis (1g at 2g range = 8192) */
#define GRAVITY_OFFSET 8192

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

typedef enum {
    MOCK_GESTURE_IDLE = 0,
    MOCK_GESTURE_WAVE,
    MOCK_GESTURE_TAP,
    MOCK_GESTURE_CIRCLE,
    MOCK_GESTURE_COUNT
} mock_gesture_t;

/* ============================================================================
 * Private Data
 * ============================================================================ */

static bool mock_initialized = false;
static mock_gesture_t current_gesture = MOCK_GESTURE_IDLE;
static uint32_t gesture_start_time = 0;
static uint32_t next_gesture_time = 0;
static uint32_t gesture_sequence_index = 0;

/** Sample timing */
static uint32_t last_sample_time = 0;
static const uint32_t sample_period_us = 1000000 / CONFIG_SENSOR_SAMPLE_RATE_HZ;

/* ============================================================================
 * Private Functions
 * ============================================================================ */

/**
 * @brief Generate random noise in range [-amplitude, +amplitude]
 */
static int16_t generate_noise(int16_t amplitude)
{
    uint32_t rand_val = sys_rand32_get();
    int32_t noise = (int32_t)(rand_val % (2 * amplitude + 1)) - amplitude;
    return (int16_t)noise;
}

/**
 * @brief Select next gesture type (cycles through all gestures for demo)
 */
static mock_gesture_t select_next_gesture(void)
{
    /* Cycle through gestures: WAVE, TAP, CIRCLE, then back to WAVE */
    gesture_sequence_index = (gesture_sequence_index + 1) % (MOCK_GESTURE_COUNT - 1);
    return (mock_gesture_t)(gesture_sequence_index + 1);  /* Skip IDLE */
}

/**
 * @brief Generate idle/baseline accelerometer data
 */
static void generate_idle(struct accel_sample *sample)
{
    sample->x = generate_noise(NOISE_AMPLITUDE);
    sample->y = generate_noise(NOISE_AMPLITUDE);
    sample->z = GRAVITY_OFFSET + generate_noise(NOISE_AMPLITUDE);
}

/**
 * @brief Generate wave gesture (side-to-side motion)
 */
static void generate_wave(struct accel_sample *sample, uint32_t elapsed_ms)
{
    /* Sinusoidal wave on X-axis, smaller Y component */
    float phase = (float)elapsed_ms / GESTURE_DURATION_MS * 4.0f * 3.14159f;
    float envelope = 1.0f - ((float)elapsed_ms / GESTURE_DURATION_MS);
    
    sample->x = (int16_t)(sinf(phase) * GESTURE_AMPLITUDE * envelope);
    sample->y = (int16_t)(cosf(phase * 0.5f) * GESTURE_AMPLITUDE * 0.3f * envelope);
    sample->z = GRAVITY_OFFSET + generate_noise(NOISE_AMPLITUDE);
}

/**
 * @brief Generate tap gesture (sharp impulse)
 */
static void generate_tap(struct accel_sample *sample, uint32_t elapsed_ms)
{
    /* Sharp spike followed by exponential decay with oscillation */
    float t = (float)elapsed_ms / GESTURE_DURATION_MS;
    float decay = expf(-t * 8.0f);
    float oscillation = sinf(t * 30.0f);
    
    sample->x = generate_noise(NOISE_AMPLITUDE);
    sample->y = (int16_t)(GESTURE_AMPLITUDE * 1.5f * decay * oscillation);
    sample->z = GRAVITY_OFFSET + (int16_t)(GESTURE_AMPLITUDE * 0.5f * decay);
}

/**
 * @brief Generate circle gesture (circular motion)
 */
static void generate_circle(struct accel_sample *sample, uint32_t elapsed_ms)
{
    /* Circular pattern on X-Y plane */
    float phase = (float)elapsed_ms / GESTURE_DURATION_MS * 2.0f * 3.14159f;
    float envelope = sinf((float)elapsed_ms / GESTURE_DURATION_MS * 3.14159f);
    
    sample->x = (int16_t)(cosf(phase) * GESTURE_AMPLITUDE * envelope);
    sample->y = (int16_t)(sinf(phase) * GESTURE_AMPLITUDE * envelope);
    sample->z = GRAVITY_OFFSET + generate_noise(NOISE_AMPLITUDE);
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

int mock_accel_init(void)
{
    LOG_INF("Initializing mock accelerometer");
    LOG_INF("  Sample rate: %d Hz", CONFIG_SENSOR_SAMPLE_RATE_HZ);
    LOG_INF("  Gesture interval: %d ms", CONFIG_SENSOR_MOCK_GESTURE_INTERVAL_MS);
    
    mock_initialized = true;
    current_gesture = MOCK_GESTURE_IDLE;
    gesture_start_time = 0;
    next_gesture_time = k_uptime_get_32() + CONFIG_SENSOR_MOCK_GESTURE_INTERVAL_MS;
    gesture_sequence_index = 0;
    last_sample_time = 0;
    
    LOG_INF("Mock accelerometer ready");
    return 0;
}

int mock_accel_read(struct accel_sample *sample)
{
    uint32_t now;
    uint32_t elapsed_ms;
    
    if (!mock_initialized) {
        return -ENODEV;
    }
    
    if (sample == NULL) {
        return -EINVAL;
    }
    
    now = k_uptime_get_32();
    
    /* Check if we should start a new gesture */
    if (current_gesture == MOCK_GESTURE_IDLE && now >= next_gesture_time) {
        current_gesture = select_next_gesture();
        gesture_start_time = now;
        LOG_INF("Starting gesture: %s", 
                current_gesture == MOCK_GESTURE_WAVE ? "WAVE" :
                current_gesture == MOCK_GESTURE_TAP ? "TAP" :
                current_gesture == MOCK_GESTURE_CIRCLE ? "CIRCLE" : "UNKNOWN");
    }
    
    /* Calculate elapsed time in current gesture */
    elapsed_ms = now - gesture_start_time;
    
    /* Check if gesture has completed */
    if (current_gesture != MOCK_GESTURE_IDLE && elapsed_ms >= GESTURE_DURATION_MS) {
        LOG_INF("Gesture complete");
        current_gesture = MOCK_GESTURE_IDLE;
        next_gesture_time = now + CONFIG_SENSOR_MOCK_GESTURE_INTERVAL_MS;
    }
    
    /* Generate sample based on current gesture */
    switch (current_gesture) {
        case MOCK_GESTURE_WAVE:
            generate_wave(sample, elapsed_ms);
            break;
        case MOCK_GESTURE_TAP:
            generate_tap(sample, elapsed_ms);
            break;
        case MOCK_GESTURE_CIRCLE:
            generate_circle(sample, elapsed_ms);
            break;
        case MOCK_GESTURE_IDLE:
        default:
            generate_idle(sample);
            break;
    }
    
    return 0;
}

bool mock_accel_data_ready(void)
{
    if (!mock_initialized) {
        return false;
    }
    
    uint32_t now = sensor_get_timestamp_us();
    
    if (now - last_sample_time >= sample_period_us) {
        last_sample_time = now;
        return true;
    }
    
    return false;
}
