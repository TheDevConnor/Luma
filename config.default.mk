# config.default.mk

CC       ?= gcc
CFLAGS   ?= -Wall -Wextra -std=c17 -Wno-unused-variable -O2
LDFLAGS  ?= 
INCLUDES ?= -Isrc

# LLVM configuration - request all necessary components
LLVM_CFLAGS := $(shell llvm-config --cflags)
LLVM_LDFLAGS := $(shell llvm-config --ldflags)
LLVM_LIBS := $(shell llvm-config --libs --system-libs all)

# Add LLVM flags to existing flags  
override CFLAGS += $(LLVM_CFLAGS)
override LDFLAGS += $(LLVM_LDFLAGS) $(LLVM_LIBS) -lstdc++

SRC_DIR  = src
OBJ_DIR  = build

# Recursive function to find all .c files
define find_c_sources
$(wildcard $(1)/*.c) \
$(foreach d,$(wildcard $(1)/*),$(call find_c_sources,$(d)))
endef

# Find all source files recursively
SRC_FILES := $(call find_c_sources,$(SRC_DIR))

# Generate object file paths
OBJ_FILES := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES))