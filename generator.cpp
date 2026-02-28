#include <iostream>
#include <fstream>
#include <vector>
#include "SplatFormat.h" // Uses your exact struct definitions

int main() {
    // 1. Setup the Header
    GaussHeader header = {};
    header.ID = 0x47415553; // "GAUS" Magic Number
    header.version = 2;     // Pro Format Version
    header.width = 1920;    // 1080p Canvas
    header.height = 1080;
    header.splatCount = 2;  // We are making 2 splats

    // 2. Setup the Payload (The Splats)
    std::vector<GaussianSplat> splats(2);

    // Splat 1: A Solid Red Circle in the center
    splats[0].pos_x = 960.0f;
    splats[0].pos_y = 540.0f;
    splats[0].scale_x = 100.0f;
    splats[0].scale_y = 100.0f;
    splats[0].rotation = 0.0f;
    splats[0].red = 65535;   // 16-bit Max Red
    splats[0].green = 0;
    splats[0].blue = 0;
    splats[0].alpha = 65535; // Fully Opaque

    // Splat 2: A Semi-Transparent Blue Oval off to the side
    splats[1].pos_x = 1200.0f;
    splats[1].pos_y = 800.0f;
    splats[1].scale_x = 200.0f;
    splats[1].scale_y = 50.0f;
    splats[1].rotation = 1.57f; // 90 degrees in radians
    splats[1].red = 0;
    splats[1].green = 0;
    splats[1].blue = 65535;  // 16-bit Max Blue
    splats[1].alpha = 32768; // 50% Opacity

    // 3. Write Data to the Binary File
    std::ofstream outFile("test_image.gauss", std::ios::binary);
    if (!outFile) {
        std::cerr << "Failed to create file!" << std::endl;
        return -1;
    }

    // Write Header (64 bytes)
    outFile.write(reinterpret_cast<const char*>(&header), sizeof(GaussHeader));
    
    // Write Splats (32 bytes * 2 = 64 bytes)
    outFile.write(reinterpret_cast<const char*>(splats.data()), splats.size() * sizeof(GaussianSplat));

    outFile.close();
    
    std::cout << "Successfully created test_image.gauss! File size should be 128 bytes." << std::endl;
    return 0;
}