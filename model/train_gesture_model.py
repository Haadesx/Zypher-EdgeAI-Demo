#!/usr/bin/env python3
"""
Zephyr Edge AI Demo - Gesture Recognition Model Training

This script:
1. Generates synthetic accelerometer data for 4 gesture classes
2. Trains a SIMPLE Dense-layer-only neural network (TFLite-Micro compatible)
3. Converts to TFLite with INT8 quantization
4. Exports as C header for embedding in Zephyr

Features beautiful progress visualization using Rich!
"""

import numpy as np
import tensorflow as tf
from pathlib import Path
import time

# Rich for beautiful console output
from rich.console import Console
from rich.progress import Progress, SpinnerColumn, BarColumn, TextColumn, TimeElapsedColumn
from rich.panel import Panel
from rich.table import Table
from rich.live import Live
from rich import box
from tqdm.keras import TqdmCallback

console = Console()

# Configuration
NUM_SAMPLES = 50        # Samples per window
NUM_AXES = 3            # X, Y, Z accelerometer axes
NUM_CLASSES = 4         # IDLE, WAVE, TAP, CIRCLE
EXAMPLES_PER_CLASS = 2000  # Training examples per class
VALIDATION_SPLIT = 0.2

# Class labels (must match gesture_types.h)
GESTURE_LABELS = ['IDLE', 'WAVE', 'TAP', 'CIRCLE']
GESTURE_EMOJIS = ['😴', '👋', '👆', '⭕']


def generate_idle_data(num_examples, progress, task_id):
    """Generate idle state - small random noise"""
    data = np.random.randn(num_examples, NUM_SAMPLES, NUM_AXES) * 0.05
    data[:, :, 2] += 1.0
    progress.update(task_id, advance=num_examples)
    return data


def generate_wave_data(num_examples, progress, task_id):
    """Generate wave gesture - sinusoidal motion on X and Y"""
    data = np.zeros((num_examples, NUM_SAMPLES, NUM_AXES))
    t = np.linspace(0, 2 * np.pi, NUM_SAMPLES)
    
    for i in range(num_examples):
        freq = np.random.uniform(1.5, 3.0)
        amp_x = np.random.uniform(0.8, 1.5)
        amp_y = np.random.uniform(0.5, 1.0)
        phase = np.random.uniform(0, 2 * np.pi)
        
        data[i, :, 0] = amp_x * np.sin(freq * t + phase)
        data[i, :, 1] = amp_y * np.sin(freq * t + phase + np.pi/4)
        data[i, :, 2] = 1.0 + np.random.randn(NUM_SAMPLES) * 0.1
        
        if i % 100 == 0:
            progress.update(task_id, advance=100)
    
    progress.update(task_id, advance=num_examples % 100)
    data += np.random.randn(*data.shape) * 0.05
    return data


def generate_tap_data(num_examples, progress, task_id):
    """Generate tap gesture - sharp spike followed by decay"""
    data = np.zeros((num_examples, NUM_SAMPLES, NUM_AXES))
    
    for i in range(num_examples):
        tap_pos = np.random.randint(10, 30)
        spike_amp = np.random.uniform(2.0, 4.0)
        decay = np.random.uniform(0.7, 0.9)
        
        for j in range(NUM_SAMPLES):
            if j >= tap_pos:
                dist = j - tap_pos
                data[i, j, 2] = 1.0 + spike_amp * (decay ** dist)
            else:
                data[i, j, 2] = 1.0
        
        data[i, :, 0] = np.random.randn(NUM_SAMPLES) * 0.2
        data[i, :, 1] = np.random.randn(NUM_SAMPLES) * 0.2
        
        if i % 100 == 0:
            progress.update(task_id, advance=100)
    
    progress.update(task_id, advance=num_examples % 100)
    data += np.random.randn(*data.shape) * 0.03
    return data


def generate_circle_data(num_examples, progress, task_id):
    """Generate circle gesture - circular motion in X-Y plane"""
    data = np.zeros((num_examples, NUM_SAMPLES, NUM_AXES))
    t = np.linspace(0, 2 * np.pi, NUM_SAMPLES)
    
    for i in range(num_examples):
        radius = np.random.uniform(0.6, 1.2)
        freq = np.random.uniform(0.8, 1.5)
        direction = np.random.choice([-1, 1])
        
        data[i, :, 0] = radius * np.cos(freq * t) * direction
        data[i, :, 1] = radius * np.sin(freq * t)
        data[i, :, 2] = 1.0 + np.random.randn(NUM_SAMPLES) * 0.1
        
        if i % 100 == 0:
            progress.update(task_id, advance=100)
    
    progress.update(task_id, advance=num_examples % 100)
    data += np.random.randn(*data.shape) * 0.05
    return data


def create_dataset():
    """Create training and validation datasets with progress visualization"""
    console.print("\n[bold cyan]📊 Generating Synthetic Gesture Data[/bold cyan]")
    
    with Progress(
        SpinnerColumn(),
        TextColumn("[progress.description]{task.description}"),
        BarColumn(bar_width=40),
        TextColumn("[progress.percentage]{task.percentage:>3.0f}%"),
        TimeElapsedColumn(),
        console=console,
    ) as progress:
        
        total = EXAMPLES_PER_CLASS * 4
        
        idle_task = progress.add_task(f"😴 IDLE gestures", total=EXAMPLES_PER_CLASS)
        wave_task = progress.add_task(f"👋 WAVE gestures", total=EXAMPLES_PER_CLASS)
        tap_task = progress.add_task(f"👆 TAP gestures", total=EXAMPLES_PER_CLASS)
        circle_task = progress.add_task(f"⭕ CIRCLE gestures", total=EXAMPLES_PER_CLASS)
        
        idle_data = generate_idle_data(EXAMPLES_PER_CLASS, progress, idle_task)
        wave_data = generate_wave_data(EXAMPLES_PER_CLASS, progress, wave_task)
        tap_data = generate_tap_data(EXAMPLES_PER_CLASS, progress, tap_task)
        circle_data = generate_circle_data(EXAMPLES_PER_CLASS, progress, circle_task)
    
    # Combine all data
    X = np.vstack([idle_data, wave_data, tap_data, circle_data])
    y = np.array([0] * EXAMPLES_PER_CLASS + 
                 [1] * EXAMPLES_PER_CLASS + 
                 [2] * EXAMPLES_PER_CLASS + 
                 [3] * EXAMPLES_PER_CLASS)
    
    # Shuffle
    indices = np.random.permutation(len(X))
    X, y = X[indices], y[indices]
    
    # FLATTEN for Dense-only model (50*3 = 150 features)
    X = X.reshape(X.shape[0], -1)
    
    # Split
    split_idx = int(len(X) * (1 - VALIDATION_SPLIT))
    X_train, X_val = X[:split_idx], X[split_idx:]
    y_train, y_val = y[:split_idx], y[split_idx:]
    
    # Summary table
    table = Table(title="Dataset Summary", box=box.ROUNDED)
    table.add_column("Split", style="cyan")
    table.add_column("Samples", justify="right", style="green")
    table.add_column("Shape", justify="right", style="yellow")
    table.add_row("Training", f"{len(X_train):,}", str(X_train.shape))
    table.add_row("Validation", f"{len(X_val):,}", str(X_val.shape))
    console.print(table)
    
    return (X_train, y_train), (X_val, y_val)


def create_model():
    """Create a SIMPLE Dense-only model for TFLite-Micro compatibility"""
    model = tf.keras.Sequential([
        tf.keras.layers.InputLayer(input_shape=(NUM_SAMPLES * NUM_AXES,)),
        tf.keras.layers.Dense(32, activation='relu'),
        tf.keras.layers.Dense(16, activation='relu'),
        tf.keras.layers.Dense(NUM_CLASSES, activation='softmax')
    ])
    
    model.compile(
        optimizer='adam',
        loss='sparse_categorical_crossentropy',
        metrics=['accuracy']
    )
    
    return model


class RichProgressCallback(tf.keras.callbacks.Callback):
    """Custom callback for Rich progress visualization during training"""
    
    def __init__(self, epochs, console):
        super().__init__()
        self.epochs = epochs
        self.console = console
        self.progress = None
        self.epoch_task = None
        self.batch_task = None
        
    def on_train_begin(self, logs=None):
        self.console.print("\n[bold cyan]🧠 Training Neural Network[/bold cyan]")
        self.progress = Progress(
            SpinnerColumn(),
            TextColumn("[progress.description]{task.description}"),
            BarColumn(bar_width=30),
            TextColumn("[progress.percentage]{task.percentage:>3.0f}%"),
            TextColumn("•"),
            TextColumn("[cyan]acc: {task.fields[acc]:.1%}[/cyan]"),
            TextColumn("[yellow]val: {task.fields[val]:.1%}[/yellow]"),
            TimeElapsedColumn(),
            console=self.console,
        )
        self.progress.start()
        self.epoch_task = self.progress.add_task(
            "Training epochs",
            total=self.epochs,
            acc=0.0,
            val=0.0
        )
        
    def on_epoch_end(self, epoch, logs=None):
        acc = logs.get('accuracy', 0)
        val_acc = logs.get('val_accuracy', 0)
        self.progress.update(
            self.epoch_task,
            advance=1,
            acc=acc,
            val=val_acc
        )
        
    def on_train_end(self, logs=None):
        self.progress.stop()


def quantize_model(model, X_train):
    """Convert model to INT8 quantized TFLite format"""
    console.print("\n[bold cyan]⚡ Quantizing Model to INT8[/bold cyan]")
    
    with Progress(
        SpinnerColumn(),
        TextColumn("[progress.description]{task.description}"),
        BarColumn(bar_width=40),
        TextColumn("[progress.percentage]{task.percentage:>3.0f}%"),
        console=console,
    ) as progress:
        
        task = progress.add_task("Quantization", total=100)
        
        def representative_dataset():
            for i in range(min(500, len(X_train))):
                yield [X_train[i:i+1].astype(np.float32)]
        
        progress.update(task, advance=20)
        
        converter = tf.lite.TFLiteConverter.from_keras_model(model)
        converter._experimental_disable_per_channel_quantization_for_dense_layers = True
        converter.optimizations = [tf.lite.Optimize.DEFAULT]
        converter.representative_dataset = representative_dataset
        converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
        converter.inference_input_type = tf.int8
        converter.inference_output_type = tf.int8
        
        progress.update(task, advance=30)
        
        # Suppress TensorFlow warnings during conversion
        import logging
        logging.getLogger('tensorflow').setLevel(logging.ERROR)
        
        tflite_model = converter.convert()
        
        progress.update(task, advance=50)
    
    console.print(f"   [green]✓[/green] Quantized model size: [bold]{len(tflite_model):,}[/bold] bytes")
    
    return tflite_model


def export_to_c_header(tflite_model, output_path):
    """Export TFLite model as C header file"""
    header_content = f'''/*
 * SPDX-License-Identifier: MIT
 *
 * Zephyr Edge AI Demo - Gesture Recognition Model Data
 *
 * THIS FILE IS AUTO-GENERATED - DO NOT EDIT
 * Generated by: model/train_gesture_model.py
 *
 * Model: Gesture classification (IDLE, WAVE, TAP, CIRCLE)
 * Input: 150 INT8 values (50 samples x 3 axes, flattened)
 * Output: 4 class probabilities (INT8)
 * Architecture: Dense(32) -> Dense(16) -> Dense(4)
 * Size: {len(tflite_model)} bytes
 */

#ifndef GESTURE_MODEL_H
#define GESTURE_MODEL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {{
#endif

/* Model data array */
extern const unsigned char gesture_model_data[];

/* Model size in bytes */
extern const size_t gesture_model_data_len;

#ifdef __cplusplus
}}
#endif

#endif /* GESTURE_MODEL_H */
'''
    
    with open(output_path, 'w') as f:
        f.write(header_content)


def export_to_c_source(tflite_model, output_path):
    """Export TFLite model as C source file"""
    source_content = '''/*
 * SPDX-License-Identifier: MIT
 *
 * Zephyr Edge AI Demo - Gesture Recognition Model Data
 *
 * THIS FILE IS AUTO-GENERATED - DO NOT EDIT
 * Generated by: model/train_gesture_model.py
 */

#include "gesture_model.h"

/* Gesture recognition model - INT8 quantized TFLite
 * Dense-only architecture for TFLite-Micro compatibility
 */
const unsigned char gesture_model_data[] __attribute__((aligned(4))) = {
'''
    
    bytes_per_line = 12
    for i in range(0, len(tflite_model), bytes_per_line):
        chunk = tflite_model[i:i+bytes_per_line]
        hex_values = ', '.join(f'0x{b:02x}' for b in chunk)
        source_content += f'    {hex_values},\n'
    
    source_content += f'''}};

/* Model size */
const size_t gesture_model_data_len = {len(tflite_model)};
'''
    
    with open(output_path, 'w') as f:
        f.write(source_content)


def main():
    # Beautiful header
    console.print(Panel.fit(
        "[bold white]🤖 Zephyr Edge AI Demo[/bold white]\n"
        "[cyan]Gesture Recognition Model Training[/cyan]\n"
        "[dim]TensorFlow Lite Micro Compatible[/dim]",
        border_style="cyan",
        padding=(1, 4)
    ))
    
    # Set random seed for reproducibility
    np.random.seed(42)
    tf.random.set_seed(42)
    
    # Suppress TensorFlow info messages
    import os
    os.environ['TF_CPP_MIN_LOG_LEVEL'] = '2'
    
    # Create dataset
    (X_train, y_train), (X_val, y_val) = create_dataset()
    
    # Create model
    console.print("\n[bold cyan]🏗️  Building Model Architecture[/bold cyan]")
    model = create_model()
    
    # Model summary table
    table = Table(title="Model Architecture", box=box.ROUNDED)
    table.add_column("Layer", style="cyan")
    table.add_column("Type", justify="center", style="green")
    table.add_column("Params", justify="right", style="yellow")
    
    table.add_row("Input", "InputLayer", "0")
    table.add_row("Dense 1", "Dense(32, relu)", f"{150*32 + 32:,}")
    table.add_row("Dense 2", "Dense(16, relu)", f"{32*16 + 16:,}")
    table.add_row("Output", "Dense(4, softmax)", f"{16*4 + 4:,}")
    table.add_row("[bold]Total[/bold]", "", f"[bold]{model.count_params():,}[/bold]")
    console.print(table)
    
    # Training
    callback = RichProgressCallback(epochs=30, console=console)
    
    history = model.fit(
        X_train, y_train,
        validation_data=(X_val, y_val),
        epochs=30,
        batch_size=32,
        verbose=0,
        callbacks=[callback]
    )
    
    # Evaluation
    console.print("\n[bold cyan]📈 Evaluation Results[/bold cyan]")
    val_loss, val_acc = model.evaluate(X_val, y_val, verbose=0)
    
    # Results table
    table = Table(box=box.ROUNDED)
    table.add_column("Metric", style="cyan")
    table.add_column("Value", justify="right", style="green")
    table.add_row("Validation Accuracy", f"[bold green]{val_acc:.2%}[/bold green]")
    table.add_row("Validation Loss", f"{val_loss:.6f}")
    console.print(table)
    
    # Sample predictions
    console.print("\n[bold cyan]🔍 Sample Predictions[/bold cyan]")
    table = Table(box=box.ROUNDED)
    table.add_column("Sample", style="dim")
    table.add_column("Actual", style="cyan")
    table.add_column("Predicted", style="green")
    table.add_column("Confidence", justify="right", style="yellow")
    
    for i in [0, 400, 800, 1200]:
        if i < len(X_val):
            pred = model.predict(X_val[i:i+1], verbose=0)
            predicted_class = np.argmax(pred)
            actual_class = y_val[i]
            conf = pred[0][predicted_class]
            
            actual_str = f"{GESTURE_EMOJIS[actual_class]} {GESTURE_LABELS[actual_class]}"
            pred_str = f"{GESTURE_EMOJIS[predicted_class]} {GESTURE_LABELS[predicted_class]}"
            
            table.add_row(f"#{i}", actual_str, pred_str, f"{conf:.1%}")
    
    console.print(table)
    
    # Quantize
    tflite_model = quantize_model(model, X_train)
    
    # Export
    console.print("\n[bold cyan]💾 Exporting Model Files[/bold cyan]")
    
    script_dir = Path(__file__).parent
    tflite_path = script_dir / 'gesture_model.tflite'
    with open(tflite_path, 'wb') as f:
        f.write(tflite_model)
    
    src_dir = script_dir.parent / 'src' / 'ml'
    export_to_c_header(tflite_model, src_dir / 'gesture_model.h')
    export_to_c_source(tflite_model, src_dir / 'gesture_model.c')
    
    # File summary
    table = Table(box=box.ROUNDED)
    table.add_column("File", style="cyan")
    table.add_column("Size", justify="right", style="green")
    table.add_row("gesture_model.tflite", f"{len(tflite_model):,} bytes")
    table.add_row("gesture_model.c", f"{(src_dir / 'gesture_model.c').stat().st_size:,} bytes")
    table.add_row("gesture_model.h", f"{(src_dir / 'gesture_model.h').stat().st_size:,} bytes")
    console.print(table)
    
    # Final summary
    console.print(Panel.fit(
        "[bold green]✅ Model Training Complete![/bold green]\n\n"
        f"[white]Model Size:[/white] [bold]{len(tflite_model):,}[/bold] bytes\n"
        f"[white]Accuracy:[/white] [bold green]{val_acc:.1%}[/bold green]\n"
        f"[white]TFLite Ops:[/white] FullyConnected, Relu, Softmax\n\n"
        "[dim]Next steps:[/dim]\n"
        "  [cyan]1.[/cyan] west build -b mps2/an385\n"
        "  [cyan]2.[/cyan] west build -t run",
        title="🎉 Success",
        border_style="green",
        padding=(1, 2)
    ))


if __name__ == '__main__':
    main()
