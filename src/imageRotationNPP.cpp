/* Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#define WINDOWS_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#pragma warning(disable : 4819)
#endif

#include <Exceptions.h>
#include <ImageIO.h>
#include <ImagesCPU.h>
#include <ImagesNPP.h>

#include <string.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>
#include <chrono>

#include <cuda_runtime.h>
#include <npp.h>

#include <helper_cuda.h>
#include <helper_string.h>

namespace fs = std::filesystem;

bool printfNPPinfo(int argc, char *argv[])
{
    const NppLibraryVersion *libVer = nppGetLibVersion();

    printf("NPP Library Version %d.%d.%d\n", libVer->major, libVer->minor,
           libVer->build);

    int driverVersion, runtimeVersion;
    cudaDriverGetVersion(&driverVersion);
    cudaRuntimeGetVersion(&runtimeVersion);

    printf("  CUDA Driver  Version: %d.%d\n", driverVersion / 1000,
           (driverVersion % 100) / 10);
    printf("  CUDA Runtime Version: %d.%d\n", runtimeVersion / 1000,
           (runtimeVersion % 100) / 10);

    // Min spec is SM 1.0 devices
    bool bVal = checkCudaCapabilities(1, 0);
    return bVal;
}

std::vector<std::string> getImageFiles(const std::string& directory, const std::string& extension)
{
    std::vector<std::string> imageFiles;
    
    try {
        if (fs::exists(directory) && fs::is_directory(directory)) {
            for (const auto& entry : fs::recursive_directory_iterator(directory)) {
                if (entry.is_regular_file()) {
                    std::string filepath = entry.path().string();
                    std::string ext = entry.path().extension().string();
                    
                    // Convert extension to lowercase for comparison
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    
                    if (ext == extension) {
                        imageFiles.push_back(filepath);
                    }
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }
    
    return imageFiles;
}

bool processImage(const std::string& inputPath, const std::string& outputPath, double angle)
{
    try {
        std::cout << "Processing: " << inputPath << std::endl;
        
        // Load image (NPP supports PGM, PPM, and with proper libraries, TIFF)
        npp::ImageCPU_8u_C1 oHostSrc;
        npp::loadImage(inputPath, oHostSrc);
        
        // Upload to device
        npp::ImageNPP_8u_C1 oDeviceSrc(oHostSrc);

        // Create ROI structures
        NppiSize oSrcSize = {(int)oDeviceSrc.width(), (int)oDeviceSrc.height()};
        NppiPoint oSrcOffset = {0, 0};

        // Calculate bounding box for rotated image
        NppiRect oBoundingBox;
        NPP_CHECK_NPP(nppiGetRotateBound(oSrcSize, angle, &oBoundingBox));

        // Allocate device memory for output
        npp::ImageNPP_8u_C1 oDeviceDst(oBoundingBox.width, oBoundingBox.height);

        // Set rotation center (center of image)
        NppiPoint oRotationCenter = {(int)(oSrcSize.width / 2), (int)(oSrcSize.height / 2)};

        // Perform rotation
        NPP_CHECK_NPP(nppiRotate_8u_C1R(
            oDeviceSrc.data(), oSrcSize, oDeviceSrc.pitch(), oSrcOffset,
            oDeviceDst.data(), oDeviceDst.pitch(), oBoundingBox, angle, 
            oRotationCenter, NPPI_INTER_LINEAR));

        // Copy result back to host
        npp::ImageCPU_8u_C1 oHostDst(oDeviceDst.size());
        oDeviceDst.copyTo(oHostDst.data(), oHostDst.pitch());

        // Save output image
        saveImage(outputPath, oHostDst);
        std::cout << "  Saved: " << outputPath << std::endl;

        // Cleanup
        nppiFree(oDeviceSrc.data());
        nppiFree(oDeviceDst.data());

        return true;
    }
    catch (npp::Exception &rException) {
        std::cerr << "  NPP Exception: " << rException << std::endl;
        return false;
    }
    catch (...) {
        std::cerr << "  Unknown exception occurred" << std::endl;
        return false;
    }
}

int main(int argc, char *argv[])
{
    printf("%s Starting...\n\n", argv[0]);

    try
    {
        findCudaDevice(argc, (const char **)argv);

        if (printfNPPinfo(argc, argv) == false)
        {
            exit(EXIT_SUCCESS);
        }

        // Configuration
        std::string inputDir = "data/aerials";
        std::string outputDir = "output";
        std::string extension = ".tiff";
        double angle = 45.0;

        // Parse command line arguments
        if (checkCmdLineFlag(argc, (const char **)argv, "input-dir"))
        {
            char *inputDirPath;
            getCmdLineArgumentString(argc, (const char **)argv, "input-dir", &inputDirPath);
            inputDir = inputDirPath;
        }

        if (checkCmdLineFlag(argc, (const char **)argv, "output-dir"))
        {
            char *outputDirPath;
            getCmdLineArgumentString(argc, (const char **)argv, "output-dir", &outputDirPath);
            outputDir = outputDirPath;
        }

        if (checkCmdLineFlag(argc, (const char **)argv, "angle"))
        {
            angle = getCmdLineArgumentFloat(argc, (const char **)argv, "angle");
        }

        if (checkCmdLineFlag(argc, (const char **)argv, "extension"))
        {
            char *ext;
            getCmdLineArgumentString(argc, (const char **)argv, "extension", &ext);
            extension = ext;
            if (extension[0] != '.') {
                extension = "." + extension;
            }
        }

        // Create output directory if it doesn't exist
        fs::create_directories(outputDir);

        // Get all image files
        std::cout << "Scanning directory: " << inputDir << std::endl;
        std::cout << "Looking for files with extension: " << extension << std::endl;
        std::vector<std::string> imageFiles = getImageFiles(inputDir, extension);

        if (imageFiles.empty()) {
            std::cout << "No images found with extension " << extension << " in " << inputDir << std::endl;
            std::cout << "\nTrying alternative extensions..." << std::endl;
            
            // Try common image extensions
            std::vector<std::string> extensions = {".pgm", ".ppm", ".jpg", ".png", ".bmp"};
            for (const auto& ext : extensions) {
                imageFiles = getImageFiles(inputDir, ext);
                if (!imageFiles.empty()) {
                    extension = ext;
                    std::cout << "Found " << imageFiles.size() << " images with " << ext << " extension" << std::endl;
                    break;
                }
            }
            
            if (imageFiles.empty()) {
                std::cerr << "No supported image files found!" << std::endl;
                exit(EXIT_FAILURE);
            }
        }

        std::cout << "\nFound " << imageFiles.size() << " image(s) to process\n" << std::endl;
        std::cout << "Rotation angle: " << angle << " degrees\n" << std::endl;

        // Process statistics
        int successCount = 0;
        int failCount = 0;
        auto startTime = std::chrono::high_resolution_clock::now();

        // Process each image
        for (size_t i = 0; i < imageFiles.size(); ++i) {
            std::cout << "\n[" << (i+1) << "/" << imageFiles.size() << "] ";
            
            std::string inputPath = imageFiles[i];
            fs::path inPath(inputPath);
            
            // Create output filename
            std::string outputFilename = inPath.stem().string() + "_rotated" + inPath.extension().string();
            std::string outputPath = outputDir + "/" + outputFilename;

            auto imgStartTime = std::chrono::high_resolution_clock::now();
            bool success = processImage(inputPath, outputPath, angle);
            auto imgEndTime = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(imgEndTime - imgStartTime);
            std::cout << "  Time: " << duration.count() << " ms" << std::endl;

            if (success) {
                successCount++;
            } else {
                failCount++;
            }
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        // Print summary
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "PROCESSING SUMMARY" << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        std::cout << "Total images processed: " << imageFiles.size() << std::endl;
        std::cout << "Successful: " << successCount << std::endl;
        std::cout << "Failed: " << failCount << std::endl;
        std::cout << "Total time: " << totalDuration.count() << " ms" << std::endl;
        std::cout << "Average time per image: " << (imageFiles.size() > 0 ? totalDuration.count() / imageFiles.size() : 0) << " ms" << std::endl;
        std::cout << "Output directory: " << outputDir << std::endl;
        std::cout << std::string(50, '=') << std::endl;

        // Write log file
        std::string logPath = outputDir + "/processing_log.txt";
        std::ofstream logFile(logPath);
        if (logFile.is_open()) {
            logFile << "NPP Image Rotation Processing Log\n";
            logFile << "==================================\n\n";
            logFile << "Date: " << __DATE__ << " " << __TIME__ << "\n";
            logFile << "Input directory: " << inputDir << "\n";
            logFile << "Output directory: " << outputDir << "\n";
            logFile << "Rotation angle: " << angle << " degrees\n";
            logFile << "Extension filter: " << extension << "\n\n";
            logFile << "Results:\n";
            logFile << "  Total images: " << imageFiles.size() << "\n";
            logFile << "  Successful: " << successCount << "\n";
            logFile << "  Failed: " << failCount << "\n";
            logFile << "  Total time: " << totalDuration.count() << " ms\n";
            logFile << "  Average time: " << (imageFiles.size() > 0 ? totalDuration.count() / imageFiles.size() : 0) << " ms\n\n";
            logFile << "Processed files:\n";
            for (const auto& file : imageFiles) {
                logFile << "  - " << file << "\n";
            }
            logFile.close();
            std::cout << "Log file saved: " << logPath << std::endl;
        }

        exit(failCount == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    }
    catch (npp::Exception &rException)
    {
        std::cerr << "Program error! The following exception occurred: \n";
        std::cerr << rException << std::endl;
        std::cerr << "Aborting." << std::endl;

        exit(EXIT_FAILURE);
    }
    catch (...)
    {
        std::cerr << "Program error! An unknown type of exception occurred. \n";
        std::cerr << "Aborting." << std::endl;

        exit(EXIT_FAILURE);
    }

    return 0;
}
