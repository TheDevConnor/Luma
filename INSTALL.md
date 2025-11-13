# Luma Compiler v0.1.0 - Installation Guide

## Quick Start

### Linux / macOS

```bash
# Extract the tarball
tar -xzf luma-v0.1.0-linux-x86_64.tar.gz
cd luma-v0.1.0

# Install (system-wide, requires sudo)
sudo ./install.sh

# OR install for current user only
./install.sh
```

### Windows

1. Extract `luma-v0.1.0-windows-x86_64.zip`
2. Open Command Prompt as Administrator (for system-wide install)
3. Run `install.bat`

OR simply run `install.bat` without admin for user-only installation.

---

## Standard Library Paths

The Luma compiler searches for `std/` imports in the following order:

### Linux / macOS
1. **System-wide**: `/usr/local/lib/luma/std/`
2. **User-local**: `~/.luma/std/`
3. **Current directory**: `./std/`

### Windows
1. **System-wide**: `C:\Program Files\luma\std\`
2. **User-local**: `%USERPROFILE%\.luma\std\`
3. **Current directory**: `.\std\`

---

## Manual Installation

If you prefer to install manually:

### Linux / macOS

```bash
# Create directories
sudo mkdir -p /usr/local/bin
sudo mkdir -p /usr/local/lib/luma/std

# Copy files
sudo cp luma /usr/local/bin/
sudo cp -r std/* /usr/local/lib/luma/std/

# Make executable
sudo chmod +x /usr/local/bin/luma
```

**For user-local installation:**

```bash
mkdir -p ~/.local/bin
mkdir -p ~/.luma/std

cp luma ~/.local/bin/
cp -r std/* ~/.luma/std/

# Add to PATH (add to ~/.bashrc or ~/.zshrc)
export PATH="$PATH:$HOME/.local/bin"
```

### Windows

1. Create directories:
   - `C:\Program Files\luma\bin` (or `%USERPROFILE%\.luma\bin`)
   - `C:\Program Files\luma\std` (or `%USERPROFILE%\.luma\std`)

2. Copy `luma.exe` to the `bin` directory
3. Copy `std/` contents to the `std` directory
4. Add the `bin` directory to your PATH environment variable

---

## Verifying Installation

After installation, verify with:

```bash
luma --version
```

You should see:
```
Luma Compiler v0.1.0
```

---

## Using the Standard Library

Once installed, you can import standard library modules:

```luma
import std/io
import std/math

fn main() {
    io.println("Hello from Luma!")
}
```

The compiler will automatically find these files in the installed standard library.

---

## Development Setup

If you're developing with Luma and want to use a local `std/` directory:

1. Create a `std/` folder in your project root
2. Place your standard library files there
3. The compiler will use these files first before checking system paths

This is useful for:
- Testing standard library changes
- Using a custom/modified standard library
- Working offline without installed standard library

---

## Troubleshooting

### "Could not find standard library file"

If you see this error, the compiler couldn't find the requested `std/` file.

**Solution:**
1. Verify installation: Check that files exist in the standard library directory
2. Re-run the installer
3. Check file permissions
4. Use absolute paths as a workaround

To see which paths are being searched:
```bash
luma your_file.lx
# If std/ import fails, search paths will be displayed
```

### "luma: command not found" (Linux/macOS)

The binary directory is not in your PATH.

**Solution:**
Add to `~/.bashrc` or `~/.zshrc`:
```bash
export PATH="$PATH:$HOME/.local/bin"
```

Then reload: `source ~/.bashrc`

### PATH issues (Windows)

**Solution:**
1. Search for "Environment Variables" in Windows
2. Edit your user's PATH variable
3. Add: `%USERPROFILE%\.luma\bin` or `C:\Program Files\luma\bin`
4. Restart Command Prompt

---

## Uninstalling

### Linux / macOS

```bash
# System-wide
sudo rm /usr/local/bin/luma
sudo rm -rf /usr/local/lib/luma

# User-local
rm ~/.local/bin/luma
rm -rf ~/.luma
```

### Windows

1. Delete the installation directory
   - `C:\Program Files\luma` or
   - `%USERPROFILE%\.luma`
2. Remove from PATH environment variable

---

## Building from Source

See the main README.md for build instructions if you want to compile Luma yourself.

---

## Support

- GitHub Issues: [your-repo-url]
- Documentation: [docs-url]
- Discord: [discord-link]

---

## License

Luma Compiler is licensed under the MIT License.
