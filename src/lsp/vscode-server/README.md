# Luma Language Server Protocol (LSP) Extension

VS Code extension providing language support for the Luma programming language through a Language Server Protocol implementation.

## Features

- Syntax highlighting for `.lx` files
- Language server integration for intelligent code features
- Command to restart the LSP server

## Prerequisites

- Visual Studio Code 1.75.0 or higher
- Node.js (version 18 or higher recommended)
- Luma compiler/LSP server built and available at `/home/TheDevConnor/Luma/luma`

## Development Setup

### 1. Clone and Navigate to the Extension Directory

```bash
cd /home/TheDevConnor/Luma/src/lsp/vscode-server
```

### 2. Install Dependencies

```bash
npm install
```

This will automatically:
- Install root dependencies
- Run `postinstall` to install client dependencies

### 3. Build the Extension

```bash
npm run compile
```

This compiles the TypeScript code to JavaScript in the `client/out` directory.

## Running the Extension

### Method 1: Debug Mode (Development)

1. **Open the project in VS Code:**
   ```bash
   code .
   ```

2. **Press F5** or select **Run > Start Debugging**

   This will:
   - Compile the extension
   - Open a new VS Code window (Extension Development Host)
   - Load your extension in that window

3. **Test the extension:**
   - Create a new file with `.lx` extension
   - The LSP should automatically start
   - Check **View > Output > Luma LSP** for logs

### Method 2: Install Locally

1. **Install the VS Code Extension packager:**
   ```bash
   npm install -g @vscode/vsce
   ```

2. **Package the extension:**
   ```bash
   vsce package
   ```
   
   This creates a `.vsix` file (e.g., `luma-lsp-1.0.0.vsix`)

3. **Install the extension:**
   ```bash
   code --install-extension luma-lsp-1.0.0.vsix
   ```

4. **Reload VS Code** and open any `.lx` file

## Available Commands

Access commands via Command Palette (`Ctrl+Shift+P` or `Cmd+Shift+P`):

- **Restart Luma LSP** - Restarts the language server without reloading VS Code

## Project Structure

```
vscode-server/
├── package.json              # Extension manifest
├── tsconfig.json            # Root TypeScript config
├── client/                  # Extension client code
│   ├── package.json         # Client dependencies
│   ├── tsconfig.json        # Client TypeScript config
│   ├── src/
│   │   └── extension.ts     # Main extension code
│   └── out/                 # Compiled JavaScript (generated)
└── language-support/        # Language definitions
    └── syntaxes/
        └── lux.tmLanguage.json
```

## Development Workflow

### Watching for Changes

```bash
npm run watch
```

This will automatically recompile when you make changes to TypeScript files.

### Linting

```bash
npm run lint
```

## Configuration

The extension expects the Luma LSP server at:
```
/home/TheDevConnor/Luma/luma -lsp
```

To change this location, edit `client/src/extension.ts`:

```typescript
const serverOptions: ServerOptions = {
  command: "/path/to/your/luma",
  transport: TransportKind.stdio,
  args: ["-lsp"]
};
```

## Troubleshooting

### Extension Not Activating

- Check that files have `.luma` extension
- Verify activation events in `package.json`
- Check **Help > Toggle Developer Tools > Console** for errors

### LSP Server Not Starting

- Verify the Luma executable exists: `ls -la /home/TheDevConnor/Luma/luma`
- Test the LSP manually: `/home/TheDevConnor/Luma/luma -lsp`
- Check Output panel: **View > Output > Luma LSP**

### Compilation Errors

```bash
# Clean build artifacts
rm -rf client/out out

# Reinstall dependencies
rm -rf node_modules client/node_modules
npm install

# Rebuild
npm run compile
```

### Server Crashes or Hangs

Use the **Restart Luma LSP** command from the Command Palette.

## Contributing

1. Make changes to the TypeScript source files
2. Run `npm run compile` to build
3. Press F5 to test in the Extension Development Host
4. Submit a pull request

## License

MIT

## Authors

- SovietPancakes
- TheDevConnor