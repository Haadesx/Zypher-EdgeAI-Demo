# System Architecture

This document describes the software architecture of the Zephyr Edge AI Demo.

## High-Level Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    Zephyr RTOS Kernel                       │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │
│  │   Sensor    │  │     ML      │  │   Output    │         │
│  │   Thread    │  │   Thread    │  │   Thread    │         │
│  │  (100 Hz)   │  │ (on-demand) │  │   (async)   │         │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘         │
│         │                │                │                 │
│         ▼                ▼                ▼                 │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              Shared Data Structures                  │   │
│  │  • Preprocessing Window Buffer                       │   │
│  │  • Inference Result Ring Buffer                      │   │
│  │  • Debug Statistics                                  │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## Thread Architecture

### Thread Priorities

| Thread | Priority | Stack Size | Purpose |
|--------|----------|------------|---------|
| `sensor` | 5 (high) | 1 KB | Consistent sampling rate |
| `ml_thread` | 7 | 4 KB | ML inference execution |
| `output` | 9 | 1 KB | UART transmission |
| `debug` | 11 (low) | 1 KB | Monitoring and logging |

### Thread Communication

```
sensor_thread ──┬──▶ preprocessing_add_sample()
                │
                └──▶ k_sem_give(&ml_sem)  [when window ready]
                            │
                            ▼
ml_thread ◀─────────────────┘
    │
    ├──▶ preprocessing_get_input()
    │
    ├──▶ ml_run_inference()
    │
    └──▶ result_buffer_push()
                │
                ▼
output_thread ◀─┘
    │
    └──▶ uart_output_inference()
```

## Module Breakdown

### Sensor HAL (`src/sensor/`)

Hardware abstraction layer for accelerometer access.

```
sensor_hal.h    - Public interface
sensor_hal.c    - HAL implementation
mock_accel.c    - Simulated sensor for QEMU
```

**Key Features**:
- Thread-safe with mutex protection
- Statistics tracking (samples read, errors)
- Pluggable backend (mock vs. real hardware)

### ML Inference (`src/ml/`)

TensorFlow Lite Micro inference engine.

```
inference.h     - Public inference API
inference.cpp   - TFLite-Micro wrapper
preprocessing.c - Input data processing
gesture_model.c - Quantized model data
```

**Key Features**:
- INT8 quantized model (~10KB)
- CMSIS-NN optimized kernels for ARM
- Inference timing measurement
- Op resolver with minimal footprint

### Output Protocol (`src/output/`)

Structured output over UART.

```
uart_protocol.h - Protocol definitions
uart_protocol.c - JSON formatting
ring_buffer.c   - Result queuing
```

**JSON Message Types**:
```json
{"type": "inference", "gesture": "WAVE", "conf": 0.94, "latency_us": 12340}
{"type": "debug", "heap_used": 1024, "stack_used": 2800}
{"type": "heartbeat", "uptime_ms": 60000}
{"type": "error", "code": -1, "message": "Sensor init failed"}
```

### Debug Infrastructure (`src/debug/`)

Runtime monitoring and profiling.

```
debug_monitor.h - Monitoring API
debug_monitor.c - Stack/heap tracking
timing.h        - Timing utilities
timing.c        - Cycle-accurate measurements
```

**Monitored Metrics**:
- Thread stack usage (current and peak)
- Heap allocation and fragmentation
- CPU usage estimate
- Inference latency statistics

## Data Flow

### Sensor Sampling

```
100 Hz Timer Tick
       │
       ▼
mock_accel_read() ──▶ Generate gesture pattern
       │
       ▼
preprocessing_add_sample() ──▶ Window buffer [50 samples]
       │
       ▼
Window complete? ──▶ k_sem_give(&ml_sem)
```

### Inference Pipeline

```
k_sem_take(&ml_sem)
       │
       ▼
preprocessing_get_input() ──▶ Quantize to INT8
       │
       ▼
ml_run_inference()
       │
       ├──▶ Copy to input tensor
       │
       ├──▶ interpreter->Invoke()
       │
       ├──▶ Read output tensor
       │
       └──▶ Measure latency
       │
       ▼
result_buffer_push()
```

## Memory Layout

### RAM Usage

| Component | Size | Notes |
|-----------|------|-------|
| TFLite Arena | 8 KB | Configured via Kconfig |
| ML Thread Stack | 4 KB | Increased for TFLite |
| Other Stacks | 4 KB | sensor + output + debug |
| Preprocessing | 600 B | 50 samples × 3 axes × 4 bytes |
| Ring Buffer | 2 KB | 16 entries × 128 bytes |
| **Total** | ~19 KB | |

### Flash Usage

| Component | Size | Notes |
|-----------|------|-------|
| Zephyr Kernel | ~40 KB | Minimal configuration |
| TFLite-Micro | ~25 KB | With CMSIS-NN |
| Model Data | ~10 KB | Quantized weights |
| Application | ~5 KB | Custom code |
| **Total** | ~80 KB | |

## Build Configuration

### Key Kconfig Options

```
# C++ for TFLite
CONFIG_CPP=y
CONFIG_STD_CPP17=y

# TensorFlow Lite Micro
CONFIG_TENSORFLOW_LITE_MICRO=y
CONFIG_TENSORFLOW_LITE_MICRO_CMSIS_NN_KERNELS=y

# Memory
CONFIG_MAIN_STACK_SIZE=4096
CONFIG_HEAP_MEM_POOL_SIZE=16384

# Debug
CONFIG_STACK_SENTINEL=y
CONFIG_THREAD_STACK_INFO=y
```

## Extensibility Points

### Adding a New Sensor

1. Create `src/sensor/my_sensor.c`
2. Implement the HAL interface
3. Update `sensor_hal.c` to dispatch based on Kconfig

### Adding a New Model

1. Train model and convert to TFLite
2. Quantize to INT8
3. Convert to C array: `xxd -i model.tflite > gesture_model.c`
4. Update op resolver if new operations needed

### Adding a New Output Format

1. Add message type to `uart_protocol.h`
2. Implement formatting function in `uart_protocol.c`
