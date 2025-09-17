# config.mk

CC       = g++
CFLAGS   = -Wall -Wextra -std=c17 -Wno-unused-variable -O2 -x c  # -x c tells g++ to treat files as C code
LDFLAGS  = 
INCLUDES = -Isrc

# LLVM configuration - request all necessary components
LLVM_CFLAGS := $(shell llvm-config --cflags)
LLVM_LDFLAGS := $(shell llvm-config --ldflags)
LLVM_LIBS := $(shell llvm-config --libs --system-libs all)

# Alternative: specify exactly what you need
# LLVM_LIBS := $(shell llvm-config --libs --system-libs core analysis bitwriter target irreader asmparser mcparser mc bitreader support demangle)

# Add LLVM flags to existing flags  
override CFLAGS += $(LLVM_CFLAGS)
override LDFLAGS += $(LLVM_LDFLAGS) $(LLVM_LIBS)

SRC_DIR  = src
OBJ_DIR  = build

define find_c_sources
$(wildcard $(1)/*.c) \
$(foreach d,$(wildcard $(1)/*),$(call find_c_sources,$(d)))
endef

SRC_FILES := $(call find_c_sources,$(SRC_DIR))
OBJ_FILES := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES))
