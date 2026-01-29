#!/usr/bin/env python3
"""
SPDX-License-Identifier: MIT

Zephyr Edge AI Demo - UART Log Collector

This script connects to the UART output from the Zephyr demo and
collects inference results and debug information. It can operate in
real-time display mode or save logs to a CSV file for later analysis.

Features:
- Auto-detect serial port
- Parse JSON-formatted messages
- Real-time display with color formatting
- CSV export for analysis
- Statistics summary

Usage:
    python uart_logger.py --port /dev/ttyACM0 --output results.csv
    python uart_logger.py --auto  # Auto-detect port
"""

import argparse
import csv
import json
import os
import sys
import time
from datetime import datetime
from typing import Optional, Dict, List, Any

try:
    import serial
    import serial.tools.list_ports
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False
    print("Warning: pyserial not installed. Install with: pip install pyserial")


class Colors:
    """ANSI color codes for terminal output."""
    RESET = '\033[0m'
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    MAGENTA = '\033[95m'
    CYAN = '\033[96m'
    BOLD = '\033[1m'


GESTURE_COLORS = {
    'IDLE': Colors.BLUE,
    'WAVE': Colors.GREEN,
    'TAP': Colors.YELLOW,
    'CIRCLE': Colors.MAGENTA,
}


class UARTLogger:
    """Collects and processes UART output from the Zephyr demo."""
    
    def __init__(self, port: str = None, baud: int = 115200):
        """
        Initialize the UART logger.
        
        Args:
            port: Serial port path (e.g., /dev/ttyACM0, COM3)
            baud: Baud rate (default 115200)
        """
        self.port = port
        self.baud = baud
        self.serial: Optional[serial.Serial] = None
        
        self.messages: List[Dict[str, Any]] = []
        self.inference_results: List[Dict[str, Any]] = []
        self.debug_stats: List[Dict[str, Any]] = []
        self.errors: List[Dict[str, Any]] = []
        
        self.start_time = datetime.now()
        self.running = False
        
    def auto_detect_port(self) -> Optional[str]:
        """
        Attempt to auto-detect the serial port.
        
        Returns:
            Detected port path or None
        """
        if not HAS_SERIAL:
            return None
            
        ports = list(serial.tools.list_ports.comports())
        
        # Look for common Zephyr/ARM debug probe identifiers
        candidates = []
        for port in ports:
            desc_lower = port.description.lower()
            if any(x in desc_lower for x in ['arm', 'cmsis', 'dap', 'stlink', 'jlink']):
                candidates.insert(0, port.device)
            elif any(x in desc_lower for x in ['usb', 'serial', 'uart']):
                candidates.append(port.device)
        
        if candidates:
            print(f"Auto-detected ports: {candidates}")
            return candidates[0]
        
        # Fallback: return first available port
        if ports:
            return ports[0].device
            
        return None
    
    def connect(self) -> bool:
        """
        Connect to the serial port.
        
        Returns:
            True if connected successfully
        """
        if not HAS_SERIAL:
            print("Error: pyserial not installed")
            return False
            
        try:
            if self.port is None:
                self.port = self.auto_detect_port()
                
            if self.port is None:
                print("Error: No serial port found")
                return False
                
            print(f"Connecting to {self.port} at {self.baud} baud...")
            self.serial = serial.Serial(
                port=self.port,
                baudrate=self.baud,
                timeout=1.0
            )
            print(f"Connected to {self.port}")
            return True
            
        except serial.SerialException as e:
            print(f"Error connecting to {self.port}: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from the serial port."""
        if self.serial and self.serial.is_open:
            self.serial.close()
            print("Disconnected")
    
    def parse_message(self, line: str) -> Optional[Dict[str, Any]]:
        """
        Parse a JSON message from the UART output.
        
        Args:
            line: Raw line from UART
            
        Returns:
            Parsed message dict or None
        """
        line = line.strip()
        
        if not line:
            return None
            
        # Try to parse as JSON
        try:
            if line.startswith('{'):
                msg = json.loads(line)
                msg['_raw'] = line
                msg['_received_at'] = datetime.now().isoformat()
                return msg
        except json.JSONDecodeError:
            pass
        
        # Non-JSON line (banner, debug logs, etc.)
        return {'type': 'raw', 'content': line}
    
    def process_message(self, msg: Dict[str, Any]):
        """
        Process a parsed message.
        
        Args:
            msg: Parsed message dict
        """
        self.messages.append(msg)
        
        msg_type = msg.get('type', 'unknown')
        
        if msg_type == 'inference':
            self.inference_results.append(msg)
            self.print_inference(msg)
            
        elif msg_type == 'debug':
            self.debug_stats.append(msg)
            self.print_debug(msg)
            
        elif msg_type == 'error':
            self.errors.append(msg)
            self.print_error(msg)
            
        elif msg_type == 'heartbeat':
            self.print_heartbeat(msg)
            
        elif msg_type == 'startup':
            self.print_startup(msg)
            
        elif msg_type == 'raw':
            print(msg.get('content', ''))
    
    def print_inference(self, msg: Dict[str, Any]):
        """Print an inference result with color formatting."""
        gesture = msg.get('gesture', 'UNKNOWN')
        conf = msg.get('conf', 0)
        latency = msg.get('latency_us', 0)
        seq = msg.get('seq', 0)
        
        color = GESTURE_COLORS.get(gesture, Colors.RESET)
        
        print(f"{Colors.BOLD}[{seq:4d}]{Colors.RESET} "
              f"Gesture: {color}{gesture:6s}{Colors.RESET} "
              f"Conf: {conf:.2f} "
              f"Latency: {latency:5d} µs")
    
    def print_debug(self, msg: Dict[str, Any]):
        """Print debug statistics."""
        heap_used = msg.get('heap_used', 0)
        heap_free = msg.get('heap_free', 0)
        stack_used = msg.get('stack_used', 0)
        stack_size = msg.get('stack_size', 0)
        cpu = msg.get('cpu_usage', 0)
        
        stack_pct = (stack_used / stack_size * 100) if stack_size > 0 else 0
        
        print(f"{Colors.CYAN}[DEBUG]{Colors.RESET} "
              f"Heap: {heap_used}/{heap_used + heap_free} "
              f"Stack: {stack_used}/{stack_size} ({stack_pct:.0f}%) "
              f"CPU: {cpu:.1f}%")
    
    def print_error(self, msg: Dict[str, Any]):
        """Print an error message."""
        code = msg.get('code', 0)
        message = msg.get('message', 'Unknown error')
        
        print(f"{Colors.RED}[ERROR]{Colors.RESET} Code {code}: {message}")
    
    def print_heartbeat(self, msg: Dict[str, Any]):
        """Print a heartbeat message."""
        uptime = msg.get('uptime_ms', 0)
        print(f"{Colors.BLUE}[HEARTBEAT]{Colors.RESET} Uptime: {uptime} ms")
    
    def print_startup(self, msg: Dict[str, Any]):
        """Print startup information."""
        version = msg.get('version', 'unknown')
        board = msg.get('board', 'unknown')
        
        print(f"{Colors.GREEN}{Colors.BOLD}")
        print(f"╔═══════════════════════════════════════╗")
        print(f"║  Zephyr Edge AI Demo Connected       ║")
        print(f"║  Version: {version:28s}║")
        print(f"║  Board:   {board:28s}║")
        print(f"╚═══════════════════════════════════════╝")
        print(f"{Colors.RESET}")
    
    def run(self, duration: Optional[int] = None):
        """
        Run the logger, collecting messages.
        
        Args:
            duration: Optional duration in seconds (None = run forever)
        """
        if not self.serial or not self.serial.is_open:
            print("Error: Not connected")
            return
            
        self.running = True
        self.start_time = datetime.now()
        
        print(f"\nLogging started at {self.start_time}")
        print("Press Ctrl+C to stop\n")
        print("-" * 60)
        
        try:
            while self.running:
                if duration and (datetime.now() - self.start_time).seconds >= duration:
                    break
                    
                if self.serial.in_waiting > 0:
                    try:
                        line = self.serial.readline().decode('utf-8', errors='replace')
                        msg = self.parse_message(line)
                        if msg:
                            self.process_message(msg)
                    except Exception as e:
                        print(f"Error reading line: {e}")
                        
        except KeyboardInterrupt:
            print("\n\nStopping...")
            
        self.running = False
        print("-" * 60)
        print(f"Logging stopped. Collected {len(self.messages)} messages.")
    
    def save_csv(self, filename: str):
        """
        Save inference results to a CSV file.
        
        Args:
            filename: Output CSV path
        """
        if not self.inference_results:
            print("No inference results to save")
            return
            
        fieldnames = ['seq', 'ts', 'gesture', 'conf', 'latency_us', 
                      'heap', 'stack', '_received_at']
        
        with open(filename, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames, 
                                    extrasaction='ignore')
            writer.writeheader()
            writer.writerows(self.inference_results)
            
        print(f"Saved {len(self.inference_results)} results to {filename}")
    
    def print_summary(self):
        """Print a summary of collected data."""
        print("\n" + "=" * 60)
        print("SUMMARY")
        print("=" * 60)
        
        print(f"\nTotal messages: {len(self.messages)}")
        print(f"Inferences:     {len(self.inference_results)}")
        print(f"Debug stats:    {len(self.debug_stats)}")
        print(f"Errors:         {len(self.errors)}")
        
        if self.inference_results:
            # Gesture distribution
            gestures = {}
            latencies = []
            
            for r in self.inference_results:
                g = r.get('gesture', 'UNKNOWN')
                gestures[g] = gestures.get(g, 0) + 1
                latencies.append(r.get('latency_us', 0))
            
            print("\nGesture Distribution:")
            for g, count in sorted(gestures.items()):
                pct = count / len(self.inference_results) * 100
                print(f"  {g:8s}: {count:4d} ({pct:5.1f}%)")
            
            if latencies:
                latencies.sort()
                print(f"\nLatency Statistics (µs):")
                print(f"  Min:  {min(latencies):8d}")
                print(f"  Max:  {max(latencies):8d}")
                print(f"  Avg:  {sum(latencies)//len(latencies):8d}")
                print(f"  P50:  {latencies[len(latencies)//2]:8d}")
                print(f"  P95:  {latencies[int(len(latencies)*0.95)]:8d}")


def simulate_mode():
    """
    Simulate UART input for testing without hardware.
    
    Reads from stdin instead of serial port.
    """
    print("Running in simulation mode (reading from stdin)")
    print("Paste JSON messages or press Ctrl+D to finish\n")
    
    logger = UARTLogger()
    
    try:
        for line in sys.stdin:
            msg = logger.parse_message(line)
            if msg:
                logger.process_message(msg)
    except KeyboardInterrupt:
        pass
    
    logger.print_summary()


def main():
    parser = argparse.ArgumentParser(
        description="Collect UART output from Zephyr Edge AI Demo"
    )
    parser.add_argument(
        '--port', '-p',
        help='Serial port (e.g., /dev/ttyACM0, COM3)'
    )
    parser.add_argument(
        '--baud', '-b',
        type=int, default=115200,
        help='Baud rate (default: 115200)'
    )
    parser.add_argument(
        '--auto', '-a',
        action='store_true',
        help='Auto-detect serial port'
    )
    parser.add_argument(
        '--output', '-o',
        help='Output CSV file for results'
    )
    parser.add_argument(
        '--duration', '-d',
        type=int,
        help='Duration in seconds (default: run until Ctrl+C)'
    )
    parser.add_argument(
        '--simulate', '-s',
        action='store_true',
        help='Simulation mode (read from stdin)'
    )
    
    args = parser.parse_args()
    
    if args.simulate:
        simulate_mode()
        return
    
    if not HAS_SERIAL:
        print("Error: pyserial is required. Install with: pip install pyserial")
        print("Or use --simulate mode to test without serial port")
        sys.exit(1)
    
    logger = UARTLogger(
        port=args.port if not args.auto else None,
        baud=args.baud
    )
    
    if not logger.connect():
        sys.exit(1)
    
    try:
        logger.run(duration=args.duration)
    finally:
        logger.disconnect()
        
    logger.print_summary()
    
    if args.output:
        logger.save_csv(args.output)


if __name__ == '__main__':
    main()
