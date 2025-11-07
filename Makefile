# ============================================================
# Luma / Lux Build System (Cross-Platform)
# Works on Linux, macOS, and Windows
# ============================================================

# Include default config
include config.default.mk

# Optional: include user config if present
ifneq ($(wildcard config.mk),)
include config.mk
endif

# ------------------------------------------------------------
# Paths & Files
# ------------------------------------------------------------

BIN       := luma

# Detect OS and set appropriate commands
ifeq ($(OS),Windows_NT)
    # Windows commands
    SHELL := cmd.exe
    MKDIR = if not exist "$(subst /,\,$(1))" mkdir "$(subst /,\,$(1))"
    RMDIR = if exist "$(subst /,\,$(1))" rmdir /s /q "$(subst /,\,$(1))"
    DEL   = if exist "$(1)" del /q "$(1)"
    RM    = del /q
    EXE   = .exe
else
    # Unix commands
    SHELL := /bin/bash
    MKDIR = mkdir -p $(1)
    RMDIR = rm -rf $(1)
    DEL   = rm -f $(1)
    RM    = rm -f
    EXE   =
endif

# ------------------------------------------------------------
# Targets
# ------------------------------------------------------------

.PHONY: all clean debug test check llvm-test view-ir run-llvm compile-native help

# Default target
all: $(BIN)$(EXE)

# Build binary
$(BIN)$(EXE): $(OBJ_FILES)
ifeq ($(OS),Windows_NT)
	@if not exist "$(dir $@)" mkdir "$(subst /,\,$(dir $@))"
else
	@mkdir -p $(dir $@)
endif
	$(CC) -o $@ $^ $(LDFLAGS)

# Compile .c → .o
ifeq ($(OS),Windows_NT)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
else
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
endif

# Debug build
debug: CFLAGS += -g
debug: all

# ------------------------------------------------------------
# Testing & LLVM Targets
# ------------------------------------------------------------

test: $(BIN)$(EXE)
	@echo "Running basic tests..."
ifeq ($(OS),Windows_NT)
	$(BIN)$(EXE) --help || exit 0
else
	./$(BIN) --help || true
endif

check: test

llvm-test: $(BIN)$(EXE)
	@echo "Testing LLVM IR generation..."
	@echo fn add(a: int, b: int) -^> int { return a + b; } > test_simple.lx
	@echo fn main() -^> int { let x: int = 42; let y: int = 24; return add(x, y); } >> test_simple.lx
ifeq ($(OS),Windows_NT)
	$(BIN)$(EXE) test_simple.luma --save
	@dir *.bc *.ll 2>nul || echo No LLVM output files generated
	@del /q test_simple.lx 2>nul
else
	./$(BIN) test_simple.luma --save
	@ls -la *.bc *.ll 2>/dev/null || echo "No LLVM output files generated"
	@rm -f test_simple.lx
endif

view-ir: output.ll
	@echo === Generated LLVM IR ===
ifeq ($(OS),Windows_NT)
	@type output.ll
else
	@cat output.ll
endif

run-llvm: output.bc
	@echo Running with LLVM interpreter...
	lli output.bc

compile-native: output.ll
	@echo Compiling LLVM IR to native executable...
	llc output.ll -o output.s
	gcc output.s -o program
	@echo Native executable created: ./program

# ------------------------------------------------------------
# Cleanup
# ------------------------------------------------------------

clean:
	@echo Cleaning build artifacts...
ifeq ($(OS),Windows_NT)
	@if exist "build" rmdir /s /q "build"
	@if exist "$(BIN).exe" del /q "$(BIN).exe"
	@if exist "$(BIN)" del /q "$(BIN)"
	@echo Cleaning LLVM output files...
	@if exist "output.bc" del /q "output.bc"
	@if exist "output.ll" del /q "output.ll"
	@if exist "output.s" del /q "output.s"
	@if exist "program.exe" del /q "program.exe"
	@if exist "program" del /q "program"
else
	@rm -rf $(OBJ_DIR)
	@rm -f $(BIN)
	@echo "Cleaning LLVM output files..."
	@rm -f output.bc output.ll output.s program
endif

# ------------------------------------------------------------
# Help
# ------------------------------------------------------------

help:
	@echo Available targets:
	@echo   all             - Build the compiler
	@echo   debug           - Build with debug symbols
	@echo   test            - Run basic tests
	@echo   llvm-test       - Test LLVM IR generation
	@echo   view-ir         - View generated LLVM IR
	@echo   run-llvm        - Run generated bitcode with lli
	@echo   compile-native  - Compile LLVM IR to native executable
	@echo   clean           - Remove all build artifacts
	@echo   help            - Show this help