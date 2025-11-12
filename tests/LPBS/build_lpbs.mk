LUMA = ./../../luma
NAME = lpbs

MAIN = ./src/main.lx
SRCS := $(filter-out $(MAIN), $(shell find . -type f -name '*.lx'))
SRCS := $(filter-out ,$(SRCS))   # remove any empty strings

STD_LIBS = ../../std/time.lx \
           ../../std/string.lx \
		   ../../std/sys.lx \
		   ../../std/io.lx \
		   ../../std/memory.lx

ALL_SRCS = $(MAIN) $(SRCS) $(STD_LIBS)

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
	@echo "Building LPBS..."
	$(LUMA) $(MAIN) -name $(NAME) -l $(SRCS) $(STD_LIBS)
ifeq ($(OS),Windows_NT)
	@if exist nul del nul
else
	@rm -f nul
endif
	@echo "Build complete: $(TARGET)"

.PHONY: run
run: $(TARGET)
	@echo "Starting LPBS..."
	$(RUN_PREFIX)$(TARGET)

.PHONY: valgrind
valgrind: $(TARGET)
ifeq ($(OS),Windows_NT)
	@echo "Valgrind not supported on Windows natively."
else
	@echo "Running LPBS with valgrind..."
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
	@echo "LPBS sources:"
	@for src in $(SRCS); do echo "  $$src"; done
	@echo ""
	@echo "Standard library:"
	@for lib in $(STD_LIBS); do echo "  $$lib"; done

.PHONY: help
help:
	@echo "Luma Build System"
	@echo ""
	@echo "Build Targets:"
	@echo "  all        - Build LPBS (default)"
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
