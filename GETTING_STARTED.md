# Getting Started

This guide will help you set up and run the Zephyr Edge AI Demo on your system.

## Prerequisites

- **macOS** (tested) or **Linux** (Ubuntu 20.04+)
- **Python 3.8 or later**
- **Git**
- **~5GB disk space** (for Zephyr SDK and modules)

## Step 1: Install System Dependencies

### macOS

```bash
# Install Homebrew if not present
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install required packages
brew install cmake ninja gperf python3 ccache qemu dtc wget

# Install ARM GCC toolchain (backup, Zephyr SDK is preferred)
brew install --cask gcc-arm-embedded
```

### Linux (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install --no-install-recommends \
    git cmake ninja-build gperf \
    ccache dfu-util device-tree-compiler wget \
    python3-dev python3-pip python3-setuptools python3-tk python3-wheel \
    xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev
```

## Step 2: Clone the Repository

```bash
cd /path/to/your/projects
git clone https://github.com/YOUR_USERNAME/zephyr-edge-ai-demo.git
cd zephyr-edge-ai-demo
```

## Step 3: Set Up Python Environment

```bash
# Create a virtual environment
python3 -m venv .venv

# Activate it
source .venv/bin/activate  # On macOS/Linux
# .venv\Scripts\activate   # On Windows

# Upgrade pip
pip install --upgrade pip

# Install west (Zephyr's meta-tool)
pip install west
```

## Step 4: Initialize the Workspace

```bash
# Initialize west workspace with this project as the manifest
west init -l .

# Fetch Zephyr and all required modules
# This downloads ~3GB of data, may take 10-30 minutes
west update

# Export CMake package (allows CMake to find Zephyr)
west zephyr-export

# Install Python dependencies
west packages pip --install
```

## Step 5: Install the Zephyr SDK

The Zephyr SDK provides toolchains for all supported architectures, including ARM.

```bash
# Download and install the SDK
west sdk install

# This will:
# 1. Download the SDK (~1.5GB)
# 2. Extract to ~/.local/zephyr-sdk-X.X.X/
# 3. Configure environment variables
```

Alternatively, install manually:

```bash
# Download SDK
cd ~
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.5/zephyr-sdk-0.16.5_macos-x86_64.tar.xz

# Extract
tar xf zephyr-sdk-0.16.5_macos-x86_64.tar.xz

# Run setup
cd zephyr-sdk-0.16.5
./setup.sh

# Set environment variable (add to ~/.zshrc or ~/.bashrc)
export ZEPHYR_SDK_INSTALL_DIR=~/zephyr-sdk-0.16.5
```

## Step 6: Build the Application

```bash
# Navigate back to the project directory
cd /path/to/zephyr-edge-ai-demo

# Activate virtual environment if not already active
source .venv/bin/activate

# Build for QEMU ARM Cortex-M3 (mps2/an385 board)
west build -b mps2/an385 -p always

# The -p always flag ensures a pristine build
```

### Build Output

A successful build will show:

```
-- west build: generating a build system
...
[140/140] Linking C executable zephyr/zephyr.elf

Memory region         Used Size  Region Size  %age Used
           FLASH:       78KB       4MB        1.90%
             RAM:       18KB       4MB        0.44%
        IDT_LIST:          0         2KB        0.00%
```

## Step 7: Run on QEMU

```bash
# Run the application in QEMU
west build -t run

# You should see the startup banner and gesture detections
# Press Ctrl+A, then X to exit QEMU
```

### Expected Output

```
*** Booting Zephyr OS build v3.x.x ***

╔══════════════════════════════════════════════════════════╗
║     Zephyr Edge AI Demo - Gesture Recognition           ║
║     Version: 1.0.0                                      ║
╚══════════════════════════════════════════════════════════╝

{"type":"startup","version":"1.0.0","board":"mps2/an385","ts":1234}
[INF] main: Zephyr Edge AI Demo starting...
[INF] main: Board: mps2/an385
[INF] sensor_hal: Initializing sensor HAL...
[INF] mock_accel: Initializing mock accelerometer
[INF] ml_inference: Initializing ML inference engine...
[INF] ml_inference: ML inference engine ready (arena used: 6144/8192 bytes)
[INF] main: All threads started successfully
[INF] main: System ready - waiting for gestures...

[00:00:03.023] Starting gesture: WAVE
{"type":"inference","seq":1,"ts":3456789,"gesture":"WAVE","conf":0.94,"latency_us":12340}
```

## Step 8: Generate Memory Reports

```bash
# RAM usage report
west build -t ram_report

# ROM/Flash usage report
west build -t rom_report
```

## Step 9: Using Analysis Tools

### Collect UART Logs (Hardware)

If you have real hardware:

```bash
# Auto-detect port and log
python scripts/uart_logger.py --auto --output results.csv
```

### Simulate with QEMU Output

```bash
# Run QEMU and pipe output to test harness
west build -t run 2>&1 | python scripts/uart_logger.py --simulate
```

### Analyze Latency

```bash
# Generate statistics and plot
python scripts/latency_analyzer.py --input results.csv --output analysis.png
```

## Troubleshooting

### "west: command not found"

Make sure your virtual environment is activated:
```bash
source .venv/bin/activate
```

### "Could not find a package configuration file provided by Zephyr"

Run the export command:
```bash
west zephyr-export
```

### Build fails with toolchain errors

Ensure the SDK is properly installed:
```bash
echo $ZEPHYR_SDK_INSTALL_DIR
# Should show path to SDK

# Re-run SDK setup if needed
cd $ZEPHYR_SDK_INSTALL_DIR
./setup.sh
```

### QEMU exits immediately

Try running directly:
```bash
qemu-system-arm -machine mps2-an385 -cpu cortex-m3 \
    -kernel build/zephyr/zephyr.elf -nographic
```

## Next Steps

1. **Explore the Code**: Start with `src/main.c` for the application architecture
2. **Read the Debugging Guide**: See `docs/DEBUGGING.md` for the stack overflow investigation
3. **Modify Configuration**: Edit `prj.conf` to change sample rates, thresholds, etc.
4. **Run Automated Tests**: `python scripts/test_harness.py`

## Building for Real Hardware

To build for a physical board (e.g., STM32F4 Discovery):

```bash
# List available boards
west boards | grep stm32

# Build for STM32F4 Discovery
west build -b stm32f4_disco -p always

# Flash to connected board
west flash
```

---

Need help? Open an issue on GitHub!
