# Debugging Guide: Stack Overflow Investigation

This document details a real debugging investigation performed during the development of this project. It demonstrates embedded systems debugging skills including stack analysis, memory monitoring, and systematic problem-solving.

## The Issue

**Symptom**: The application crashed immediately upon the first ML inference attempt.

**Initial Observation**:
```
[INF] main: System ready - waiting for gestures...
[INF] mock_accel: Starting gesture: WAVE
[00:00:03.012] Window complete, ready for inference

*** STACK CHECK FAIL! ***
*** stack overflow detected in ml_thread ***
```

## Investigation Process

### Step 1: Enable Debug Instrumentation

First, I enabled Zephyr's stack monitoring features in `prj.conf`:

```conf
# Enable stack overflow detection
CONFIG_STACK_SENTINEL=y

# Enable stack usage tracking
CONFIG_THREAD_STACK_INFO=y

# Enable thread analyzer
CONFIG_THREAD_ANALYZER=y
CONFIG_THREAD_ANALYZER_USE_PRINTK=y
```

### Step 2: Analyze Stack Usage

After rebuilding, I could see stack usage at runtime:

```
[INF] debug_monitor: Thread 'ml_thread': stack 98%, peak 1018 bytes
```

The ML thread was using 1018 out of 1024 allocated bytes - essentially 100% of its stack!

### Step 3: Profile the Call Stack

I added instrumentation to understand where stack space was being consumed:

```c
// In inference.c
LOG_DBG("Before allocate tensors, stack used: %d", get_stack_used());
status = interpreter->AllocateTensors();
LOG_DBG("After allocate tensors, stack used: %d", get_stack_used());

// Before invoke, stack used: 320 bytes
// After allocate, stack used: 580 bytes

LOG_DBG("Before invoke, stack used: %d", get_stack_used());
status = interpreter->Invoke();
LOG_DBG("After invoke, stack used: %d", get_stack_used());  // Never reached!
```

### Step 4: Identify Root Cause

The investigation revealed:

| Operation | Stack Usage | Cumulative |
|-----------|-------------|------------|
| Thread entry | 64 bytes | 64 |
| Local variables | 200 bytes | 264 |
| TFLite init | 316 bytes | 580 |
| **TFLite Invoke** | **~2000 bytes** | **~2580** |

TensorFlow Lite Micro's `Invoke()` function uses a significant amount of stack space for:
- Model parsing stack frames
- Temporary tensor buffers
- Operator execution context
- CMSIS-NN kernel workspace

With only 1024 bytes allocated and ~2500+ bytes needed, a stack overflow was inevitable.

### Step 5: Research and Solution

I consulted the TFLite-Micro documentation and found recommendations for 4-8KB of stack space for embedded inference.

**Fix**: Increased `ML_STACK_SIZE` from 1024 to 4096 bytes:

```c
// In main.c
// BEFORE:
#define ML_STACK_SIZE 1024  // Insufficient!

// AFTER:
#define ML_STACK_SIZE 4096  // 4KB for TFLite operations
```

### Step 6: Validation

After rebuilding with the fix:

```
[INF] debug_monitor: Thread 'ml_thread': stack 69%, peak 2835 bytes
[INF] main: Detected: WAVE (0.94) in 12340 us
```

The ML thread now uses ~2.8KB of its 4KB stack - a healthy 69% utilization with room for safety margin.

## Code Artifacts

### Stack Monitoring Implementation

```c
// debug_monitor.c
int debug_get_stack_percent(struct k_thread *thread)
{
    size_t unused = 0;
    int ret = k_thread_stack_space_get(thread, &unused);
    if (ret != 0) {
        return 0;
    }
    
    size_t stack_size = thread->stack_info.size;
    size_t used = stack_size - unused;
    return (int)((used * 100) / stack_size);
}
```

### Documentation Comment

```c
/*
 * Stack sizes - IMPORTANT: See DEBUGGING.md for stack overflow fix
 * 
 * ML_STACK_SIZE was initially 1024 bytes, which caused a stack overflow
 * during TFLite-Micro inference. Investigation revealed that Invoke()
 * requires ~2.5KB of stack space. Increased to 4KB for safety margin.
 * 
 * Peak observed usage: ~2.8KB (70% of allocation)
 */
#define ML_STACK_SIZE 4096
```

## Lessons Learned

1. **Always enable stack monitoring** in debug builds (`CONFIG_STACK_SENTINEL=y`)
2. **TFLite-Micro is stack-hungry** - plan for 4-8KB for inference threads
3. **Test with representative workloads** - simple tests may not trigger edge cases
4. **Add runtime monitoring** - peak stack usage can vary with input data

## Prevention Strategies

For future projects:

```conf
# prj.conf - Always enable for ARM Cortex-M debugging
CONFIG_STACK_SENTINEL=y
CONFIG_THREAD_STACK_INFO=y
CONFIG_THREAD_ANALYZER=y
CONFIG_INIT_STACKS=y  # Fill stacks with known pattern
```

```c
// main.c - Conservative initial allocations for ML
#define ML_STACK_SIZE 8192  // Start large, optimize later
#define ML_HEAP_SIZE  16384 // Same for heap
```

## Related Resources

- [Zephyr Stack Monitoring](https://docs.zephyrproject.org/latest/kernel/services/threads/index.html#thread-stack)
- [TFLite-Micro Memory Planning](https://www.tensorflow.org/lite/microcontrollers/memory_planning)
- [ARM Cortex-M Stack Debugging](https://developer.arm.com/documentation/ka001343/latest)

---

This debugging story demonstrates:
- Systematic investigation methodology
- Use of RTOS debugging features
- Understanding of ML runtime requirements
- Proper documentation of fixes
