#!/bin/bash
# One-line setup script for Zypher EdgeAI Demo
set -e

echo "🚀 Setting up Zypher EdgeAI Demo..."

# 1. Create virtual environment
if [ ! -d ".venv" ]; then
    echo "📦 Creating Python virtual environment..."
    python3 -m venv .venv
fi
source .venv/bin/activate

# 2. Install dependencies
echo "⬇️  Installing West and dependencies..."
pip install west
pip install -r scripts/requirements.txt 2>/dev/null || true

# 3. Initialize West
if [ ! -d ".west" ]; then
    echo "🌍 Initializing West workspace..."
    west init -l .
fi
echo "🔄 Updating West modules..."
west update

# 4. Install Zephyr SDK (if needed)
echo "🛠️  Checking Zephyr SDK..."
west sdk install

echo "✅ Setup complete! run 'source .venv/bin/activate' to start working."
