#!/usr/bin/env python3
"""
SPDX-License-Identifier: MIT

Zephyr Edge AI Demo - Automated Test Harness

Runs automated tests on the Zephyr demo using QEMU.
Validates that the application starts correctly, produces output,
and detects expected gesture types.

Usage:
    python test_harness.py --build-dir ../build --timeout 30
"""

import argparse
import os
import re
import subprocess
import sys
import time
from typing import List, Dict, Tuple, Optional


class TestResult:
    """Container for test results."""
    
    def __init__(self, name: str):
        self.name = name
        self.passed = False
        self.message = ""
        self.duration = 0.0
        
    def __str__(self):
        status = "PASS" if self.passed else "FAIL"
        return f"[{status}] {self.name}: {self.message} ({self.duration:.2f}s)"


class QEMUTestHarness:
    """Runs automated tests on the Zephyr demo using QEMU."""
    
    def __init__(self, build_dir: str, timeout: int = 30):
        """
        Initialize the test harness.
        
        Args:
            build_dir: Path to the Zephyr build directory
            timeout: Maximum test duration in seconds
        """
        self.build_dir = build_dir
        self.timeout = timeout
        self.output_lines: List[str] = []
        self.results: List[TestResult] = []
        
    def run_qemu(self) -> Tuple[int, str]:
        """
        Run the application in QEMU.
        
        Returns:
            Tuple of (return_code, output)
        """
        # Find the zephyr.elf
        elf_path = os.path.join(self.build_dir, 'zephyr', 'zephyr.elf')
        
        if not os.path.exists(elf_path):
            return (-1, f"ELF not found: {elf_path}")
        
        # Construct QEMU command
        # This assumes the west build configured QEMU properly
        cmd = ['west', 'build', '-t', 'run']
        
        print(f"Running: {' '.join(cmd)}")
        print(f"Timeout: {self.timeout}s")
        print("-" * 60)
        
        try:
            proc = subprocess.Popen(
                cmd,
                cwd=os.path.dirname(self.build_dir),
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True
            )
            
            output_lines = []
            start_time = time.time()
            
            while True:
                elapsed = time.time() - start_time
                
                if elapsed > self.timeout:
                    proc.terminate()
                    proc.wait(timeout=5)
                    break
                
                line = proc.stdout.readline()
                if not line and proc.poll() is not None:
                    break
                    
                if line:
                    line = line.strip()
                    output_lines.append(line)
                    print(line)
                    
            self.output_lines = output_lines
            return (0, '\n'.join(output_lines))
            
        except subprocess.TimeoutExpired:
            proc.kill()
            return (-1, "QEMU timed out")
        except Exception as e:
            return (-1, str(e))
    
    def test_startup(self) -> TestResult:
        """Test that the application starts correctly."""
        result = TestResult("Application Startup")
        start = time.time()
        
        # Look for startup banner
        startup_patterns = [
            r'Zephyr Edge AI Demo',
            r'Version:',
            r'ML inference engine ready'
        ]
        
        found = sum(1 for pattern in startup_patterns 
                    if any(re.search(pattern, line) for line in self.output_lines))
        
        if found >= 2:
            result.passed = True
            result.message = f"Found {found}/{len(startup_patterns)} startup markers"
        else:
            result.message = f"Only found {found}/{len(startup_patterns)} startup markers"
        
        result.duration = time.time() - start
        return result
    
    def test_sensor_sampling(self) -> TestResult:
        """Test that sensor data is being sampled."""
        result = TestResult("Sensor Sampling")
        start = time.time()
        
        # Look for sensor-related log messages
        sensor_patterns = [
            r'Sensor HAL initialized',
            r'mock accelerometer',
            r'Starting gesture:'
        ]
        
        found = sum(1 for pattern in sensor_patterns 
                    if any(re.search(pattern, line, re.IGNORECASE) 
                          for line in self.output_lines))
        
        if found >= 2:
            result.passed = True
            result.message = f"Sensor subsystem working"
        else:
            result.message = "Sensor initialization not detected"
        
        result.duration = time.time() - start
        return result
    
    def test_inference_output(self) -> TestResult:
        """Test that inference results are being produced."""
        result = TestResult("Inference Output")
        start = time.time()
        
        # Count JSON inference outputs
        inference_count = 0
        gestures_found = set()
        
        for line in self.output_lines:
            if '"type":"inference"' in line:
                inference_count += 1
                
                # Extract gesture
                match = re.search(r'"gesture":"(\w+)"', line)
                if match:
                    gestures_found.add(match.group(1))
        
        if inference_count > 0:
            result.passed = True
            result.message = f"Found {inference_count} inferences, gestures: {gestures_found}"
        else:
            result.message = "No inference output detected"
        
        result.duration = time.time() - start
        return result
    
    def test_gesture_detection(self) -> TestResult:
        """Test that multiple gesture types are detected."""
        result = TestResult("Gesture Detection")
        start = time.time()
        
        gestures_found = set()
        expected_gestures = {'IDLE', 'WAVE', 'TAP', 'CIRCLE'}
        
        for line in self.output_lines:
            for gesture in expected_gestures:
                if f'gesture":"{gesture}"' in line or f'Detected: {gesture}' in line:
                    gestures_found.add(gesture)
        
        coverage = len(gestures_found) / len(expected_gestures) * 100
        
        if len(gestures_found) >= 2:
            result.passed = True
            result.message = f"Detected {len(gestures_found)}/4 gesture types: {gestures_found}"
        else:
            result.message = f"Only detected {len(gestures_found)} gesture type(s)"
        
        result.duration = time.time() - start
        return result
    
    def test_debug_output(self) -> TestResult:
        """Test that debug statistics are being output."""
        result = TestResult("Debug Output")
        start = time.time()
        
        debug_count = 0
        
        for line in self.output_lines:
            if '"type":"debug"' in line or 'heap=' in line.lower():
                debug_count += 1
        
        if debug_count > 0:
            result.passed = True
            result.message = f"Found {debug_count} debug messages"
        else:
            result.message = "No debug output detected"
        
        result.duration = time.time() - start
        return result
    
    def test_no_errors(self) -> TestResult:
        """Test that no critical errors occurred."""
        result = TestResult("No Critical Errors")
        start = time.time()
        
        error_patterns = [
            r'\[ERROR\]',
            r'ASSERTION FAILED',
            r'Stack overflow',
            r'FATAL',
            r'panic'
        ]
        
        errors_found = []
        for pattern in error_patterns:
            for line in self.output_lines:
                if re.search(pattern, line, re.IGNORECASE):
                    errors_found.append(line[:80])
        
        if not errors_found:
            result.passed = True
            result.message = "No critical errors detected"
        else:
            result.message = f"Found {len(errors_found)} error(s): {errors_found[0]}"
        
        result.duration = time.time() - start
        return result
    
    def run_all_tests(self) -> List[TestResult]:
        """
        Run all tests.
        
        Returns:
            List of test results
        """
        print("=" * 60)
        print("ZEPHYR EDGE AI DEMO - AUTOMATED TEST HARNESS")
        print("=" * 60)
        
        # Run QEMU
        ret, output = self.run_qemu()
        
        print("-" * 60)
        print("Running tests...")
        print("-" * 60)
        
        # Run individual tests
        self.results = [
            self.test_startup(),
            self.test_sensor_sampling(),
            self.test_inference_output(),
            self.test_gesture_detection(),
            self.test_debug_output(),
            self.test_no_errors(),
        ]
        
        # Print results
        print("\n" + "=" * 60)
        print("TEST RESULTS")
        print("=" * 60)
        
        passed = 0
        for result in self.results:
            print(result)
            if result.passed:
                passed += 1
        
        print("-" * 60)
        print(f"Total: {passed}/{len(self.results)} tests passed")
        print("=" * 60)
        
        return self.results
    
    def get_exit_code(self) -> int:
        """Get exit code based on test results."""
        if all(r.passed for r in self.results):
            return 0
        return 1


def main():
    parser = argparse.ArgumentParser(
        description="Automated test harness for Zephyr Edge AI Demo"
    )
    parser.add_argument(
        '--build-dir', '-b',
        default='../build',
        help='Path to the Zephyr build directory'
    )
    parser.add_argument(
        '--timeout', '-t',
        type=int,
        default=30,
        help='Test timeout in seconds (default: 30)'
    )
    
    args = parser.parse_args()
    
    harness = QEMUTestHarness(
        build_dir=args.build_dir,
        timeout=args.timeout
    )
    
    harness.run_all_tests()
    sys.exit(harness.get_exit_code())


if __name__ == '__main__':
    main()
