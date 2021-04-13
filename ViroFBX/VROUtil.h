//
//  VROUtil.hpp
//  ViroFBX
//
//  Created by Raj Advani on 7/6/17.
//  Copyright © 2017 Viro. All rights reserved.
//

#ifndef VROUtil_hpp
#define VROUtil_hpp

#include <stdio.h>
#include <string>
#include <zlib.h>

/*
 Compress an STL string using zlib with given compression level and return
 the binary data.
 */
std::string compressString(const std::string& str,
                           int compressionlevel = Z_BEST_COMPRESSION);

/*
 Compress the given byte array and return the binary data in a string.
 */
std::string compressBytes(const void *data, size_t dataLength,
                          int compressionlevel = Z_BEST_COMPRESSION);

#endif /* VROUtil_hpp */
