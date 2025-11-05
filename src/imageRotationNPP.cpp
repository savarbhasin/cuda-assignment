/* rotate.cpp
 *
 * Completed NPP image rotation example.
 *
 * Build (example, adjust include/lib paths for your system):
 *   nvcc -std=c++11 -I/path/to/NPP/include -I/path/to/CUDA/samples/common/inc \
 *        rotate.cpp -L/path/to/NPP/lib -lnppif -lnppig -lnppc -lcudart -o rotate
 *
 * (Or use the CUDA samples Makefile from the CUDA template.)
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
#include <cmath>

#include <cuda_runtime.h>
#include <npp.h>

#include <helper_cuda.h>
#include <helper_string.h>

// Simple NPP error check macro
#define NPP_CHECK_NPP(call)                                                   \
    do                                                                        \
    {                                                                         \
        NppStatus _status = call;                                             \
        if (_status != NPP_SUCCESS)                                           \
        {                                                                     \
            std::cerr << "NPP error at " << __FILE__ << ":" << __LINE__      \
                      << " code=" << _status << " (" #call ")" << std::endl;  \
            exit(EXIT_FAILURE);                                               \
        }                                                                     \
    } while (0)

bool printfNPPinfo(int argc, char *argv[])
{
    const NppLibraryVersion *libVer = nppGetLibVersion();

    printf("NPP Library Version %d.%d.%d\n", libVer->major, libVer->minor,
           libVer->build);

    int driverVersion = 0, runtimeVersion = 0;
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

// compute bounding box for rotating a rectangle of width w and height h by angle degrees
static void computeRotateBoundingBox(int w, int h, double angleDegrees, int &outW, int &outH)
{
    const double a = angleDegrees * (M_PI / 180.0);
    double ca = std::cos(a);
    double sa = std::sin(a);

    // Coordinates of four corners relative to origin (0,0)
    // We'll rotate around the center later. For bbox size only absolute extents matter:
    double hw = w / 2.0;
    double hh = h / 2.0;

    // Four corners centered at origin
    double cornersX[4] = { -hw, hw, hw, -hw };
    double cornersY[4] = { -hh, -hh, hh, hh };

    double minx = 1e30, miny = 1e30, maxx = -1e30, maxy = -1e30;

    for (int i = 0; i < 4; ++i)
    {
        double x = cornersX[i];
        double y = cornersY[i];
        double xr = x * ca - y * sa;
        double yr = x * sa + y * ca;
        if (xr < minx) minx = xr;
        if (xr > maxx) maxx = xr;
        if (yr < miny) miny = yr;
        if (yr > maxy) maxy = yr;
    }

    // width and height of bounding box
    outW = static_cast<int>(std::ceil(maxx - minx));
    outH = static_cast<int>(std::ceil(maxy - miny));

    // Make sure sizes are at least 1
    if (outW < 1) outW = 1;
    if (outH < 1) outH = 1;
}

int main(int argc, char *argv[])
{
    printf("%s Starting...\n\n", (argc > 0 ? argv[0] : "rotate"));

    try
    {
        std::string sFilename;
        char *filePath = nullptr;

        findCudaDevice(argc, (const char **)argv);

        if (printfNPPinfo(argc, argv) == false)
        {
            exit(EXIT_SUCCESS);
        }

        if (checkCmdLineFlag(argc, (const char **)argv, "input"))
        {
            getCmdLineArgumentString(argc, (const char **)argv, "input", &filePath);
        }
        else
        {
            filePath = sdkFindFilePath("Lena.pgm", argv[0]);
        }

        if (filePath)
        {
            sFilename = filePath;
        }
        else
        {
            sFilename = "Lena.pgm";
        }

        // if we specify the filename at the command line, then we only test
        // sFilename[0].
        int file_errors = 0;
        std::ifstream infile(sFilename.data(), std::ifstream::in);

        if (infile.good())
        {
            std::cout << "nppiRotate opened: <" << sFilename.data()
                      << "> successfully!" << std::endl;
            file_errors = 0;
            infile.close();
        }
        else
        {
            std::cout << "nppiRotate unable to open: <" << sFilename.data() << ">"
                      << std::endl;
            file_errors++;
            infile.close();
        }

        if (file_errors > 0)
        {
            exit(EXIT_FAILURE);
        }

        std::string sResultFilename = sFilename;

        std::string::size_type dot = sResultFilename.rfind('.');

        if (dot != std::string::npos)
        {
            sResultFilename = sResultFilename.substr(0, dot);
        }

        sResultFilename += "_rotate.pgm";

        if (checkCmdLineFlag(argc, (const char **)argv, "output"))
        {
            char *outputFilePath = nullptr;
            getCmdLineArgumentString(argc, (const char **)argv, "output",
                                     &outputFilePath);
            if (outputFilePath)
                sResultFilename = outputFilePath;
        }

        // declare a host image object for an 8-bit grayscale image
        npp::ImageCPU_8u_C1 oHostSrc;
        // load gray-scale image from disk
        npp::loadImage(sFilename, oHostSrc);
        // declare a device image and copy construct from the host image,
        // i.e. upload host to device
        npp::ImageNPP_8u_C1 oDeviceSrc(oHostSrc);

        // create struct with the ROI size
        NppiSize oSrcSize = {(int)oDeviceSrc.width(), (int)oDeviceSrc.height()};
        NppiPoint oSrcOffset = {0, 0};
        NppiSize oSizeROI = {(int)oDeviceSrc.width(), (int)oDeviceSrc.height()};

        // rotation parameters
        double angle = 45.0; // Rotation angle in degrees

        // Calculate the bounding box of the rotated image (manual)
        int dstW = 0, dstH = 0;
        computeRotateBoundingBox(oSrcSize.width, oSrcSize.height, angle, dstW, dstH);
        // ensure odd/even or other constraints are satisfied if needed

        // allocate device image for the rotated image
        npp::ImageNPP_8u_C1 oDeviceDst(dstW, dstH);

        // Set the rotation point (center of the source image)
        NppiPoint oRotationCenter = {(int)(oSrcSize.width / 2), (int)(oSrcSize.height / 2)};

        // For destination we need to set a destination ROI offset which determines
        // where source center maps to. To rotate around image center and place
        // the rotated image centered in destination, compute dest center:
        NppiPoint oDstOffset = {0, 0};
        // If you want the rotated image centered inside the destination buffer,
        // compute translation to center it:
        int dstCenterX = dstW / 2;
        int dstCenterY = dstH / 2;
        int srcCenterX = oRotationCenter.x;
        int srcCenterY = oRotationCenter.y;

        // The NPP rotate function uses the rotation center in source coordinates,
        // and writes into the destination ROI. To center the rotated image we
        // set the destination ROI so that the source center maps to the dest center.
        // That means dest ROI origin should be (dstCenter - srcCenter)
        oDstOffset.x = dstCenterX - srcCenterX;
        oDstOffset.y = dstCenterY - srcCenterY;

        // Ensure offset is non-negative and within destination image (NPP will clip)
        if (oDstOffset.x < 0) oDstOffset.x = 0;
        if (oDstOffset.y < 0) oDstOffset.y = 0;

        // build destination ROI rectangle (full destination)
        NppiRect oBoundingBox;
        oBoundingBox.x = 0;
        oBoundingBox.y = 0;
        oBoundingBox.width = dstW;
        oBoundingBox.height = dstH;

        // run the rotation
        // Use nearest-neighbor interpolation (NPPI_INTER_NN) as in original example.
        NPP_CHECK_NPP(nppiRotate_8u_C1R(
            oDeviceSrc.data(), oSrcSize, oDeviceSrc.pitch(), oSrcOffset,
            oDeviceDst.data(), oDeviceDst.pitch(), oBoundingBox, angle, oRotationCenter,
            NPPI_INTER_NN));

        // declare a host image for the result
        npp::ImageCPU_8u_C1 oHostDst(oDeviceDst.size());
        // and copy the device result data into it
        oDeviceDst.copyTo(oHostDst.data(), oHostDst.pitch());

        saveImage(sResultFilename, oHostDst);
        std::cout << "Saved image: " << sResultFilename << std::endl;

        // Let destructors release GPU memory (don't call nppiFree on members)
        exit(EXIT_SUCCESS);
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
        return -1;
    }

    return 0;
}
