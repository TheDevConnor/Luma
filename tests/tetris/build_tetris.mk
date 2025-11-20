# Makefile for Luma Tetris
# Example:
#   ./luma tests/tetris/tetris.lx -name tetris -l std/termfx.lx std/terminal.lx std/string.lx std/sys.lx std/time.lx std/math.lx

LUMA = ./../../luma
NAME = tetris

MAIN = tetris.lx
TETRIS_SRCS = $(filter-out $(MAIN), $(wildcard *.lx))

STD_LIBS = ../../std/termfx.lx \
           ../../std/terminal.lx \
           ../../std/string.lx \
           ../../std/sys.lx \
           ../../std/time.lx \
           ../../std/math.lx

ALL_SRCS = $(MAIN) $(TETRIS_SRCS) $(STD_LIBS)

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

# Build Tetris
$(TARGET): $(ALL_SRCS)
	@echo "Building Luma Tetris..."
	$(LUMA) $(MAIN) -name $(NAME) -l $(TETRIS_SRCS) $(STD_LIBS)
	@echo "Build complete: $(TARGET)"

.PHONY: run
run: $(TARGET)
	@echo "Starting Tetris..."
	$(RUN_PREFIX)$(TARGET)

.PHONY: valgrind
valgrind: $(TARGET)
ifeq ($(OS),Windows_NT)
	@echo "Valgrind not supported on Windows natively."
else
	@echo "Running Tetris with valgrind..."
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
	@echo "Tetris sources:"
	@for src in $(TETRIS_SRCS); do echo "  $$src"; done
	@echo ""
	@echo "Standard library:"
	@for lib in $(STD_LIBS); do echo "  $$lib"; done

.PHONY: help
help:
	@echo "Luma Tetris Build System"
	@echo ""
	@echo "Build Targets:"
	@echo "  all        - Build Tetris (default)"
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
	@echo "  make run          # Build and play"
	@echo "  make list         # Show what will be compiled"
