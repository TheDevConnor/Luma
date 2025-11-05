# Makefile for Luma Chess Engine
LUMA = ./../../luma
NAME = chess

MAIN = main.lx
CHESS_SRCS = $(filter-out $(MAIN), $(wildcard $/*.lx))

STD_LIBS = ../../std/string.lx \
           ../../std/terminal.lx \
           ../../std/termfx.lx \
           ../../std/memory.lx

ALL_SRCS = $(MAIN) $(CHESS_SRCS) $(STD_LIBS)

.PHONY: all
all: $(NAME)

# Build chess engine
$(NAME): $(ALL_SRCS)
	@echo "Building chess engine..."
	$(LUMA) $(MAIN) -name $(NAME) -l $(CHESS_SRCS) $(STD_LIBS)
	@echo "Build complete: ./$(NAME)"

.PHONY: run
run: $(NAME)
	@echo "Starting chess engine..."
	./$(NAME)

.PHONY: valgrind
valgrind: $(NAME)
	@echo "Running chess engine with valgrind..."
	valgrind --leak-check=full --show-leak-kinds=all ./$(NAME)

.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(NAME)
	rm -f obj/*.o

.PHONY: rebuild
rebuild: clean all

# Show what files will be compiled
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

# Help target
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
	@echo "  valgrind   - Run with memory leak detection"
	@echo ""
	@echo "Development:"
	@echo "  list       - Show all source files"
	@echo "  clean      - Remove build artifacts"
	@echo ""
	@echo "Examples:"
	@echo "  make              # Build"
	@echo "  make run          # Build and play"
	@echo "  make list         # Show what will be compiled"
