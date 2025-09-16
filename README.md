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

#### Linux Install

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

# Building LLVM on Windows

## Prerequisites

Install the required tools using Scoop:

```bash
# Install Scoop package manager first if you haven't: https://scoop.sh/
scoop install python ninja cmake mingw
```

## Build Steps

1. Clone the LLVM repository:
```bash
git clone https://github.com/llvm/llvm-project.git
cd llvm-project
```

2. Configure the build:
```bash
cmake -S llvm -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_PROJECTS="clang;lld" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_ASM_COMPILER=gcc
```

3. Build LLVM (adjust `-j8` based on your CPU cores):
```bash
ninja -C build -j8
```

## Notes

- Build time: 30 minutes to several hours depending on hardware
- Disk space required: ~15-20 GB for full build
- RAM usage: Can use 8+ GB during compilation
- If you encounter memory issues, reduce parallelism: `ninja -C build -j4` or `ninja -C build -j1`

## After Build

The compiled binaries will be located in `build/bin/`

### Add to PATH (Optional but Recommended)

To use `clang`, `lld`, and other LLVM tools from anywhere, add the build directory to your PATH:

**Option 1: Temporary (current session only)**
```cmd
set PATH=%PATH%;C:\path\to\your\llvm-project\build\bin
```

**Option 2: Permanent**
1. Open System Properties → Advanced → Environment Variables
2. Edit the `PATH` variable for your user or system
3. Add the full path to your `build\bin` directory (e.g., `C:\Users\yourname\Desktop\llvm-project\build\bin`)

**Option 3: Using PowerShell (permanent)**
```powershell
[Environment]::SetEnvironmentVariable("PATH", $env:PATH + ";C:\path\to\your\llvm-project\build\bin", "User")
```

### Verify Installation
After adding to PATH, open a new command prompt and test:
```bash
clang --version
lld --version
llvm-config --version
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
