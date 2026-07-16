#!/usr/bin/env bash
# Set up Python virtual environment and install dependencies
set -euo pipefail

VENV_DIR=".venv"
REQUIREMENTS="python/requirements.txt"

echo "==> Creating virtual environment in $VENV_DIR..."
python3 -m venv "$VENV_DIR"
source "$VENV_DIR/bin/activate"

echo "==> Upgrading pip..."
pip install --upgrade pip

echo "==> Installing Python dependencies..."
pip install -r "$REQUIREMENTS"

echo ""
echo "Done! Activate with: source $VENV_DIR/bin/activate"
echo "Run notebooks with:  jupyter lab research/notebooks/"
