# Makefile for Luma 3D Spinning Cube
# Example:
#   ./luma tests/3d_spinning_cube.lx -name 3d -l std/math.lx std/termfx.lx std/string.lx std/memory.lx

LUMA = ./../../luma
NAME = spinning_cube

MAIN = 3d_spinning_cube.lx
SPINNING_SRCS = $(filter-out $(MAIN), $(wildcard *.lx))

STD_LIBS = ../../std/termfx.lx \
           ../../std/string.lx \
           ../../std/math.lx \
           ../../std/memory.lx \
		   ../../std/io.lx \
		   ../../std/time.lx \
		   ../../std/sys.lx

ALL_SRCS = $(MAIN) $(SPINNING_SRCS) $(STD_LIBS)

# Detect OS (Windows_NT is predefined on Windows)
ifeq ($(OS),Windows_NT)
    EXE_EXT := .exe
    RM := del /Q
    MKDIR := if not exist build mkdir build
    RUN_PREFIX :=
else
    EXE_EXT :=
    RM := rm -f
    MKDIR := mkdir -p build
    RUN_PREFIX := ./
endif

TARGET = $(NAME)$(EXE_EXT)

.PHONY: all
all: $(TARGET)

# Build spinning cube demo
$(TARGET): $(ALL_SRCS)
	@echo "Building 3D spinning cube..."
	$(LUMA) $(MAIN) -name $(NAME) -l $(SPINNING_SRCS) $(STD_LIBS)
	@echo "Build complete: $(TARGET)"

.PHONY: run
run: $(TARGET)
	@echo "Starting spinning cube..."
	$(RUN_PREFIX)$(TARGET)

.PHONY: valgrind
valgrind: $(TARGET)
ifeq ($(OS),Windows_NT)
	@echo "Valgrind not supported on Windows natively."
else
	@echo "Running spinning cube with valgrind..."
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET)
endif

.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	-$(RM) $(TARGET)
	-$(RM) obj\*.o 2>nul || true
	-$(RM) obj/*.o 2>/dev/null || true

.PHONY: rebuild
rebuild: clean all

.PHONY: list
list:
	@echo "Main file:"
	@echo "  $(MAIN)"
	@echo ""
	@echo "Spinning cube sources:"
	@for src in $(SPINNING_SRCS); do echo "  $$src"; done
	@echo ""
	@echo "Standard library:"
	@for lib in $(STD_LIBS); do echo "  $$lib"; done

.PHONY: help
help:
	@echo "Luma 3D Spinning Cube Build System"
	@echo ""
	@echo "Build Targets:"
	@echo "  all        - Build Spinning Cube (default)"
	@echo "  rebuild    - Clean and rebuild"
	@echo ""
	@echo "Run Targets:"
	@echo "  run        - Build and run"
	@echo "  valgrind   - Run with memory leak detection (Linux only)"
	@echo ""
	@echo "Development:"
	@echo "  list       - Show all source files"
	@echo "  clean      - Remove build artifacts"
	@echo ""
	@echo "Examples:"
	@echo "  make              # Build"
	@echo "  make run          # Build and run"
	@echo "  make list         # Show what will be compiled"
