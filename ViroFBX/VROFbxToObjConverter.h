//
//  VROFbxToObjConverter.hpp
//  ViroFBX
//
//  Created by Raj Advani on 4/8/19.
//  Copyright © 2019 Viro. All rights reserved.
//

#ifndef VROFbxToObjConverter_h
#define VROFbxToObjConverter_h

#include <stdio.h>
#include "fbxsdk.h"
#include <vector>
#include <map>
#include <string>

class VROFbxToObjConverter {
public:

    VROFbxToObjConverter();
    virtual ~VROFbxToObjConverter();

    /*
     Convert the _first exportable_ node  in the given FBX to OBJ.
     This is the first node found that contains a mesh.
     */
    void convertToObj(std::string fbxPath, std::string outPath);

private:

    FbxManager *_fbxManager;
    FbxScene *loadFBX(std::string fbxPath);

    /*
     The path of the file we're currently exporting.
     */
    std::string _fbxPath;

    bool isExportableNode(FbxNode *node);
    void convertToObj(FbxNode *node, std::string outPath);

};

#endif /* VROFbxToObjConverter_h */
