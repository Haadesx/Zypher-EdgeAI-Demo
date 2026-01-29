# Project Documentation: Zypher EdgeAI Demo

Comprehensive documentation of the codebase structure and individual files to support technical walkthroughs and interviews.

## Table of Contents

### Configuration & Build
- [CMakeLists.txt](#cmakeliststxt)
- [prj.conf](#prjconf)
- [west.yml](#westyml)

### Documentation
- [README.md](#readmemd)
- [GETTING_STARTED.md](#getting_startedmd)
- [docs/ARCHITECTURE.md](#docsarchitecturemd)
- [docs/DEBUGGING.md](#docsdebuggingmd)

### ML Model Training
- [model/train_gesture_model.py](#modeltrain_gesture_modelpy)

### Application Source
- [src/main.c](#srcmainc)
- **Machine Learning**: [src/ml/inference.cpp](#srcmlinferencecpp), [src/ml/inference.h](#srcmlinferenceh), [src/ml/gesture_model.c](#srcmlgesture_modelc), [src/ml/gesture_model.h](#srcmlgesture_modelh), [src/ml/preprocessing.c](#srcmlpreprocessingc), [src/ml/preprocessing.h](#srcmlpreprocessingh)
- **Sensors**: [src/sensor/sensor_hal.c](#srcsensorsensor_halc), [src/sensor/sensor_hal.h](#srcsensorsensor_halh), [src/sensor/mock_accel.c](#srcsensormock_accelc)
- **Output & Comms**: [src/output/uart_protocol.c](#srcoutputuart_protocolc), [src/output/uart_protocol.h](#srcoutputuart_protocolh), [src/output/ring_buffer.c](#srcoutputring_bufferc), [src/output/ring_buffer.h](#srcoutputring_bufferh)
- **Debug & Utils**: [src/debug/debug_monitor.c](#srcdebugdebug_monitorc), [src/debug/debug_monitor.h](#srcdebugdebug_monitorh), [src/debug/timing.c](#srcdebugtimingc), [src/debug/timing.h](#srcdebugtimingh)

### Tools & Scripts
- [scripts/show_demo.py](#scriptsshow_demopy)
- [scripts/latency_analyzer.py](#scriptslatency_analyzerpy)
- [scripts/test_harness.py](#scriptstest_harnesspy)
- [scripts/uart_logger.py](#scriptsuart_loggerpy)

---◊

## File Documentation

### Configuration & Build

#### CMakeLists.txt
- **Purpose**: Build configuration for the Zephyr application.
- **Reason for Inclusion**: Required by Zephyr's CMake-based build system to define sources and dependencies.
- **Functionality**:
  - Initializes the project foundation.
  - Registers the TFLite-Micro module.
  - Defines the source files for compilation.
  - Links C++ standard libraries required by TensorFlow Lite.
- **When It Is Used**: Automatically invoked during `west build`.

#### prj.conf
- **Purpose**: Kernel configuration (Kconfig) for the project.
- **Reason for Inclusion**: Enables/disables specific Zephyr subsystems and sets kernel parameters.
- **Functionality**:
  - Enables C++ support, logging, and floating-point unit (FPU).
  - Configures heap/stack sizes for ML workloads.
  - Sets TFLite-Micro specific options (e.g., disabling CMSIS-NN kernels for QEMU compatibility).
- **When It Is Used**: Read during the build configuration phase to generate the kernel binary.

#### west.yml
- **Purpose**: Manifest file for the `west` meta-tool.
- **Reason for Inclusion**: Manages external dependencies like the Zephyr kernel and TFLite-Micro library.
- **Functionality**:
  - Defines the "remote" repositories (Zephyr upstream, TensorFlow).
  - Specifies the exact versions/commits to ensure reproducible builds.
- **When It Is Used**: During `west init` and `west update` to fetch project dependencies.

---

### Documentation

#### README.md
- **Purpose**: Primary project entry point and overview.
- **Reason for Inclusion**: Essential for telling users (and interviewers) what the project is.
- **Functionality**: Summarizes the project features, architecture, and provides a quick-start guide.
- **When It Is Used**: First file viewed on GitHub or when exploring the project.

#### GETTING_STARTED.md
- **Purpose**: Step-by-step setup guide.
- **Reason for Inclusion**: Ensures new users can replicate the build environment.
- **Functionality**: Detailed commands for installing Zephyr SDK, Python dependencies, and building the firmware.
- **When It Is Used**: During initial workspace setup.

#### docs/ARCHITECTURE.md
- **Purpose**: Technical design document.
- **Reason for Inclusion**: Explains the "why" and "how" of the system design.
- **Functionality**: Diagrams and text explaining the data flow: Sensor -> Preprocessing -> Inference -> UART Output.
- **When It Is Used**: For understanding system internals during reviews or architectural discussions.

#### docs/DEBUGGING.md
- **Purpose**: Troubleshooting guide.
- **Reason for Inclusion**: Captures knowledge about common issues (e.g., stack overflows, TFLite op mismatches).
- **Functionality**: Lists "Known Issues" and their solutions, plus tips for using GDB with QEMU.
- **When It Is Used**: When the system crashes or behaves unexpectedly.

---

### ML Model Training

#### model/train_gesture_model.py
- **Purpose**: End-to-end Python pipeline for creating the neural network.
- **Reason for Inclusion**: Generates the "brains" of the embedded systems; creates the `gesture_model.tflite` file.
- **Functionality**:
  - Generates synthetic accelerometer data (sine waves, spikes) for testing.
  - Trains a Keras Dense neural network.
  - Visualizes progress with the `rich` library.
  - **Quantizes** the model to INT8 for efficiency.
  - **Exports** the model as C code (`gesture_model.c/h`) for embedding.
- **When It Is Used**: Run manually to retrain or update the gesture recognition model.

---

### Application Source

#### src/main.c
- **Purpose**: Application entry point and main coordinator.
- **Reason for Inclusion**: The root of the firmware execution.
- **Functionality**:
  - Initializes all subsystems (Sensor, ML, UART).
  - Spawns the worker threads.
  - Manages the main event loop (if applicable) or allows threads to take over.
- **When It Is Used**: Runs immediately after kernel boot.

#### src/ml/inference.cpp
- **Purpose**: C++ wrapper for the TensorFlow Lite Micro engine.
- **Reason for Inclusion**: Bridges the gap between C-based Zephyr and C++ TFLite library.
- **Functionality**:
  - Sets up the TFLite interpreter and memory arena.
  - Registers TFLite operations (Conv2D, Dense, Softmax) using `MicroMutableOpResolver`.
  - Runs the actual `interpreter->Invoke()` for inference.
- **When It Is Used**: Called periodically by the application loop to process sensor data.

#### src/ml/inference.h
- **Purpose**: C interface for the C++ inference code.
- **Reason for Inclusion**: Allows C files (like `main.c`) to call C++ functions without compilation errors.
- **Functionality**: Declares `inference_init()` and `inference_process()`.
- **When It Is Used**: Included by `main.c`.

#### src/ml/gesture_model.c
- **Purpose**: The actual neural network weights and structure stored as a C byte array.
- **Reason for Inclusion**: Stores the trained model in flash memory so the firmware can read it.
- **Functionality**: Defines `const unsigned char gesture_model_data[]`.
- **When It Is Used**: Accessed by `inference.cpp` during model loading.

#### src/ml/gesture_model.h
- **Purpose**: Header for the model data.
- **Reason for Inclusion**: Exposes the model array and length to other files.
- **Functionality**: Declares the external model array.
- **When It Is Used**: Included by `inference.cpp`.

#### src/ml/preprocessing.c
- **Purpose**: Data preparation before inference.
- **Reason for Inclusion**: Raw sensor data needs normalization/scaling to match the model's training data.
- **Functionality**:
  - Buffers incoming samples to create a "window" (e.g., 50 samples).
  - Normalizes values (e.g., converts G-force to -1.0 to 1.0 range).
- **When It Is Used**: Called for every new sensor sample before passing it to the neural network.

#### src/ml/preprocessing.h
- **Purpose**: Header for preprocessing logic.
- **Reason for Inclusion**: Exposes preprocessing functions and constants (window size).
- **Functionality**: API declarations.
- **When It Is Used**: Included by sensor processing loops.

#### src/sensor/sensor_hal.c
- **Purpose**: Hardware Abstraction Layer (HAL) for sensors.
- **Reason for Inclusion**: Decouples logic from hardware; allows swapping between real sensors and mock data.
- **Functionality**:
  - Initializes the sensor device.
  - Fetches accelerometer X, Y, Z data.
  - In QEMU mode, it routes to `mock_accel.c`.
- **When It Is Used**: Called repeatedly to fetch the latest motion data.

#### src/sensor/sensor_hal.h
- **Purpose**: Header for the sensor HAL.
- **Reason for Inclusion**: Defines the standard interface for getting sensor data.
- **Functionality**: Declares `sensor_hal_init()` and `sensor_hal_read()`.
- **When It Is Used**: Included by `main.c` or data processing threads.

#### src/sensor/mock_accel.c
- **Purpose**: Simulates a physical accelerometer.
- **Reason for Inclusion**: Enables testing on QEMU (which has no physical sensors).
- **Functionality**:
  - Generates "fake" data patterns (Sine waves for WAVE, Spikes for TAP).
  - Used to verify the model correctly detects gestures without needing hardware.
- **When It Is Used**: invoked by `sensor_hal.c` when running on QEMU.

#### src/output/uart_protocol.c
- **Purpose**: Formats and sends data over UART (Serial).
- **Reason for Inclusion**: Allows the device to communicate results to a PC dashboard.
- **Functionality**:
  - Formats data into JSON commands (e.g., `{"gesture": "WAVE", "conf": 0.99}`).
  - Handles thread-safe printing to the console.
- **When It Is Used**: Called whenever an inference result is ready.

#### src/output/uart_protocol.h
- **Purpose**: Header for UART protocol.
- **Reason for Inclusion**: Defines API for sending structured messages.
- **Functionality**: API declarations for sending startup, inference, and heartbeat messages.
- **When It Is Used**: Included by files that need to log data.

#### src/output/ring_buffer.c
- **Purpose**: Circular buffer implementation.
- **Reason for Inclusion**: Safe data transfer between high-priority sensor interrupts and lower-priority processing threads.
- **Functionality**: Standard FIFO (First-In, First-Out) logic for storing data structs.
- **When It Is Used**: Buffering sensor samples or inference results.

#### src/output/ring_buffer.h
- **Purpose**: Header for circular buffer.
- **Reason for Inclusion**: Defines the ring buffer structure and API.
- **Functionality**: API declarations.
- **When It Is Used**: Included anywhere data buffering is needed.

#### src/debug/debug_monitor.c
- **Purpose**: System health monitoring.
- **Reason for Inclusion**: Crucial for embedded reliability; tracks stack usage to catch overflows.
- **Functionality**:
  - Periodically checks thread stack pointers.
  - Logs warnings if stack usage exceeds safety thresholds (e.g., >90%).
- **When It Is Used**: Runs as a background "watchdog" thread.

#### src/debug/debug_monitor.h
- **Purpose**: Header for debug monitor.
- **Reason for Inclusion**: Exposes init functions for the monitor.
- **Functionality**: API declarations.
- **When It Is Used**: Included by `main.c` to start the monitor.

#### src/debug/timing.c
- **Purpose**: Performance profiling utilities.
- **Reason for Inclusion**: Measuring inference latency (how fast the AI runs).
- **Functionality**: Uses hardware cycle counters (DWT on ARM) to measure precise execution time in microseconds.
- **When It Is Used**: Wraps the `inference_process()` call to measure duration.

#### src/debug/timing.h
- **Purpose**: Header for timing utilities.
- **Reason for Inclusion**: Exposes timing start/stop functions.
- **Functionality**: API declarations.
- **When It Is Used**: Included by inference loops.

---

### Tools & Scripts

#### scripts/show_demo.py
- **Purpose**: **"The Tangible Demo"** - Real-time visualization dashboard.
- **Reason for Inclusion**: Transforms raw text logs into a visual, interactive UI.
- **Functionality**:
  - Runs QEMU in the background.
  - Parses UART logs in real-time.
  - Uses `rich` to render a terminal dashboard with live emojis and graphs.
- **When It Is Used**: During presentations or demonstrations to showcase the project.

#### scripts/latency_analyzer.py
- **Purpose**: Statistical analysis tool.
- **Reason for Inclusion**: Validates performance requirements.
- **Functionality**: Parses a log file and calculates min/max/avg latency and jitter statistics.
- **When It Is Used**: For generating performance reports.

#### scripts/test_harness.py
- **Purpose**: Automated testing script.
- **Reason for Inclusion**: CI/CD integration and regression testing.
- **Functionality**: Runs the simulation for a set duration and asserts that specific gestures were detected correctly.
- **When It Is Used**: In GitHub Actions or pre-release checks.

#### scripts/uart_logger.py
- **Purpose**: Simple data logger.
- **Reason for Inclusion**: Capturing raw data for offline debugging.
- **Functionality**: Reads serial port data and saves it to a text file with timestamps.
- **When It Is Used**: Debugging intermittent issues or collecting datasets.
