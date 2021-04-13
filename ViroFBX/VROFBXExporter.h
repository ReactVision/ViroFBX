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

    void exportFBX(std::string fbxPath, std::string protoPath, bool compressTextures);
    void debugPrint(std::string fbxPath);

private:

    FbxManager *_fbxManager;
    FbxScene *loadFBX(std::string fbxPath);

    /*
     The path of the file we're currently exporting.
     */
    std::string _fbxPath;

#pragma mark - Export Methods

    void exportNode(FbxScene *scene, FbxNode *node, int depth, bool compressTextures,
                    const std::vector<FbxNode *> &boneNodes, viro::Node::Skeleton *outSkeleton, viro::Node *outNode);
    void exportGeometry(FbxNode *node, int depth, bool compressTextures, viro::Node::Geometry *geo);
    void exportMaterial(FbxSurfaceMaterial *inMaterial, bool compressTextures, viro::Node::Geometry::Material *outMaterial);
    void exportHardwareMaterial(FbxSurfaceMaterial *inMaterial, const FbxImplementation *implementation,
                                viro::Node::Geometry::Material *outMaterial);
    void exportSkeleton(FbxNode *rootNode, std::vector<FbxNode *> *outBoneNodes, viro::Node::Skeleton *outSkeleton);
    void exportSkeletonRecursive(FbxNode *node, int depth, int index, int parentIndex, std::vector<FbxNode *> *outBoneNodes, viro::Node::Skeleton *outSkeleton);
    void exportSkin(FbxNode *node, const std::vector<FbxNode *> &boneNodes, viro::Node::Skeleton *outSkeleton, viro::Node::Geometry::Skin *outSkin);

    void exportKeyframeAnimations(FbxScene *scene, FbxNode *node, viro::Node *outNode);
    void exportSampledKeyframeAnimations(FbxScene *scene, FbxNode *node, viro::Node *outNode);
    void exportSkeletalAnimations(FbxScene *scene, FbxNode *node, const std::vector<FbxNode *> &boneNodes,
                                  viro::Node *outNode);
    void exportBlendShapeAnimations(FbxScene *scene, FbxNode *node, viro::Node *outNode);

#pragma mark - Export Helpers

    FbxVector4 readNormal(FbxMesh *mesh, int controlPointIndex, int cornerCounter);
    FbxVector4 readTangent(FbxMesh *mesh, int controlPointIndex, int cornerCounter);
    std::vector<int> readMaterialToMeshMapping(FbxMesh *mesh, int numPolygons);
    unsigned int findBoneIndex(FbxNode *node, const std::vector<FbxNode *> &boneNodes);
    bool isExportableNode(FbxNode *node);
    FbxAMatrix getGeometryMatrix(FbxNode *node);

    viro::Node_Geometry_Material_Visual_WrapMode convert(FbxTexture::EWrapMode wrapMode);

#pragma mark - Texture Compression

    bool compressTexture(std::string textureName);

#pragma mark - Print Methods

    int _numTabs;
    void printNode(FbxNode *pNode);
    void printAttribute(FbxNodeAttribute *pAttribute);
    void printGeometry(FbxGeometry *pGeometry);
    void printTabs();

#pragma mark - Utils

    std::string extractTextureName(FbxFileTexture *texture);
    std::string getFileName(std::string file);
    std::string getFileExtension(std::string file);
    bool startsWith(const std::string &candidate, const std::string &beginning);
    bool endsWith(const std::string &candidate, const std::string &ending);
    FbxDouble2 parseDouble2(FbxProperty property);
    FbxDouble3 parseDouble3(FbxProperty property);
    FbxDouble4 parseDouble4(FbxProperty property);
    std::string parseTexture(FbxProperty property);
    bool parseBool(FbxProperty property);
    float parseFloat(FbxProperty property);

};

#endif /* VROFBXExporter_h */
