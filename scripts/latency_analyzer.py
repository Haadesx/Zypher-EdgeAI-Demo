#!/usr/bin/env python3
"""
SPDX-License-Identifier: MIT

Zephyr Edge AI Demo - Latency Analyzer

Analyzes inference latency data collected by uart_logger.py
and generates visualization plots and statistical reports.

Features:
- Latency histogram
- Time series plot
- Percentile analysis
- Gesture-specific breakdown
- Memory usage correlation
- Export to PDF/PNG

Usage:
    python latency_analyzer.py --input results.csv --output report.png
"""

import argparse
import csv
import json
import os
import sys
from collections import defaultdict
from typing import List, Dict, Any, Optional, Tuple

# Try to import matplotlib (optional)
try:
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False


class LatencyAnalyzer:
    """Analyzes inference latency data from the Zephyr Edge AI demo."""
    
    def __init__(self):
        self.data: List[Dict[str, Any]] = []
        self.latencies: List[int] = []
        self.gestures: Dict[str, List[int]] = defaultdict(list)
        self.memory_usage: List[Tuple[int, int]] = []  # (heap, stack)
        
    def load_csv(self, filename: str) -> bool:
        """
        Load inference results from CSV file.
        
        Args:
            filename: Path to CSV file
            
        Returns:
            True if loaded successfully
        """
        try:
            with open(filename, 'r') as f:
                reader = csv.DictReader(f)
                for row in reader:
                    self.data.append(row)
                    
                    # Extract latency
                    latency = int(row.get('latency_us', 0))
                    self.latencies.append(latency)
                    
                    # Group by gesture
                    gesture = row.get('gesture', 'UNKNOWN')
                    self.gestures[gesture].append(latency)
                    
                    # Memory usage
                    heap = int(row.get('heap', 0))
                    stack = int(row.get('stack', 0))
                    self.memory_usage.append((heap, stack))
                    
            print(f"Loaded {len(self.data)} records from {filename}")
            return True
            
        except FileNotFoundError:
            print(f"Error: File not found: {filename}")
            return False
        except Exception as e:
            print(f"Error loading {filename}: {e}")
            return False
    
    def load_json_stream(self, lines: List[str]):
        """
        Load data from JSON lines (e.g., from stdin or log file).
        
        Args:
            lines: List of JSON lines
        """
        for line in lines:
            line = line.strip()
            if not line or not line.startswith('{'):
                continue
                
            try:
                msg = json.loads(line)
                if msg.get('type') == 'inference':
                    self.data.append(msg)
                    
                    latency = msg.get('latency_us', 0)
                    self.latencies.append(latency)
                    
                    gesture = msg.get('gesture', 'UNKNOWN')
                    self.gestures[gesture].append(latency)
                    
                    heap = msg.get('heap', 0)
                    stack = msg.get('stack', 0)
                    self.memory_usage.append((heap, stack))
                    
            except json.JSONDecodeError:
                pass
                
        print(f"Loaded {len(self.data)} inference records")
    
    def compute_percentile(self, values: List[int], p: float) -> int:
        """Compute the p-th percentile of values."""
        if not values:
            return 0
        sorted_v = sorted(values)
        idx = int(len(sorted_v) * p / 100)
        return sorted_v[min(idx, len(sorted_v) - 1)]
    
    def compute_stats(self, values: List[int]) -> Dict[str, Any]:
        """Compute statistics for a list of values."""
        if not values:
            return {
                'count': 0,
                'min': 0,
                'max': 0,
                'avg': 0,
                'p50': 0,
                'p95': 0,
                'p99': 0,
                'std': 0
            }
        
        sorted_v = sorted(values)
        n = len(values)
        avg = sum(values) / n
        
        # Standard deviation
        variance = sum((x - avg) ** 2 for x in values) / n
        std = variance ** 0.5
        
        return {
            'count': n,
            'min': min(values),
            'max': max(values),
            'avg': avg,
            'p50': sorted_v[n // 2],
            'p95': sorted_v[int(n * 0.95)],
            'p99': sorted_v[int(n * 0.99)],
            'std': std
        }
    
    def print_report(self):
        """Print a text-based analysis report."""
        if not self.latencies:
            print("No data to analyze")
            return
        
        print("\n" + "=" * 70)
        print("INFERENCE LATENCY ANALYSIS REPORT")
        print("=" * 70)
        
        # Overall statistics
        stats = self.compute_stats(self.latencies)
        
        print("\n## Overall Latency Statistics (µs)")
        print("-" * 40)
        print(f"  Count:     {stats['count']:,}")
        print(f"  Minimum:   {stats['min']:,}")
        print(f"  Maximum:   {stats['max']:,}")
        print(f"  Average:   {stats['avg']:,.1f}")
        print(f"  Std Dev:   {stats['std']:,.1f}")
        print(f"  Median:    {stats['p50']:,}")
        print(f"  P95:       {stats['p95']:,}")
        print(f"  P99:       {stats['p99']:,}")
        
        # Per-gesture breakdown
        print("\n## Latency by Gesture (µs)")
        print("-" * 40)
        print(f"  {'Gesture':<10} {'Count':>8} {'Avg':>10} {'P50':>10} {'P95':>10}")
        print("  " + "-" * 48)
        
        for gesture, latencies in sorted(self.gestures.items()):
            g_stats = self.compute_stats(latencies)
            print(f"  {gesture:<10} {g_stats['count']:>8,} "
                  f"{g_stats['avg']:>10,.1f} "
                  f"{g_stats['p50']:>10,} "
                  f"{g_stats['p95']:>10,}")
        
        # Memory usage
        if self.memory_usage:
            heaps = [h for h, s in self.memory_usage if h > 0]
            stacks = [s for h, s in self.memory_usage if s > 0]
            
            if heaps:
                print("\n## Memory Usage")
                print("-" * 40)
                print(f"  Heap:  avg={sum(heaps)//len(heaps):,} bytes, "
                      f"max={max(heaps):,} bytes")
            if stacks:
                print(f"  Stack: avg={sum(stacks)//len(stacks):,} bytes, "
                      f"max={max(stacks):,} bytes")
        
        # Distribution histogram (text-based)
        print("\n## Latency Distribution")
        print("-" * 40)
        
        # Create buckets
        min_lat = stats['min']
        max_lat = stats['max']
        bucket_size = max(1, (max_lat - min_lat) // 10)
        buckets = defaultdict(int)
        
        for lat in self.latencies:
            bucket = ((lat - min_lat) // bucket_size) * bucket_size + min_lat
            buckets[bucket] += 1
        
        max_count = max(buckets.values()) if buckets else 1
        
        for bucket in sorted(buckets.keys()):
            count = buckets[bucket]
            bar_len = int(count / max_count * 40)
            bar = '█' * bar_len
            print(f"  {bucket:>8,}-{bucket+bucket_size:>8,}: {bar} ({count})")
        
        print("\n" + "=" * 70)
    
    def plot_report(self, output_file: Optional[str] = None):
        """
        Generate visualization plots.
        
        Args:
            output_file: Path to save plot (None = display)
        """
        if not HAS_MATPLOTLIB:
            print("Matplotlib not available. Install with: pip install matplotlib")
            return
        
        if not self.latencies:
            print("No data to plot")
            return
        
        fig, axes = plt.subplots(2, 2, figsize=(14, 10))
        fig.suptitle('Zephyr Edge AI Demo - Latency Analysis', fontsize=14, fontweight='bold')
        
        # 1. Histogram
        ax1 = axes[0, 0]
        ax1.hist(self.latencies, bins=30, edgecolor='black', alpha=0.7, color='steelblue')
        ax1.axvline(x=sum(self.latencies)/len(self.latencies), color='red', 
                    linestyle='--', label=f'Mean: {sum(self.latencies)//len(self.latencies)} µs')
        ax1.axvline(x=sorted(self.latencies)[len(self.latencies)//2], color='green', 
                    linestyle='--', label=f'Median: {sorted(self.latencies)[len(self.latencies)//2]} µs')
        ax1.set_xlabel('Latency (µs)')
        ax1.set_ylabel('Count')
        ax1.set_title('Latency Distribution')
        ax1.legend()
        ax1.grid(True, alpha=0.3)
        
        # 2. Time series
        ax2 = axes[0, 1]
        ax2.plot(range(len(self.latencies)), self.latencies, alpha=0.7, linewidth=0.8)
        ax2.set_xlabel('Inference Number')
        ax2.set_ylabel('Latency (µs)')
        ax2.set_title('Latency Over Time')
        ax2.grid(True, alpha=0.3)
        
        # 3. Per-gesture box plot
        ax3 = axes[1, 0]
        gesture_data = [self.gestures[g] for g in sorted(self.gestures.keys())]
        gesture_labels = sorted(self.gestures.keys())
        
        colors = ['#4CAF50', '#2196F3', '#FF9800', '#9C27B0']
        bp = ax3.boxplot(gesture_data, labels=gesture_labels, patch_artist=True)
        
        for patch, color in zip(bp['boxes'], colors[:len(gesture_labels)]):
            patch.set_facecolor(color)
            patch.set_alpha(0.7)
        
        ax3.set_xlabel('Gesture')
        ax3.set_ylabel('Latency (µs)')
        ax3.set_title('Latency by Gesture Type')
        ax3.grid(True, alpha=0.3)
        
        # 4. Percentile plot
        ax4 = axes[1, 1]
        percentiles = [50, 75, 90, 95, 99]
        p_values = [self.compute_percentile(self.latencies, p) for p in percentiles]
        
        bars = ax4.bar([f'P{p}' for p in percentiles], p_values, 
                       color=['#4CAF50', '#8BC34A', '#FFC107', '#FF9800', '#F44336'],
                       edgecolor='black', alpha=0.8)
        
        for bar, val in zip(bars, p_values):
            ax4.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 100,
                    f'{val:,}', ha='center', va='bottom', fontsize=9)
        
        ax4.set_xlabel('Percentile')
        ax4.set_ylabel('Latency (µs)')
        ax4.set_title('Latency Percentiles')
        ax4.grid(True, alpha=0.3, axis='y')
        
        plt.tight_layout()
        
        if output_file:
            plt.savefig(output_file, dpi=150, bbox_inches='tight')
            print(f"Plot saved to {output_file}")
        else:
            plt.show()


def main():
    parser = argparse.ArgumentParser(
        description="Analyze inference latency data from Zephyr Edge AI Demo"
    )
    parser.add_argument(
        '--input', '-i',
        help='Input CSV file from uart_logger.py'
    )
    parser.add_argument(
        '--output', '-o',
        help='Output plot file (PNG or PDF)'
    )
    parser.add_argument(
        '--no-plot',
        action='store_true',
        help='Skip plot generation (text report only)'
    )
    
    args = parser.parse_args()
    
    analyzer = LatencyAnalyzer()
    
    if args.input:
        if not analyzer.load_csv(args.input):
            sys.exit(1)
    else:
        # Read from stdin
        print("No input file specified. Reading from stdin...")
        print("Paste JSON logs and press Ctrl+D when done.\n")
        lines = sys.stdin.readlines()
        analyzer.load_json_stream(lines)
    
    if not analyzer.latencies:
        print("No latency data found")
        sys.exit(1)
    
    # Print text report
    analyzer.print_report()
    
    # Generate plot
    if not args.no_plot:
        if HAS_MATPLOTLIB:
            analyzer.plot_report(args.output)
        else:
            print("\nNote: Install matplotlib for visualization: pip install matplotlib")


if __name__ == '__main__':
    main()
