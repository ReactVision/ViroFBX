//
//  VROFbxToObjConverter.cpp
//  ViroFBX
//
//  Created by Raj Advani on 4/8/19.
//  Copyright © 2019 Viro. All rights reserved.
//

#include "VROFbxToObjConverter.h"
#include <set>
#include <fstream>
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <array>
#include <sstream>

#include <Foundation/Foundation.h>
#define pinfo(message,...) \
do { \
NSLog(@#message, ##__VA_ARGS__); \
} while (0)

#define passert(condition) (assert(condition))

VROFbxToObjConverter::VROFbxToObjConverter() {
    _fbxManager = FbxManager::Create();

}

VROFbxToObjConverter::~VROFbxToObjConverter() {
    _fbxManager->Destroy();
}

FbxScene *VROFbxToObjConverter::loadFBX(std::string fbxPath) {
    _fbxPath = fbxPath;

    FbxIOSettings *ios = FbxIOSettings::Create(_fbxManager, IOSROOT);
    _fbxManager->SetIOSettings(ios);

    FbxImporter *importer = FbxImporter::Create(_fbxManager, "");

    pinfo("Loading file [%s]", fbxPath.c_str());
    bool importStatus = importer->Initialize(fbxPath.c_str());
    if (!importStatus) {
        pinfo("Call to FBXImporter::Initialize() failed");
        pinfo("Error returned: %s", importer->GetStatus().GetErrorString());

        return nullptr;
    }
    else {
        pinfo("Import successful");
    }

    FbxScene *scene = FbxScene::Create(_fbxManager, "scene");
    importer->Import(scene);
    importer->Destroy();

    /*
     Convert the file to the default axis sytem.
     */
    FbxAxisSystem axisSystem;
    static const FbxAxisSystem::EUpVector defaultUpAxis = FbxAxisSystem::eYAxis;
    static const FbxAxisSystem::EFrontVector defaultFrontAxis = FbxAxisSystem::eParityOdd;
    static const FbxAxisSystem::ECoordSystem defaultCoordSystem = FbxAxisSystem::eRightHanded;

    FbxAxisSystem origAxisSystem = scene->GetGlobalSettings().GetAxisSystem();
    int upAxisSign;
    FbxAxisSystem::EUpVector upAxis = origAxisSystem.GetUpVector(upAxisSign);
    pinfo("   Original up axis %d, sign %d", (int)upAxis, upAxisSign);

    int forwardAxisSign;
    FbxAxisSystem::EFrontVector forwardAxis = origAxisSystem.GetFrontVector(forwardAxisSign);
    pinfo("   Original forward axis %d, sign %d", (int)forwardAxis, forwardAxisSign);

    FbxAxisSystem::ECoordSystem coordSystem = origAxisSystem.GetCoorSystem();
    pinfo("   Coordinate system %d", (int)coordSystem);

    FbxAxisSystem axis(defaultUpAxis, defaultFrontAxis, defaultCoordSystem);
    axis.ConvertScene(scene);
    return scene;
}

#pragma mark - Export Geometry

void VROFbxToObjConverter::convertToObj(std::string fbxPath, std::string outPath) {
    FbxScene *scene = loadFBX(fbxPath);
    if (scene == nullptr) {
        return;
    }

    pinfo("Triangulating scene...");

    FbxGeometryConverter converter(_fbxManager);
    converter.Triangulate(scene, true);

    pinfo("Exporting FBX to OBJ...");

    FbxNode *rootNode = scene->GetRootNode();
    if (rootNode) {
        for (int i = 0; i < rootNode->GetChildCount(); i++) {
            if (isExportableNode(rootNode->GetChild(i))) {
                convertToObj(rootNode->GetChild(i), outPath);
            }
        }
    }
    pinfo("Conversion complete");
}

bool VROFbxToObjConverter::isExportableNode(FbxNode *node) {
    if (!node->GetNodeAttribute() || !node->GetNodeAttribute()->GetAttributeType()) {
        pinfo("Encountered node with no attribute type -- skipping");
        return false;
    }

    // Do not export skeleton nodes here (processed separately)
    if (node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton) {
        return false;
    }

    // Skip camera and light nodes
    if (node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eCamera ||
        node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLight) {
        return false;
    }

    // Skip invisible nodes
    if (!node->GetVisibility()) {
        if (node->GetMesh() != nullptr) {
            pinfo("Not exporting geometry of node [%s] -- visibility is false", node->GetName());
        }
        return false;
    }

    // Skip nodes with no mesh
    if (node->GetMesh() == nullptr) {
        return false;
    }

    return true;
}

void VROFbxToObjConverter::convertToObj(FbxNode *node, std::string outPath) {
    std::stringstream ss;
    ss << "File converted from FBX" << "\n" << "\n";

    FbxMesh *mesh = node->GetMesh();
    if (!mesh) {
        pinfo("Failed to export, null mesh!");
    }

    mesh->GenerateTangentsData(0);
    int cornerCounter = 0;

    /*
     Get the UV set names. For now we only use the first one.
     */
    const char *uvSetName = nullptr;
    int layerCount = mesh->GetLayerCount();
    if (layerCount > 0) {
        for (int uvLayerIndex = 0; uvLayerIndex < layerCount; uvLayerIndex++) {
            FbxLayer *layer = mesh->GetLayer(uvLayerIndex);

            // One layer can have multiple UV sets
            int uvSetCount = layer->GetUVSetCount();
            FbxArray<const FbxLayerElementUV *> sets = layer->GetUVSets();

            for (int uvIndex = 0; uvIndex < uvSetCount; uvIndex++) {
                FbxLayerElementUV const* elementUV = sets[uvIndex];
                if (elementUV) {
                    uvSetName = elementUV->GetName();
                }
            }
        }
    }

    pinfo("   UV set name %s", uvSetName);

    /*
     Export the vertex, normal, tex-coord, and tangent data (the geometry
     sources).
     */
    std::vector<double> vertices;

    int numVertices = mesh->GetControlPointsCount();
    pinfo("   Vertices count %d", numVertices);
    ss << "# " << numVertices << " vertices" << "\n";

    for (int i = 0; i < numVertices; i++) {
        FbxVector4 vertex = mesh->GetControlPointAt(i);
        ss << "v " << vertex.mData[0] << " " << vertex.mData[1] << " " << vertex.mData[2] << "\n";
    }
    ss << "\n";

    int numPolygons = mesh->GetPolygonCount();
    pinfo("   Polygon count %d", numPolygons);

    std::vector<std::pair<double, double>> texcoords;
    std::vector<std::vector<double>> normals;
    std::vector<int> vertexIndices;
    std::vector<int> texNormalIndices;

    for (unsigned int i = 0; i < numPolygons; i++) {
        // We only support triangles
        int polygonSize = mesh->GetPolygonSize(i);
        passert (polygonSize == 3);

        for (int j = 0; j < polygonSize; j++) {
            int vertexIndex = mesh->GetPolygonVertex(i, j);
            vertexIndices.push_back(vertexIndex + 1);
            texNormalIndices.push_back((int) texNormalIndices.size() + 1);

            FbxVector2 uv;
            bool unmapped;
            bool hasUV = mesh->GetPolygonVertexUV(i, j, uvSetName, uv, unmapped);
            passert (hasUV && !unmapped);

            texcoords.push_back({ uv.mData[0], uv.mData[1] });
            // ? 1 - uv.mData[1];

            FbxVector4 normal;
            bool hasNormal = mesh->GetPolygonVertexNormal(i, j, normal);
            passert (hasNormal);

            normals.push_back({ normal.mData[0], normal.mData[1], normal.mData[2] });
            ++cornerCounter;
        }
    }

    int numTexcoords = (int) texcoords.size();
    pinfo("     Texcoords count %d", numTexcoords);
    ss << "# " << numTexcoords << " texcoords" << "\n";

    for (int i = 0; i < numTexcoords; i++) {
        std::pair<double, double> texcoord = texcoords[i];
        ss << "vt " << texcoord.first << " " << texcoord.second << " 0" << "\n";
    }
    ss << "\n";

    int numNormals = (int) normals.size();
    pinfo("     Normals count %d", numNormals);
    ss << "# " << numNormals << " normals" << "\n";

    for (int i = 0; i < numNormals; i++) {
        std::vector<double> normal = normals[i];
        ss << "vn " << normal[0] << " " << normal[1] << " " << normal[2] << "\n";
    }
    ss << "\n";

    int numFaces = (int) vertexIndices.size() / 3;
    pinfo("     Faces count %d", numFaces);
    ss << "# " << numFaces << " faces" << "\n";
    passert (vertexIndices.size() == texNormalIndices.size());

    for (int i = 0; i < numFaces; i++) {
        int v0 = vertexIndices[i * 3 + 0];
        int v1 = vertexIndices[i * 3 + 1];
        int v2 = vertexIndices[i * 3 + 2];

        int tn0 = texNormalIndices[i * 3 + 0];
        int tn1 = texNormalIndices[i * 3 + 1];
        int tn2 = texNormalIndices[i * 3 + 2];

        ss << "f " << v0 << "/" << tn0 << "/" << tn0 << " " << v1 << "/" << tn1 << "/" << tn1 << " " << v2 << "/" << tn2 << "/" << tn2 << "\n";
    }

    std::ofstream outFile;
    outFile.open(outPath);
    outFile << ss.rdbuf();
}
