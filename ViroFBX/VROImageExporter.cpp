//
//  VROImageExporter.cpp
//  ViroFBX
//
//  Created by Raj Advani on 10/4/17.
//  Copyright © 2017 Viro. All rights reserved.
//

#include "VROImageExporter.h"
#include "VROLog.h"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "gtc/packing.hpp"
#include <fstream>
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <array>
#include "VROUtil.h"

VROImageExporter::VROImageExporter() {
    
}

VROImageExporter::~VROImageExporter() {
    
}

void VROImageExporter::exportEXR(std::string exrPath, std::string outPath, VROImageOutputFormat outFormat) {
    float *data; // width * height * RGBA
    int width, height;
    const char *err;
    
    pinfo("Loading EXR file...");
    int ret = LoadEXR(&data, &width, &height, exrPath.c_str(), &err);
    if (ret != 0) {
        pinfo("Error loading EXR file [%s]", err);
        return;
    }
    pinfo("Load successful [width: %d, height %d]", width, height);
    
    writeVHD(data, width, height, 4, outPath, outFormat);
    free (data);
}

void VROImageExporter::exportHDR(std::string hdrPath, std::string outPath, VROImageOutputFormat outFormat) {
    int width, height, n;
    
    pinfo("Loading HDR file...");
    float *data = stbi_loadf(hdrPath.c_str(), &width, &height, &n, 0);
    if (data == nullptr) {
        pinfo("Error loading HDR file");
        return;
    }
    pinfo("Load successful [width: %d, height %d", width, height);
    
    writeVHD(data, width, height, n, outPath, outFormat);
    free (data);
}

void VROImageExporter::writeVHD(float *data, int width, int height, int componentsPerPixel,
                                std::string outPath, VROImageOutputFormat outFormat) {
    passert (componentsPerPixel == 3 || componentsPerPixel == 4);
    int numPixels = width * height;
    
    uint32_t header[3];
    header[0] = width;
    header[1] = height;
    header[2] = 1; // Mip levels

    pinfo("Packing...");
    void *packed;
    int packedLength;
    
    if (outFormat == VROImageOutputFormat::RGB32F) {
        packed = malloc(numPixels * componentsPerPixel * sizeof(float));
        packedLength = numPixels * componentsPerPixel * sizeof(float);
        memcpy(packed, data, packedLength);
    }
    else if (outFormat == VROImageOutputFormat::RGB16F) {
        uint64 *packed16F = (uint64 *) malloc(numPixels * sizeof(uint64));
        for (int i = 0; i < numPixels; i++) {
            float r = data[i * componentsPerPixel + 0];
            float g = data[i * componentsPerPixel + 1];
            float b = data[i * componentsPerPixel + 2];
            
            float a = 1.0;
            if (componentsPerPixel == 4) {
                a = data[i * 4 + 3];
            }
            
            const glm::vec4 v(r, g, b, a);
            packed16F[i] = glm::packHalf4x16(v);
        }
        
        packed = (void *) packed16F;
        packedLength = numPixels * sizeof(uint64);
    }
    else if (outFormat == VROImageOutputFormat::RGB9E5) {
        uint32 *packedF9E5 = (uint32 *) malloc(numPixels * sizeof(uint32));
        for (int i = 0; i < numPixels; i++) {
            float r = data[i * componentsPerPixel + 0];
            float g = data[i * componentsPerPixel + 1];
            float b = data[i * componentsPerPixel + 2];
            // alpha is disregarded
            
            const glm::vec3 v(r, g, b);
            packedF9E5[i] = glm::packF3x9_E1x5(v);
        }
        
        packed = (void *) packedF9E5;
        packedLength = numPixels * sizeof(uint32);
    }
    else {
        pabort();
    }
    
    void *out = malloc(sizeof(header) + packedLength);
    memcpy(out, header, sizeof(header));
    memcpy((char *)out + sizeof(header), packed, packedLength);
    
    pinfo("Compressing...");
    std::string compressed = compressBytes(out, sizeof(header) + packedLength);
    pinfo("Writing image [%d bytes]", (int)compressed.size());
    std::ofstream(outPath.c_str(), std::ios::binary).write(compressed.c_str(), compressed.size());
    pinfo("Export complete");
    free (packed);
}

