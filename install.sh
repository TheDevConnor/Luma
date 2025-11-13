#!/bin/bash

# Luma Compiler Installation Script v0.1.0
# Installs the Luma compiler and standard library

set -e

VERSION="v0.1.0"
INSTALL_DIR="/usr/local"
BIN_DIR="$INSTALL_DIR/bin"
LIB_DIR="$INSTALL_DIR/lib/luma"
STD_DIR="$LIB_DIR/std"

echo "======================================"
echo "  Luma Compiler $VERSION Installer"
echo "======================================"
echo ""

# Check if running as root for system-wide install
if [ "$EUID" -ne 0 ]; then 
    echo "Note: Not running as root. Will attempt user-local installation."
    echo "For system-wide installation, run with sudo."
    echo ""
    
    INSTALL_DIR="$HOME/.local"
    BIN_DIR="$INSTALL_DIR/bin"
    LIB_DIR="$HOME/.luma"
    STD_DIR="$LIB_DIR/std"
    USER_INSTALL=true
fi

# Create directories
echo "Creating installation directories..."
mkdir -p "$BIN_DIR"
mkdir -p "$STD_DIR"

# Copy binary
echo "Installing Luma compiler to $BIN_DIR..."
if [ -f "./luma" ]; then
    cp ./luma "$BIN_DIR/"
    chmod +x "$BIN_DIR/luma"
elif [ -f "./bin/luma" ]; then
    cp ./bin/luma "$BIN_DIR/"
    chmod +x "$BIN_DIR/luma"
else
    echo "Error: Could not find luma binary!"
    echo "Please ensure 'luma' is in the current directory or bin/ subdirectory."
    exit 1
fi

# Copy standard library
echo "Installing standard library to $STD_DIR..."
if [ -d "./std" ]; then
    cp -r ./std/* "$STD_DIR/"
elif [ -d "./lib/std" ]; then
    cp -r ./lib/std/* "$STD_DIR/"
else
    echo "Warning: Standard library not found. Skipping..."
fi

echo ""
echo "Installation complete!"
echo ""
echo "Installed to:"
echo "  Binary:  $BIN_DIR/luma"
echo "  Std lib: $STD_DIR"
echo ""

# Check if bin directory is in PATH
if [ "$USER_INSTALL" = true ]; then
    if [[ ":$PATH:" != *":$BIN_DIR:"* ]]; then
        echo "IMPORTANT: Add the following to your ~/.bashrc or ~/.zshrc:"
        echo ""
        echo "  export PATH=\"\$PATH:$BIN_DIR\""
        echo ""
    fi
fi

echo "Verify installation with: luma --version"
echo ""
