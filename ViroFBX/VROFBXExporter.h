//
//  VROFBXExporter.h
//  ViroFBX
//
//  Created by Raj Advani on 4/25/17.
//  Copyright © 2017 Viro. All rights reserved.
//

#ifndef VROFBXExporter_h
#define VROFBXExporter_h

#include <stdio.h>
#include "fbxsdk.h"
#include <vector>
#include "Nodes.pb.h"

class VROFBXExporter {
  
public:
  
    VROFBXExporter();
    virtual ~VROFBXExporter();
    
    void exportFBX(std::string fbxPath, std::string protoPath);
    void debugPrint(std::string fbxPath);
    
private:
    
    FbxManager *_fbxManager;
    FbxScene *loadFBX(std::string fbxPath);
    
#pragma mark - Export Methods
    
    void exportNode(FbxNode *node, viro::Node *outNode);
    void exportGeometry(FbxNode *node, viro::Node::Geometry *geo);
    void exportMaterial(FbxSurfaceMaterial *inMaterial, viro::Node::Geometry::Material *outMaterial);
    void exportHardwareMaterial(FbxSurfaceMaterial *inMaterial, const FbxImplementation *implementation,
                                viro::Node::Geometry::Material *outMaterial);
    
#pragma mark - Export Helpers
    
    FbxVector4 readNormal(FbxMesh *mesh, int controlPointIndex, int cornerCounter);
    FbxVector4 readTangent(FbxMesh *mesh, int controlPointIndex, int cornerCounter);
    std::vector<int> readMaterialToMeshMapping(FbxMesh *mesh, int numPolygons);
    
#pragma mark - Print Methods
    
    int _numTabs;
    void printNode(FbxNode *pNode);
    void printAttribute(FbxNodeAttribute *pAttribute);
    void printGeometry(FbxGeometry *pGeometry);
    void printTabs();

#pragma mark - Utils
    
    std::string extractTextureName(FbxFileTexture *texture);
    bool endsWith(const std::string& candidate, const std::string& ending);
  
};

#endif /* VROFBXExporter_h */
