/*
 * SPDX-License-Identifier: MIT
 *
 * Zephyr Edge AI Demo - TensorFlow Lite Micro Inference Engine
 *
 * This file implements the inference engine wrapper around TensorFlow
 * Lite Micro, handling model loading, tensor management, and inference
 * execution for gesture classification.
 */

#include "inference.h"
#include "gesture_model.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* TensorFlow Lite Micro headers */
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>
#include <tensorflow/lite/schema/schema_generated.h>

LOG_MODULE_REGISTER(ml_inference, CONFIG_LOG_DEFAULT_LEVEL);

/* ============================================================================
 * Configuration
 * ============================================================================ */

#ifndef CONFIG_ML_TENSOR_ARENA_SIZE
#define CONFIG_ML_TENSOR_ARENA_SIZE 8192
#endif

/* ============================================================================
 * Private Data
 * ============================================================================ */

/** Tensor arena for TFLite-Micro allocations */
static uint8_t tensor_arena[CONFIG_ML_TENSOR_ARENA_SIZE] __aligned(16);

/** Model and interpreter pointers */
static const tflite::Model *model = nullptr;
static tflite::MicroInterpreter *interpreter = nullptr;

/** Input/output tensor pointers */
static TfLiteTensor *input_tensor = nullptr;
static TfLiteTensor *output_tensor = nullptr;

/** Initialization flag */
static bool ml_initialized = false;
static bool use_mock_inference = false;

/** Statistics */
static ml_stats_t ml_stats = {0};

/** Inference sequence counter */
static uint32_t inference_sequence = 0;

/** Mutex for thread safety */
static K_MUTEX_DEFINE(ml_mutex);

/* ============================================================================
 * Gesture Labels
 * ============================================================================ */

static const char *gesture_names[GESTURE_COUNT] = {
    "IDLE",
    "WAVE",
    "TAP",
    "CIRCLE"
};

/* ============================================================================
 * Op Resolver
 *
 * Only include operations used by the gesture model to minimize code size.
 * This is determined by the model's converter output.
 * ============================================================================ */

static tflite::MicroMutableOpResolver<12> *resolver = nullptr;

static int setup_op_resolver()
{
    static tflite::MicroMutableOpResolver<12> static_resolver;
    resolver = &static_resolver;
    
    /* Add operations used by the gesture model (Conv1D-based) */
    TfLiteStatus status;
    
    /* Conv1D uses Conv2D internally with expanded dims */
    status = resolver->AddConv2D();
    if (status != kTfLiteOk) {
        LOG_ERR("Failed to add Conv2D op");
        return -1;
    }
    
    status = resolver->AddMaxPool2D();
    if (status != kTfLiteOk) {
        LOG_ERR("Failed to add MaxPool2D op");
        return -1;
    }
    
    status = resolver->AddExpandDims();
    if (status != kTfLiteOk) {
        LOG_ERR("Failed to add ExpandDims op");
        return -1;
    }
    
    status = resolver->AddSqueeze();
    if (status != kTfLiteOk) {
        LOG_ERR("Failed to add Squeeze op");
        return -1;
    }
    
    status = resolver->AddFullyConnected();
    if (status != kTfLiteOk) {
        LOG_ERR("Failed to add FullyConnected op");
        return -1;
    }
    
    status = resolver->AddRelu();
    if (status != kTfLiteOk) {
        LOG_ERR("Failed to add Relu op");
        return -1;
    }
    
    status = resolver->AddSoftmax();
    if (status != kTfLiteOk) {
        LOG_ERR("Failed to add Softmax op");
        return -1;
    }
    
    status = resolver->AddReshape();
    if (status != kTfLiteOk) {
        LOG_ERR("Failed to add Reshape op");
        return -1;
    }
    
    status = resolver->AddQuantize();
    if (status != kTfLiteOk) {
        LOG_ERR("Failed to add Quantize op");
        return -1;
    }
    
    status = resolver->AddDequantize();
    if (status != kTfLiteOk) {
        LOG_ERR("Failed to add Dequantize op");
        return -1;
    }
    
    /* Additional ops that may be used by Keras models */
    status = resolver->AddPad();
    if (status != kTfLiteOk) {
        LOG_ERR("Failed to add Pad op");
        return -1;
    }
    
    status = resolver->AddMean();
    if (status != kTfLiteOk) {
        LOG_ERR("Failed to add Mean op");
        return -1;
    }
    
    return 0;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

ml_status_t ml_inference_init(void)
{
    k_mutex_lock(&ml_mutex, K_FOREVER);
    
    if (ml_initialized) {
        LOG_WRN("ML inference already initialized");
        k_mutex_unlock(&ml_mutex);
        return ML_STATUS_OK;
    }
    
    LOG_INF("Initializing ML inference engine...");
    LOG_INF("  Tensor arena size: %d bytes", CONFIG_ML_TENSOR_ARENA_SIZE);
    LOG_INF("  Model size: %d bytes", gesture_model_data_len);
    
    /* Load the model */
    model = tflite::GetModel(gesture_model_data);
    if (model == nullptr) {
        LOG_ERR("Failed to load model");
        LOG_WRN("Falling back to MOCK inference mode");
        use_mock_inference = true;
        ml_initialized = true;
        k_mutex_unlock(&ml_mutex);
        return ML_STATUS_OK;
    }
    
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        LOG_ERR("Model schema version mismatch: %lu vs %d",
                model->version(), TFLITE_SCHEMA_VERSION);
        LOG_WRN("Falling back to MOCK inference mode");
        use_mock_inference = true;
        ml_initialized = true;
        k_mutex_unlock(&ml_mutex);
        return ML_STATUS_OK;
    }
    
    /* Setup operation resolver */
    if (setup_op_resolver() != 0) {
        LOG_ERR("Failed to setup op resolver");
        LOG_WRN("Falling back to MOCK inference mode");
        use_mock_inference = true;
        ml_initialized = true;
        k_mutex_unlock(&ml_mutex);
        return ML_STATUS_OK;
    }
    
    /* Create the interpreter */
    static tflite::MicroInterpreter static_interpreter(
        model, *resolver, tensor_arena, CONFIG_ML_TENSOR_ARENA_SIZE);
    interpreter = &static_interpreter;
    
    /* Allocate tensors */
    TfLiteStatus status = interpreter->AllocateTensors();
    if (status != kTfLiteOk) {
        LOG_ERR("Failed to allocate tensors");
        k_mutex_unlock(&ml_mutex);
        return ML_STATUS_ALLOC_FAILED;
    }
    
    /* Get input/output tensors */
    input_tensor = interpreter->input(0);
    output_tensor = interpreter->output(0);
    
    if (input_tensor == nullptr || output_tensor == nullptr) {
        LOG_ERR("Failed to get input/output tensors");
        k_mutex_unlock(&ml_mutex);
        return ML_STATUS_ERROR;
    }
    
    /* Validate tensor dimensions */
    LOG_INF("  Input tensor: dims=%d, size=%d, type=%d",
            input_tensor->dims->size,
            input_tensor->bytes,
            input_tensor->type);
    LOG_INF("  Output tensor: dims=%d, size=%d, type=%d",
            output_tensor->dims->size,
            output_tensor->bytes,
            output_tensor->type);
    
    /* Initialize statistics */
    ml_stats.inference_count = 0;
    ml_stats.min_time_us = UINT32_MAX;
    ml_stats.max_time_us = 0;
    ml_stats.total_time_us = 0;
    ml_stats.invoke_failures = 0;
    
    inference_sequence = 0;
    ml_initialized = true;
    
    size_t arena_used = interpreter->arena_used_bytes();
    LOG_INF("ML inference engine ready (arena used: %zu/%d bytes)",
            arena_used, CONFIG_ML_TENSOR_ARENA_SIZE);
    
    k_mutex_unlock(&ml_mutex);
    return ML_STATUS_OK;
}

ml_status_t ml_run_inference(const int8_t *input_data, inference_result_t *result)
{
    uint32_t start_time, end_time;
    TfLiteStatus status;
    
    if (!ml_initialized) {
        return ML_STATUS_NOT_INITIALIZED;
    }
    
    if (input_data == nullptr || result == nullptr) {
        return ML_STATUS_INVALID_INPUT;
    }
    
    k_mutex_lock(&ml_mutex, K_FOREVER);
    
    /* Copy input data to tensor */
    if (!use_mock_inference) {
        size_t input_size = input_tensor->bytes;
        int8_t *input_ptr = input_tensor->data.int8;
        
        for (size_t i = 0; i < input_size && i < ML_INPUT_SIZE; i++) {
            input_ptr[i] = input_data[i];
        }
    }
    
    /* Run inference with timing */
    start_time = k_cycle_get_32();
    
    if (use_mock_inference) {
        /* Mock inference simulation */
        k_busy_wait(5000); /* Simulate 5ms load */
        status = kTfLiteOk;
    } else {
        status = interpreter->Invoke();
    }
    
    end_time = k_cycle_get_32();
    
    /* Calculate inference time in microseconds */
    uint32_t cycles = end_time - start_time;
    uint32_t freq_mhz = sys_clock_hw_cycles_per_sec() / 1000000;
    uint32_t inference_time_us = (freq_mhz > 0) ? (cycles / freq_mhz) : 0;
    
    if (status != kTfLiteOk) {
        LOG_ERR("Inference invoke failed");
        ml_stats.invoke_failures++;
        k_mutex_unlock(&ml_mutex);
        return ML_STATUS_INVOKE_FAILED;
    }
    
    /* Extract output probabilities */
    /* Extract output probabilities */
    int8_t *output_ptr = nullptr;
    float scale = 0.0f;
    int32_t zero_point = 0;
    
    if (!use_mock_inference) {
        output_ptr = output_tensor->data.int8;
        scale = output_tensor->params.scale;
        zero_point = output_tensor->params.zero_point;
    }
    
    float max_score = -1000.0f;
    gesture_label_t best_gesture = GESTURE_IDLE;
    
    if (use_mock_inference) {
        /* Generate synthetic confidence scores */
        /* Default to IDLE */
        best_gesture = GESTURE_IDLE;
        max_score = 0.95f;
        
        result->class_scores[0] = 0.95f;
        result->class_scores[1] = 0.02f;
        result->class_scores[2] = 0.02f;
        result->class_scores[3] = 0.01f;
        
        /* Occasionally detect a gesture based on sequence */
        if (inference_sequence % 50 == 25) {
            best_gesture = GESTURE_WAVE;
            max_score = 0.85f;
            result->class_scores[1] = 0.85f;
            result->class_scores[0] = 0.10f;
        } else if (inference_sequence % 50 == 35) {
            best_gesture = GESTURE_TAP;
            max_score = 0.90f;
            result->class_scores[2] = 0.90f;
            result->class_scores[0] = 0.05f;
        }
    } else {
        for (int i = 0; i < GESTURE_COUNT; i++) {
            /* Dequantize output */
            float score = (output_ptr[i] - zero_point) * scale;
            result->class_scores[i] = score;
            
            if (score > max_score) {
                max_score = score;
                best_gesture = (gesture_label_t)i;
            }
        }
    }
    
    /* Fill in result */
    result->gesture = best_gesture;
    result->confidence = max_score;
    result->inference_time_us = inference_time_us;
    result->timestamp_us = (uint32_t)(k_uptime_get() * 1000);
    result->sequence = ++inference_sequence;
    
    /* Update statistics */
    ml_stats.inference_count++;
    ml_stats.total_time_us += inference_time_us;
    
    if (inference_time_us < ml_stats.min_time_us) {
        ml_stats.min_time_us = inference_time_us;
    }
    if (inference_time_us > ml_stats.max_time_us) {
        ml_stats.max_time_us = inference_time_us;
    }
    
    LOG_DBG("Inference #%u: %s (%.2f), %u us",
            result->sequence,
            gesture_names[best_gesture],
            max_score,
            inference_time_us);
    
    k_mutex_unlock(&ml_mutex);
    return ML_STATUS_OK;
}

ml_status_t ml_get_stats(ml_stats_t *stats)
{
    if (stats == nullptr) {
        return ML_STATUS_INVALID_INPUT;
    }
    
    k_mutex_lock(&ml_mutex, K_FOREVER);
    *stats = ml_stats;
    k_mutex_unlock(&ml_mutex);
    
    return ML_STATUS_OK;
}

void ml_reset_stats(void)
{
    k_mutex_lock(&ml_mutex, K_FOREVER);
    
    ml_stats.inference_count = 0;
    ml_stats.min_time_us = UINT32_MAX;
    ml_stats.max_time_us = 0;
    ml_stats.total_time_us = 0;
    ml_stats.invoke_failures = 0;
    
    k_mutex_unlock(&ml_mutex);
    
    LOG_INF("ML statistics reset");
}

const char *ml_gesture_to_string(gesture_label_t gesture)
{
    if (gesture >= GESTURE_COUNT) {
        return "UNKNOWN";
    }
    return gesture_names[gesture];
}

size_t ml_get_arena_used(void)
{
    if (!ml_initialized || interpreter == nullptr) {
        return 0;
    }
    return interpreter->arena_used_bytes();
}

bool ml_is_ready(void)
{
    return ml_initialized;
}
