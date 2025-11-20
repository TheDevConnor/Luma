# Makefile for Luma Chess Engine
LUMA = ./../../luma
NAME = chess

MAIN = main.lx
CHESS_SRCS = $(filter-out $(MAIN), $(wildcard *.lx))

STD_LIBS = ../../std/string.lx \
           ../../std/terminal.lx \
           ../../std/termfx.lx \
           ../../std/memory.lx

ALL_SRCS = $(MAIN) $(CHESS_SRCS) $(STD_LIBS)

# Detect OS (Windows_NT is predefined on Windows)
ifeq ($(OS),Windows_NT)
    EXE_EXT := .exe
    RM := del /Q
    MKDIR := if not exist build mkdir build
    SEP := &
    RUN_PREFIX :=
else
    EXE_EXT :=
    RM := rm -f
    MKDIR := mkdir -p build
    SEP := ;
    RUN_PREFIX := ./
endif

TARGET = $(NAME)$(EXE_EXT)

.PHONY: all
all: $(TARGET)

# Build chess engine
$(TARGET): $(ALL_SRCS)
	@echo "Building chess engine..."
	$(LUMA) $(MAIN) -name $(NAME) -l $(CHESS_SRCS) $(STD_LIBS)
	@echo "Build complete: $(TARGET)"

.PHONY: run
run: $(TARGET)
	@echo "Starting chess engine..."
	$(RUN_PREFIX)$(TARGET)

.PHONY: valgrind
valgrind: $(TARGET)
ifeq ($(OS),Windows_NT)
	@echo "Valgrind not supported on Windows natively."
else
	@echo "Running chess engine with valgrind..."
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
	@echo "Chess engine sources:"
	@for src in $(CHESS_SRCS); do echo "  $$src"; done
	@echo ""
	@echo "Standard library:"
	@for lib in $(STD_LIBS); do echo "  $$lib"; done

.PHONY: help
help:
	@echo "Luma Chess Engine Build System"
	@echo ""
	@echo "Build Targets:"
	@echo "  all        - Build the chess engine (default)"
	@echo "  rebuild    - Clean and rebuild"
	@echo ""
	@echo "Run Targets:"
	@echo "  run        - Build and run"
	@echo "  valgrind   - Run with memory leak detection (Linux only)"
	@echo ""
	@echo "Development:"
	@echo "  list       - Show all source files"
	@echo "  clean      - Remove build artifacts"
