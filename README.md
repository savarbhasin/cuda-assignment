# CUDA NPP Batch Image Rotation Project

## Project Description

This project implements a high-performance batch image processing application using NVIDIA's NPP (NVIDIA Performance Primitives) library with CUDA. The program processes multiple images in parallel, applying rotation transformations using GPU acceleration.

### Key Features

- **Batch Processing**: Automatically processes all images in a specified directory
- **GPU Acceleration**: Utilizes NVIDIA NPP library for optimized image rotation
- **Multiple Format Support**: Handles TIFF, PGM, PPM, and other common image formats
- **Performance Metrics**: Tracks and reports processing time for each image and overall batch
- **Flexible Configuration**: Command-line arguments for custom input/output directories and rotation angles
- **Detailed Logging**: Generates processing logs with statistics and file lists

## Technical Implementation

### Image Processing Pipeline

1. **Image Loading**: Loads images from disk into CPU memory
2. **Memory Transfer**: Uploads image data to GPU device memory
3. **Bounding Box Calculation**: Computes optimal output dimensions for rotated image
4. **GPU Rotation**: Performs interpolated rotation using NPP primitives
5. **Memory Transfer**: Downloads processed image back to CPU
6. **Image Saving**: Writes rotated image to output directory

### NPP Functions Used

- `nppiGetRotateBound()`: Calculates bounding box for rotated image
- `nppiRotate_8u_C1R()`: Performs 8-bit grayscale image rotation with interpolation

## Building the Project

### Prerequisites

- CUDA Toolkit (10.0 or later)
- C++17 compatible compiler (GCC 7+, MSVC 2017+)
- CMake 3.10 or later
- NVIDIA GPU with Compute Capability 3.0+

### Build Instructions

```bash
# Clone the repository
git clone <your-repo-url>
cd <project-directory>

# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build
make

# Run
./nppiRotate --input-dir ../data/aerials --output-dir ../output --angle 45
```

## Usage

### Basic Usage

```bash
./nppiRotate
```

This will process all TIFF images in `data/aerials/` with a 45-degree rotation.

### Command Line Arguments

- `--input-dir <path>`: Specify input directory (default: `data/aerials`)
- `--output-dir <path>`: Specify output directory (default: `output`)
- `--angle <degrees>`: Rotation angle in degrees (default: 45.0)
- `--extension <ext>`: File extension filter (default: `.tiff`)

### Example Commands

```bash
# Rotate by 90 degrees
./nppiRotate --angle 90

# Process PGM images
./nppiRotate --input-dir ./images --extension .pgm

# Custom output location
./nppiRotate --output-dir ./results --angle 30
```

## Dataset

This project uses aerial TIFF images located in `data/aerials/`. The images are processed in batch mode, demonstrating the ability to handle multiple large images efficiently.

### Supported Formats

- TIFF (.tiff, .tif)
- PGM (.pgm)
- PPM (.ppm)
- BMP (.bmp)
- PNG (.png) - if proper libraries are linked
- JPEG (.jpg) - if proper libraries are linked

## Output

### Processed Images

- Rotated images are saved in the `output/` directory
- Output files are named with `_rotated` suffix
- Original format and bit depth are preserved

### Processing Log

A `processing_log.txt` file is generated containing:
- Configuration parameters
- Processing statistics
- List of processed files
- Timing information

### Example Output

```
==================================================
PROCESSING SUMMARY
==================================================
Total images processed: 24
Successful: 24
Failed: 0
Total time: 3847 ms
Average time per image: 160 ms
Output directory: output
==================================================
```

## Performance Characteristics

- **Throughput**: Processes 100+ images per minute (depends on image size and GPU)
- **GPU Utilization**: Efficient use of NPP optimized kernels
- **Memory Management**: Automatic allocation and deallocation of device memory
- **Scalability**: Linear scaling with number of images

## Project Structure

```
.
├── src/
│   └── main.cpp              # Main application code
├── data/
│   └── aerials/              # Input TIFF images
├── output/                   # Generated output images
│   └── processing_log.txt    # Processing statistics
├── CMakeLists.txt            # Build configuration
└── README.md                 # This file
```

## Error Handling

The program includes comprehensive error handling:
- File I/O validation
- CUDA device capability checks
- NPP function return code verification
- Exception catching for graceful failure

## Future Enhancements

- [ ] Support for color images (RGB/RGBA)
- [ ] Additional transformations (scale, flip, blur)
- [ ] Multi-GPU support for larger datasets
- [ ] Real-time progress bar
- [ ] Parallel batch processing with CUDA streams

## References

- [NVIDIA NPP Documentation](https://docs.nvidia.com/cuda/npp/)
- [CUDA Samples](https://github.com/NVIDIA/cuda-samples)
- [Image Processing with CUDA](https://developer.nvidia.com/gpu-accelerated-libraries)

## License

This project is based on NVIDIA CUDA samples and follows the BSD 3-Clause License. See source code headers for full license text.

## Author

Created for CUDA at Scale for the Enterprise Course Project

## Acknowledgments

- NVIDIA Corporation for NPP library and CUDA samples
- Course instructors and TAs
- Dataset providers
