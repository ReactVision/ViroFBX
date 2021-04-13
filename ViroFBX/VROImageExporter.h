//
//  VROImageExporter.hpp
//  ViroFBX
//
//  Created by Raj Advani on 10/4/17.
//  Copyright © 2017 Viro. All rights reserved.
//

#ifndef VROImageExporter_hpp
#define VROImageExporter_hpp

#include <stdio.h>
#include <string>

enum class VROImageOutputFormat {
    RGB9E5,
    RGB16F,
    RGB32F,
};

/*
 Converts EXR and HDR images to RGB9_E5, RGB16F, and RGB32F formats so that they
 can be read by OpenGL ES.
 */
class VROImageExporter {
public:

    VROImageExporter();
    virtual ~VROImageExporter();

    void exportEXR(std::string exrPath, std::string outPath, VROImageOutputFormat outFormat);
    void exportHDR(std::string hdrPath, std::string outPath, VROImageOutputFormat outFormat);

private:

    void writeVHD(float *data, int width, int height, int componentsPerPixel,
                  std::string outPath, VROImageOutputFormat outFormat);

};

#endif /* VROImageExporter_hpp */
