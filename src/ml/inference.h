/*
 * SPDX-License-Identifier: MIT
 *
 * Zephyr Edge AI Demo - ML Inference Interface
 *
 * This header defines the interface for the TensorFlow Lite Micro
 * inference engine used for gesture recognition.
 */

#ifndef INFERENCE_H
#define INFERENCE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Number of input axes (X, Y, Z) */
#define ML_INPUT_AXES 3

/** Number of samples per inference window */
#ifndef CONFIG_ML_INFERENCE_WINDOW_SIZE
#define CONFIG_ML_INFERENCE_WINDOW_SIZE 50
#endif

/** Total input size */
#define ML_INPUT_SIZE (ML_INPUT_AXES * CONFIG_ML_INFERENCE_WINDOW_SIZE)

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Gesture classification labels
 */
typedef enum {
    GESTURE_IDLE = 0,
    GESTURE_WAVE,
    GESTURE_TAP,
    GESTURE_CIRCLE,
    GESTURE_COUNT  /* Number of gesture classes */
} gesture_label_t;

/**
 * @brief Inference result structure
 */
typedef struct {
    /** Detected gesture class */
    gesture_label_t gesture;
    
    /** Confidence score (0.0 - 1.0) */
    float confidence;
    
    /** Confidence scores for all classes */
    float class_scores[GESTURE_COUNT];
    
    /** Inference duration in microseconds */
    uint32_t inference_time_us;
    
    /** Timestamp of inference completion */
    uint32_t timestamp_us;
    
    /** Sequence number */
    uint32_t sequence;
} inference_result_t;

/**
 * @brief Inference engine status codes
 */
typedef enum {
    ML_STATUS_OK = 0,
    ML_STATUS_NOT_INITIALIZED,
    ML_STATUS_ALLOC_FAILED,
    ML_STATUS_INVOKE_FAILED,
    ML_STATUS_INVALID_INPUT,
    ML_STATUS_ERROR,
} ml_status_t;

/**
 * @brief Inference engine statistics
 */
typedef struct {
    /** Total inferences run */
    uint32_t inference_count;
    
    /** Minimum inference time (us) */
    uint32_t min_time_us;
    
    /** Maximum inference time (us) */
    uint32_t max_time_us;
    
    /** Cumulative inference time (for average) */
    uint64_t total_time_us;
    
    /** Number of invoke failures */
    uint32_t invoke_failures;
} ml_stats_t;

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/**
 * @brief Initialize the ML inference engine
 *
 * Loads the gesture recognition model and allocates tensors.
 * Must be called once before running any inferences.
 *
 * @return ML_STATUS_OK on success, error code otherwise
 */
ml_status_t ml_inference_init(void);

/**
 * @brief Run inference on input data
 *
 * Processes a window of accelerometer samples and produces
 * a gesture classification result.
 *
 * @param[in]  input_data  Array of preprocessed input values
 *                         (size: ML_INPUT_SIZE, INT8 format)
 * @param[out] result      Pointer to result structure
 * @return ML_STATUS_OK on success, error code otherwise
 */
ml_status_t ml_run_inference(const int8_t *input_data, inference_result_t *result);

/**
 * @brief Get inference engine statistics
 *
 * @param[out] stats Pointer to statistics structure
 * @return ML_STATUS_OK on success
 */
ml_status_t ml_get_stats(ml_stats_t *stats);

/**
 * @brief Reset inference statistics
 */
void ml_reset_stats(void);

/**
 * @brief Get human-readable gesture name
 *
 * @param gesture Gesture label
 * @return String representation
 */
const char *ml_gesture_to_string(gesture_label_t gesture);

/**
 * @brief Get tensor arena usage
 *
 * Returns the amount of tensor arena memory actually used
 * by the model, useful for tuning CONFIG_ML_TENSOR_ARENA_SIZE.
 *
 * @return Used bytes, or 0 if not initialized
 */
size_t ml_get_arena_used(void);

/**
 * @brief Check if inference engine is ready
 *
 * @return true if initialized and ready for inference
 */
bool ml_is_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* INFERENCE_H */
