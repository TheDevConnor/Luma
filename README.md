# Luma
*A low-level compiled alternative to C, C++, and more!*

<p align="center">
  <img src="assets/luma.png" alt="Luma Logo" width="120">
</p>

<p align="center">
  <a href="#why">Why?</a> •
  <a href="#language-goals">Goals</a> •
  <a href="#project-status">Status</a> •
  <a href="#getting-started">Getting Started</a> •
  <a href="#usage">Usage</a> •
  <a href="#join-us">Join Us</a>
</p>

---

## Introduction

Luma is a modern systems programming language designed to provide the performance and control of low-level languages while maintaining developer productivity and code clarity. Built from the ground up to address common pain points in systems programming.

## Why?

Modern systems programming often involves a trade-off between performance, safety, and developer experience. Luma aims to bridge this gap by providing:

- **Direct hardware access** without sacrificing code readability
- **Predictable performance** characteristics for systems-critical applications  
- **Developer-friendly tooling** that doesn't compromise on compile speed
- **Memory safety options** that can be opted into when needed

## Language Goals

- **🎯 Minimal & Explicit Syntax** – Avoid hidden control flow or magic
- **⚡ Fast Compilation** – Prioritize developer feedback cycles
- **🚀 Zero-Cost Abstractions** – Avoid performance penalties for convenience
- **🔧 Manual Memory Control** – Support fine-grained memory management
- **🛠️ Toolchain Simplicity** – No complex build systems required

## Project Status

**Current Phase:** Early Development

Luma is currently in active development. Core language features are being implemented and the compiler architecture is being established. 
Check out the [todo](todo) to see what is being worked on or that is done.

## Getting Started

### Prerequisites

You'll need the following tools installed:

- **[Make](https://www.gnu.org/software/make/)** - Build automation
- **[GCC](https://gcc.gnu.org/)** - GNU Compiler Collection
- **[LLVM](https://releases.llvm.org/download.html)** - Compiler infrastructure (**Version 20.0+ required**)
- **[Valgrind](https://valgrind.org/)** *(optional)* - Memory debugging

### LLVM Version Requirements

**Important:** Luma requires LLVM 20.0 or higher due to critical bug fixes in the constant generation system.

**Known Issues:**
- **LLVM 19.1.x**: Contains a regression that causes crashes during code generation (`illegal hardware instruction` errors)
- **LLVM 18.x and older**: Not tested, may have compatibility issues

If you encounter crashes during the "LLVM IR" compilation stage (typically at 60% progress), this is likely due to an incompatible LLVM version.

#### Checking Your LLVM Version

```bash
llvm-config --version
```

#### Upgrading LLVM

**Arch Linux:**
```bash
sudo pacman -S llvm
# For development headers:
sudo pacman -S llvm-libs
```

**Fedora/RHEL:**
```bash
sudo dnf update llvm llvm-devel llvm-libs
# Or install specific version:
sudo dnf install llvm20-devel llvm20-libs
```

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install llvm-20-dev
```

**macOS (Homebrew):**
```bash
brew install llvm
```

### Linux Installation

## Usage

## Troubleshooting

### Common Issues

**"illegal hardware instruction" during compilation:**
- This indicates an LLVM version incompatibility
- Upgrade to LLVM 20.0+ to resolve this issue
- See [LLVM Version Requirements](#llvm-version-requirements) above

**Missing LLVM development headers:**
```bash
# Install development packages
sudo dnf install llvm-devel        # Fedora/RHEL
sudo apt install llvm-dev          # Ubuntu/Debian
```

## Join Us

Interested in contributing to Luma? We'd love to have you!
- Check out our [GitHub repository](https://github.com/TheDevConnor/luma)
- Join our [Discord community](https://bit.ly/lux-discord)
- Look at the doxygen-generated docs for architecture details [here](https://thedevconnor.github.io/Luma/)
- If you would like to contribute, please read our [contribution guidelines](CONTRIBUTING.md).

---

<p align="center">
  <strong>Built with ❤️ by the Luma community</strong>
</p>