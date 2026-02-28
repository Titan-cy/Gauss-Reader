#pragma once
#include <cstdint>

const uint32_t ID = 0x47415553;

// Header (64 bytes)

struct GaussHeader
{
    uint32_t ID;
    uint16_t version;
    uint16_t compression;

    uint32_t width;
    uint32_t height;

    uint32_t splatCount;
    uint32_t frameRate;

    uint32_t reserved[10];
    
};

// Payload (32 bytes)

struct GaussianSplat
{
    float pos_x;
    float pos_y;
    float scale_x;
    float scale_y;
    float rotation;

    uint16_t red;
    uint16_t green;
    uint16_t blue;
    uint16_t alpha;

    uint32_t padding;

};

