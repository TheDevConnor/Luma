# include config.mk

# # LLVM configuration
# LLVM_CFLAGS := $(shell llvm-config --cflags)
# LLVM_LDFLAGS := $(shell llvm-config --ldflags --system-libs --libs core analysis bitwriter target)

# # Add LLVM flags to existing flags
# override CFLAGS += $(LLVM_CFLAGS)
# override LDFLAGS += $(LLVM_LDFLAGS)

# # Detect platform and define commands
include config.mk

# Detect platform and define commands
ifeq ($(OS),Windows_NT)
	SHELL := cmd.exe
	MKDIR = if not exist "$(subst /,\,$1)" mkdir "$(subst /,\,$1)"
	RMDIR = if exist "$(subst /,\,$1)" rmdir /s /q "$(subst /,\,$1)"
	DEL   = if exist "$(subst /,\,$1)" del /q "$(subst /,\,$1)"
	EXE   = .exe
	PATHSEP = \\
else
	SHELL := /bin/sh
	MKDIR = mkdir -p $1
	RMDIR = rm -rf $1
	DEL   = rm -f $1
	EXE   =
	PATHSEP = /
endif

BIN := luma$(EXE)

.PHONY: all clean debug test llvm-test

all: $(BIN)

$(BIN): $(OBJ_FILES)
	$(call MKDIR,$(@D))
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(call MKDIR,$(dir $@))
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

debug: CFLAGS += -g
debug: all

# Test targets
test: $(BIN)
	@echo "Running basic tests..."
	./$(BIN) --help

llvm-test: $(BIN)
	@echo "Testing LLVM IR generation..."
	@echo "fn add(a: int, b: int) -> int { return a + b; }" > test_simple.lx
	@echo "fn main() -> int { let x: int = 42; let y: int = 24; return add(x, y); }" >> test_simple.lx
	./$(BIN) test_simple.luma --save
	@echo "Generated files:"
	@ls -la *.bc *.ll 2>/dev/null || echo "No LLVM output files generated"
	@echo "Cleaning up test file..."
	@rm -f test_simple.lx

# View generated LLVM IR
view-ir: output.ll
	@echo "=== Generated LLVM IR ==="
	@cat output.ll

# Run the generated LLVM bitcode with lli (LLVM interpreter)
run-llvm: output.bc
	@echo "Running with LLVM interpreter..."
	lli output.bc

# Compile LLVM IR to native executable
compile-native: output.ll
	@echo "Compiling LLVM IR to native executable..."
	llc output.ll -o output.s
	gcc output.s -o program
	@echo "Native executable created: ./program"

clean:
	$(call RMDIR,$(OBJ_DIR))
	$(call RMDIR,"output")
	$(call DEL,$(BIN))
	@echo "Cleaning LLVM output files..."
	$(call DEL,output.bc)
	$(call DEL,output.ll)
	$(call DEL,output.s)
	$(call DEL,program)

# Help target
help:
	@echo "Available targets:"
	@echo "  all          - Build the compiler"
	@echo "  debug        - Build with debug symbols"
	@echo "  test         - Run basic tests"
	@echo "  llvm-test    - Test LLVM IR generation"
	@echo "  view-ir      - View generated LLVM IR"
	@echo "  run-llvm     - Run generated bitcode with lli"
	@echo "  compile-native - Compile LLVM IR to native executable"
	@echo "  clean        - Remove all build artifacts and generated files"
	@echo "  help         - Show this help"
