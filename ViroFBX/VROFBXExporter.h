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
#include <map>
#include "Nodes.pb.h"

/*
 Maximum number of bones that can influence each vertex.
 */
static const int kMaxBoneInfluences = 4;

/*
 Maximum number of bones in a node. Keep in sync with:
 
 ViroRenderer::VROBoneUBO.h and
 ViroRenderer::skinning_vsh.glsl
 */
static const int kMaxBones = 192;

class VROBoneIndexWeight {
public:
    int index;
    float weight;
    
    VROBoneIndexWeight(int i, float w) :
        index(i), weight(w) {}
};

class VROControlPointMetadata {
public:
    /*
     Maps control point indices to each bone mapped to that
     control point.
     */
    std::map<int, std::vector<VROBoneIndexWeight>> bones;
};

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
    
    void exportNode(FbxScene *scene, FbxNode *node, const viro::Node::Skeleton &skeleton, viro::Node *outNode);
    void exportGeometry(FbxNode *node, viro::Node::Geometry *geo);
    void exportMaterial(FbxSurfaceMaterial *inMaterial, viro::Node::Geometry::Material *outMaterial);
    void exportHardwareMaterial(FbxSurfaceMaterial *inMaterial, const FbxImplementation *implementation,
                                viro::Node::Geometry::Material *outMaterial);
    void exportSkeleton(FbxNode *rootNode, viro::Node::Skeleton *outSkeleton);
    void exportSkeletonRecursive(FbxNode *node, int depth, int index, int parentIndex, viro::Node::Skeleton *outSkeleton);
    void exportSkin(FbxNode *node, const viro::Node::Skeleton &skeleton, viro::Node::Geometry::Skin *outSkin);
    void exportAnimations(FbxScene *scene, FbxNode *node, const viro::Node::Skeleton &skeleton, viro::Node *outNode);
    
#pragma mark - Export Helpers
    
    FbxVector4 readNormal(FbxMesh *mesh, int controlPointIndex, int cornerCounter);
    FbxVector4 readTangent(FbxMesh *mesh, int controlPointIndex, int cornerCounter);
    std::vector<int> readMaterialToMeshMapping(FbxMesh *mesh, int numPolygons);
    unsigned int findBoneIndexUsingName(const std::string &name, const viro::Node::Skeleton &skeleton);
    bool isExportableNode(FbxNode *node);
    FbxAMatrix getGeometryMatrix(FbxNode *node);
    
    viro::Node_Geometry_Material_Visual_WrapMode convert(FbxTexture::EWrapMode wrapMode);
    
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
