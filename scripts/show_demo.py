#!/usr/bin/env python3
"""
Zephyr Edge AI Demo - Real-time Dashboard
Wraps the QEMU simulation and visualizes inference results in a high-fidelity dashboard.
"""

import sys
import subprocess
import json
import re
import threading
import queue
import time
from collections import deque
from datetime import datetime

from rich.console import Console
from rich.layout import Layout
from rich.panel import Panel
from rich.live import Live
from rich.text import Text
from rich.table import Table
from rich.align import Align
from rich import box

# Configuration
MAX_LOG_LINES = 15
MAX_LATENCY_HISTORY = 30

# Gesture Mapping
GESTURES = {
    "IDLE": {"emoji": "😴", "color": "dim white", "text": "IDLE STATE"},
    "WAVE": {"emoji": "👋", "color": "bold cyan", "text": "WAVE DETECTED"},
    "TAP": {"emoji": "👆", "color": "bold yellow", "text": "TAP DETECTED"},
    "CIRCLE": {"emoji": "⭕", "color": "bold green", "text": "CIRCLE DETECTED"},
    "UNKNOWN": {"emoji": "❓", "color": "red", "text": "UNKNOWN"}
}

class DashboardData:
    def __init__(self):
        self.current_gesture = "IDLE"
        self.last_gesture_time = time.time()
        self.confidence = 0.0
        self.latency_us = 0
        self.stack_used = 0
        self.stack_size = 2048
        self.inference_count = 0
        self.logs = deque(maxlen=MAX_LOG_LINES)
        self.latencies = deque(maxlen=MAX_LATENCY_HISTORY)
        self.is_running = True

data = DashboardData()
console = Console()

def parse_line(line):
    """Parse a line of output from QEMU"""
    line = line.strip()
    if not line:
        return

    # Add to raw logs
    timestamp = datetime.now().strftime("%H:%M:%S")
    
    # Try parsing JSON
    try:
        if line.startswith("{") and line.endswith("}"):
            payload = json.loads(line)
            
            if payload.get("type") == "inference":
                data.current_gesture = payload.get("gesture", "UNKNOWN")
                data.last_gesture_time = time.time()
                data.confidence = payload.get("conf", 0.0)
                data.latency_us = payload.get("latency_us", 0)
                data.stack_used = payload.get("stack", 0)
                data.inference_count += 1
                data.latencies.append(data.latency_us)
                
                # Add formatted log
                msg = f"[{timestamp}] Inference #{payload['seq']}: {data.current_gesture} ({data.confidence:.1%})"
                if data.current_gesture != "IDLE":
                    data.logs.append(f"[bold]{msg}[/bold]")
                else:
                    data.logs.append(f"[dim]{msg}[/dim]")
                    
            elif payload.get("type") == "error":
                data.logs.append(f"[bold red][{timestamp}] ERROR: {payload.get('message')}[/bold red]")
                
        else:
            # Regular log line
            # Clean up ANSI codes if present
            clean_line = re.sub(r'\x1b\[[0-9;]*m', '', line)
            
            # Highlight interesting logs
            if "mock_accel: Starting" in clean_line:
                 data.logs.append(f"[yellow][{timestamp}] {clean_line}[/yellow]")
            elif "main: Starting" in clean_line:
                 data.logs.append(f"[bold green][{timestamp}] {clean_line}[/bold green]")
            else:
                 data.logs.append(f"[dim][{timestamp}] {clean_line}[/dim]")
                 
    except Exception:
        # Fallback for parsing unexpected text
        data.logs.append(f"[dim]{line}[/dim]")

def runner_thread():
    """Background thread to run QEMU and capture output"""
    # Run QEMU directly to avoid 'west' buffering and lock file issues
    # We use the path found on the system or fallback to default
    qemu_bin = "/opt/homebrew/bin/qemu-system-arm"
    
    cmd = [
        qemu_bin,
        "-cpu", "cortex-m3", 
        "-machine", "mps2-an385", 
        "-nographic", 
        "-vga", "none", 
        "-net", "none",
        "-pidfile", "qemu.pid",
        "-chardev", "stdio,id=con,mux=on",
        "-serial", "chardev:con",
        "-mon", "chardev=con,mode=readline",
        "-icount", "shift=7,align=off,sleep=off",
        "-rtc", "clock=vm",
        "-kernel", "build/zephyr/zephyr.elf"
    ]
    
    # Fallback if specific binary not found
    try:
        subprocess.run([qemu_bin, "--version"], capture_output=True, check=True)
    except (FileNotFoundError, subprocess.CalledProcessError):
        cmd[0] = "qemu-system-arm"  # Try generic name in PATH
    
    process = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,            # Line buffered
        universal_newlines=True
    )
    
    while data.is_running and process.poll() is None:
        line = process.stdout.readline()
        if line:
            parse_line(line)
    
    if process.poll() is None:
        process.terminate()

def generate_layout():
    """Create the rich layout tree"""
    layout = Layout()
    
    layout.split_column(
        Layout(name="header", size=3),
        Layout(name="main", ratio=1),
        Layout(name="logs", size=MAX_LOG_LINES + 2)
    )
    
    layout["main"].split_row(
        Layout(name="gesture", ratio=2),
        Layout(name="stats", ratio=1)
    )
    
    return layout

def render_header():
    return Panel(
        Align.center("[bold white]🤖 Zephyr Edge AI Demo - Live Inference Dashboard[/bold white]"),
        style="cyan",
        box=box.ROUNDED
    )

def render_gesture_panel():
    g = GESTURES.get(data.current_gesture, GESTURES["UNKNOWN"])
    
    # Check if signal is stale (no inference for >2 seconds)
    if time.time() - data.last_gesture_time > 2.0:
        g = {"emoji": "⏳", "color": "dim white", "text": "WAITING..."}
    
    content = Align.center(
        f"\n\n\n[size=80]{g['emoji']}[/size]\n\n"
        f"[{g['color']}][size=20]{g['text']}[/size][/{g['color']}]\n"
        f"[dim]Confidence: {data.confidence:.1%}[/dim]"
    )
    
    return Panel(
        content,
        title="[bold]Detected Gesture[/bold]",
        border_style=g['color'].split()[-1],  # Extract color name
        box=box.DOUBLE
    )

def render_stats_panel():
    avg_latency = sum(data.latencies) / len(data.latencies) if data.latencies else 0
    stack_pct = (data.stack_used / data.stack_size) * 100
    
    table = Table.grid(padding=1)
    table.add_column(style="dim", justify="right")
    table.add_column(style="bold white")
    
    table.add_row("Inferences:", f"{data.inference_count:,}")
    table.add_row("Latency:", f"{data.latency_us/1000:.1f} ms")
    table.add_row("Avg Latency:", f"{avg_latency/1000:.1f} ms")
    table.add_row("Stack Used:", f"{data.stack_used} B ({stack_pct:.0f}%)")
    table.add_row("Status:", "[green]Running[/green]" if data.is_running else "[red]Stopped[/red]")

    return Panel(
        Align.center(table, vertical="middle"),
        title="[bold]Performance Stats[/bold]",
        box=box.ROUNDED
    )

def render_logs_panel():
    log_text = "\n".join(data.logs)
    return Panel(
        log_text,
        title="[bold]Device Logs (UART)[/bold]",
        box=box.ROUNDED,
        style="white"
    )

def main():
    # Start runner thread
    t = threading.Thread(target=runner_thread, daemon=True)
    t.start()
    
    layout = generate_layout()
    
    try:
        with Live(layout, refresh_per_second=10, screen=True) as live:
            while True:
                layout["header"].update(render_header())
                layout["gesture"].update(render_gesture_panel())
                layout["stats"].update(render_stats_panel())
                layout["logs"].update(render_logs_panel())
                time.sleep(0.1)
                
    except KeyboardInterrupt:
        data.is_running = False
        console.print("\n[bold yellow]Stopping demo...[/bold yellow]")
        time.sleep(1) # Allow thread to clean up

if __name__ == "__main__":
    main()
