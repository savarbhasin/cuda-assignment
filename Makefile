# Makefile for CUDA NPP Batch TIFF Image Rotation
# Compatible with Linux and can be adapted for Windows

# CUDA Toolkit location
CUDA_PATH ?= /usr/local/cuda

# Architecture - adjust based on your GPU
# Common values: 50,52,60,61,70,75,80,86
SMS ?= 50 52 60 61 70 75 80 86

# Compiler settings
HOST_COMPILER ?= g++
NVCC := $(CUDA_PATH)/bin/nvcc -ccbin $(HOST_COMPILER)

# Target executable name
TARGET := batchRotateTIFF

# Directories
SRCDIR := src
OBJDIR := obj
BINDIR := bin
INCDIR := include

# NPP and CUDA samples common directory (adjust path as needed)
CUDA_SAMPLES_PATH ?= $(CUDA_PATH)/samples
COMMON_INC := $(CUDA_SAMPLES_PATH)/common/inc

# Include paths
INCLUDES := -I$(CUDA_PATH)/include
INCLUDES += -I$(INCDIR)
INCLUDES += -I$(COMMON_INC)
INCLUDES += -I$(CUDA_SAMPLES_PATH)/7_CUDALibraries/common/UtilNPP
INCLUDES += -I$(CUDA_SAMPLES_PATH)/7_CUDALibraries/common/FreeImage/include

# Library paths
LIBRARIES := -L$(CUDA_PATH)/lib64
LIBRARIES += -L$(CUDA_SAMPLES_PATH)/7_CUDALibraries/common/FreeImage/lib/linux/x86_64

# Libraries to link
LIBS := -lcudart -lnppc -lnppi -lnppig -lnppif -lnppist -lfreeimage

# Compiler flags
NVCCFLAGS := -std=c++11
CXXFLAGS := -std=c++11 -O3

# Generate SASS code for each architecture
$(foreach sm,$(SMS),$(eval GENCODE_FLAGS += -gencode arch=compute_$(sm),code=sm_$(sm)))

# Generate PTX code for the highest architecture
HIGHEST_SM := $(lastword $(sort $(SMS)))
GENCODE_FLAGS += -gencode arch=compute_$(HIGHEST_SM),code=compute_$(HIGHEST_SM)

# Source files
SOURCES := $(SRCDIR)/batchRotate.cpp

# Object files
OBJECTS := $(OBJDIR)/batchRotate.o

# Default target
all: directories $(BINDIR)/$(TARGET)

# Create necessary directories
directories:
	@mkdir -p $(OBJDIR)
	@mkdir -p $(BINDIR)
	@mkdir -p output
	@mkdir -p logs

# Link executable
$(BINDIR)/$(TARGET): $(OBJECTS)
	$(NVCC) $(GENCODE_FLAGS) -o $@ $^ $(LIBRARIES) $(LIBS)
	@echo "Build complete: $@"

# Compile source files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(NVCC) $(NVCCFLAGS) $(GENCODE_FLAGS) $(INCLUDES) -o $@ -c $<

# Clean build files
clean:
	rm -rf $(OBJDIR) $(BINDIR)
	@echo "Clean complete"

# Clean everything including output
cleanall: clean
	rm -rf output/* logs/*
	@echo "All generated files removed"

# Run with default parameters
run: all
	./$(BINDIR)/$(TARGET) -input data -output output -angle 45

# Run with custom parameters
# Usage: make run-custom INPUT=path/to/images OUTPUT=path/to/output ANGLE=30
run-custom: all
	./$(BINDIR)/$(TARGET) -input $(INPUT) -output $(OUTPUT) -angle $(ANGLE)

# Display help
help:
	@echo "Available targets:"
	@echo "  all (default) - Build the project"
	@echo "  clean         - Remove object and binary files"
	@echo "  cleanall      - Remove all generated files including output"
	@echo "  run           - Build and run with default parameters"
	@echo "  run-custom    - Build and run with custom parameters"
	@echo "                  Usage: make run-custom INPUT=path OUTPUT=path ANGLE=45"
	@echo "  help          - Display this help message"
	@echo ""
	@echo "Environment variables:"
	@echo "  CUDA_PATH          - Path to CUDA toolkit (default: /usr/local/cuda)"
	@echo "  CUDA_SAMPLES_PATH  - Path to CUDA samples (default: CUDA_PATH/samples)"
	@echo "  SMS                - Target GPU architectures (default: 50 52 60 61 70 75 80 86)"
	@echo "  HOST_COMPILER      - Host C++ compiler (default: g++)"

# Check CUDA installation
check:
	@echo "Checking CUDA installation..."
	@echo "CUDA_PATH: $(CUDA_PATH)"
	@echo "NVCC: $(NVCC)"
	@which nvcc || echo "NVCC not found in PATH"
	@echo "Target architectures: $(SMS)"
	@echo "Include paths: $(INCLUDES)"
	@echo "Library paths: $(LIBRARIES)"

.PHONY: all clean cleanall run run-custom help check directories
