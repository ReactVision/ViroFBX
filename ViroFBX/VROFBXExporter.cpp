//
//  VROFBXExporter.cpp
//  ViroFBX
//
//  Created by Raj Advani on 4/25/17.
//  Copyright © 2017 Viro. All rights reserved.
//

#include "VROFBXExporter.h"
#include "VROLog.h"
#include <set>
#include <fstream>
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <array>
#include "VROUtil.h"

static const bool kDebugGeometrySource = false;
static const bool kDebugBones = false;
static const bool kDebugNodeTransforms = false;

static const int kAnimationFPS = 30;
static const float kEpsilon = 0.00000001;

FbxString GetAttributeTypeName(FbxNodeAttribute::EType type) {
    switch(type) {
        case FbxNodeAttribute::eUnknown: return "unidentified";
        case FbxNodeAttribute::eNull: return "null";
        case FbxNodeAttribute::eMarker: return "marker";
        case FbxNodeAttribute::eSkeleton: return "skeleton";
        case FbxNodeAttribute::eMesh: return "mesh";
        case FbxNodeAttribute::eNurbs: return "nurbs";
        case FbxNodeAttribute::ePatch: return "patch";
        case FbxNodeAttribute::eCamera: return "camera";
        case FbxNodeAttribute::eCameraStereo: return "stereo";
        case FbxNodeAttribute::eCameraSwitcher: return "camera switcher";
        case FbxNodeAttribute::eLight: return "light";
        case FbxNodeAttribute::eOpticalReference: return "optical reference";
        case FbxNodeAttribute::eOpticalMarker: return "marker";
        case FbxNodeAttribute::eNurbsCurve: return "nurbs curve";
        case FbxNodeAttribute::eTrimNurbsSurface: return "trim nurbs surface";
        case FbxNodeAttribute::eBoundary: return "boundary";
        case FbxNodeAttribute::eNurbsSurface: return "nurbs surface";
        case FbxNodeAttribute::eShape: return "shape";
        case FbxNodeAttribute::eLODGroup: return "lodgroup";
        case FbxNodeAttribute::eSubDiv: return "subdiv";
        default: return "unknown";
    }
}

#pragma mark - Initialization

VROFBXExporter::VROFBXExporter() {
    _fbxManager = FbxManager::Create();

}

VROFBXExporter::~VROFBXExporter() {
    _fbxManager->Destroy();
}

FbxScene *VROFBXExporter::loadFBX(std::string fbxPath) {
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
    
    // Bake in the pivot information: pre-rotation, post-rotation, rotation pivot, scale pivot,
    // etc., into the local transforms (node rotation, translation scale). While Viro supports
    // pivots, it does not support rotation and scale offsets
    //
    // NOTE: This apparently stopped working after FBX SDK 2015 (tried in versions 2016 and 2018)
    //       https://forums.autodesk.com/t5/fbx-forum/issue-with-convertpivotanimationrecursive-and-default-transform/td-p/6887073
    pinfo("Baking pivots...");
    scene->GetRootNode()->ResetPivotSetAndConvertAnimation(kAnimationFPS);

    return scene;
}

#pragma mark - Export Geometry

void VROFBXExporter::exportFBX(std::string fbxPath, std::string destPath, bool compressTextures) {
    FbxScene *scene = loadFBX(fbxPath);
    if (scene == nullptr) {
        return;
    }
    
    pinfo("Triangulating scene...");
    
    FbxGeometryConverter converter(_fbxManager);
    converter.Triangulate(scene, true);
    
    pinfo("Exporting FBX...");
    viro::Node *outNode = new viro::Node();

    FbxNode *rootNode = scene->GetRootNode();
    
    FbxDouble3 translation = rootNode->LclTranslation.Get();
    outNode->add_position(translation[0]);
    outNode->add_position(translation[1]);
    outNode->add_position(translation[2]);
    
    FbxDouble3 scaling = rootNode->LclScaling.Get();
    outNode->add_scale(scaling[0]);
    outNode->add_scale(scaling[1]);
    outNode->add_scale(scaling[2]);
    
    FbxDouble3 rotation = rootNode->LclRotation.Get();
    outNode->add_rotation(rotation[0]);
    outNode->add_rotation(rotation[1]);
    outNode->add_rotation(rotation[2]);
    
    if (rootNode) {
        pinfo("Exporting skeleton");
        // The skeleton is exported into the root node
        viro::Node::Skeleton *skeleton = outNode->mutable_skeleton();
        exportSkeleton(rootNode, skeleton);
        
        for (int i = 0; i < rootNode->GetChildCount(); i++) {
            if (isExportableNode(rootNode->GetChild(i))) {
                exportNode(scene, rootNode->GetChild(i), 0, compressTextures,
                           *skeleton, outNode->add_subnode());
            }
        }
    }
    
    int byteSize = outNode->ByteSize();
    
    pinfo("Encoding protobuf [%d bytes]...", byteSize);
    std::string encoded = outNode->SerializeAsString();
    pinfo("Compressing...");
    std::string compressed = compressString(encoded);
    pinfo("Writing protobuf [%d bytes]", (int)compressed.size());
    std::ofstream(destPath.c_str(), std::ios::binary).write(compressed.c_str(), compressed.size());
    
    pinfo("Export complete");
}

bool VROFBXExporter::isExportableNode(FbxNode *node) {
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
    
    return true;
}

void VROFBXExporter::exportNode(FbxScene *scene, FbxNode *node, int depth, bool compressTextures,
                                const viro::Node::Skeleton &skeleton, viro::Node *outNode) {
    passert (isExportableNode(node));
    
    pinfo("Exporting node [%s], type [%s]", node->GetName(),
          GetAttributeTypeName(node->GetNodeAttribute()->GetAttributeType()).Buffer());
    outNode->set_name(node->GetName());
    
    FbxDouble3 translation = node->LclTranslation.Get();
    outNode->add_position(translation[0]);
    outNode->add_position(translation[1]);
    outNode->add_position(translation[2]);
    if (kDebugNodeTransforms && (translation[0] != 0 || translation[1] != 0 || translation[2] != 0)) {
        pinfo("   Node translation %f, %f, %f", translation[0], translation[1], translation[2]);
    }
    
    FbxDouble3 scaling = node->LclScaling.Get();
    outNode->add_scale(scaling[0]);
    outNode->add_scale(scaling[1]);
    outNode->add_scale(scaling[2]);
    if (kDebugNodeTransforms && (scaling[0] != 1 || scaling[1] != 1 || scaling[2] != 1)) {
        pinfo("   Node scale %f, %f, %f", scaling[0], scaling[1], scaling[2]);
    }

    FbxDouble3 rotation = node->LclRotation.Get();
    outNode->add_rotation(rotation[0]);
    outNode->add_rotation(rotation[1]);
    outNode->add_rotation(rotation[2]);
    if (kDebugNodeTransforms && (rotation[0] != 0 || rotation[1] != 0 || rotation[2] != 0)) {
        pinfo("   Node rotation %f, %f, %f", rotation[0], rotation[1], rotation[2]);
    }
    
    FbxVector4 geoTranslation = node->GetGeometricTranslation(FbxNode::eSourcePivot);
    FbxVector4 geoRotation = node->GetGeometricRotation(FbxNode::eSourcePivot);
    FbxVector4 geoScale = node->GetGeometricScaling(FbxNode::eSourcePivot);
    
    if (geoTranslation[0] != 0 || geoTranslation[1] != 0 || geoTranslation[2] != 0) {
        pinfo("   Geo-only translation %f, %f, %f", geoTranslation[0], geoTranslation[1], geoTranslation[2]);
    }
    if (geoRotation[0] != 0 || geoRotation[1] != 0 || geoRotation[2] != 0) {
        pinfo("   Geo-only rotation %f, %f, %f", geoRotation[0], geoRotation[1], geoRotation[2]);
    }
    
    outNode->set_rendering_order(0);
    outNode->set_opacity(1.0);
    
    if (node->GetMesh() != nullptr) {
        pinfo("   Exporting geometry");
        exportGeometry(node, depth, compressTextures, outNode->mutable_geometry());
        
        /*
         Export the skin, if there is a skeleton.
         */
        if (skeleton.bone_size() > 0 && node->GetMesh()->GetDeformerCount() > 0) {
            pinfo("   Exporting skin");
            exportSkin(node, skeleton, outNode->mutable_geometry()->mutable_skin());
            
            pinfo("   Exporting skeletal animations");
            exportSkeletalAnimations(scene, node, skeleton, outNode);
        }
        else {
            pinfo("   No skeleton found, will not export skin");
        }
        
        pinfo("   Exporting keyframe animations");
        exportSampledKeyframeAnimations(scene, node, outNode);
        
        pinfo("   Exporting blend shape animations");
        exportBlendShapeAnimations(scene, node, outNode);
    }
    for (int i = 0; i < node->GetChildCount(); i++) {
        if (isExportableNode(node->GetChild(i))) {
            exportNode(scene, node->GetChild(i), depth, compressTextures,
                       skeleton, outNode->add_subnode());
        }
    }
}

void VROFBXExporter::exportGeometry(FbxNode *node, int depth, bool compressTextures, viro::Node::Geometry *geo) {
    FbxMesh *mesh = node->GetMesh();
    passert_msg (mesh, "Failed to export, null mesh!");
    
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
    
    pinfo("      UV set name %s", uvSetName);
    
    /*
     Export the vertex, normal, tex-coord, and tangent data (the geometry
     sources).
     */
    std::vector<float> data;
    
    int numPolygons = mesh->GetPolygonCount();
    pinfo("      Polygon count %d", numPolygons);

    for (unsigned int i = 0; i < numPolygons; i++) {
        if (kDebugGeometrySource) {
            pinfo("      Reading polygon %d", i);
        }
        
        // We only support triangles
        int polygonSize = mesh->GetPolygonSize(i);
        passert (polygonSize == 3);
        
        for (int j = 0; j < polygonSize; j++) {
            if (kDebugGeometrySource) {
                pinfo("         V%d", j);
            }
            int controlPointIndex = mesh->GetPolygonVertex(i, j);
            
            FbxVector4 vertex = mesh->GetControlPointAt(controlPointIndex);
            data.push_back(vertex.mData[0]);
            data.push_back(vertex.mData[1]);
            data.push_back(vertex.mData[2]);
            
            if (kDebugGeometrySource) {
                pinfo("            Read vertex %f, %f, %f", vertex.mData[0], vertex.mData[1], vertex.mData[2]);
            }
            
            FbxVector2 uv;
            bool unmapped;
            bool hasUV = mesh->GetPolygonVertexUV(i, j, uvSetName, uv, unmapped);
            
            if (kDebugGeometrySource) {
                pinfo("            Read UV %f, %f", uv.mData[0], uv.mData[1]);
            }
            
            if (hasUV && !unmapped) {
                data.push_back(uv.mData[0]);
                data.push_back(1 - uv.mData[1]);
            }
            else {
                data.push_back(0);
                data.push_back(0);
            }
            
            FbxVector4 normal;
            bool hasNormal = mesh->GetPolygonVertexNormal(i, j, normal);
            
            if (kDebugGeometrySource) {
                pinfo("            Read normal %f, %f, %f", normal.mData[0], normal.mData[1], normal.mData[2]);
            }
            
            if (hasNormal) {
                data.push_back(normal.mData[0]);
                data.push_back(normal.mData[1]);
                data.push_back(normal.mData[2]);
            }
            else {
                data.push_back(0);
                data.push_back(0);
                data.push_back(0);
            }
            
            FbxVector4 tangent = readTangent(mesh, controlPointIndex, cornerCounter);
            data.push_back(tangent.mData[0]);
            data.push_back(tangent.mData[1]);
            data.push_back(tangent.mData[2]);
            data.push_back(tangent.mData[3]);
            
            if (kDebugGeometrySource) {
                pinfo("            Read tangent %f, %f, %f, %f", tangent.mData[0], tangent.mData[1], tangent.mData[2], tangent.mData[3]);
            }
            ++cornerCounter;
        }
    }
    
    int floatsPerVertex = 12;
    int numVertices = (uint32_t)data.size() / floatsPerVertex;
    int stride = floatsPerVertex * sizeof(float);
    
    passert (numPolygons * 3 * stride == data.size() * sizeof(float));
    geo->set_data(data.data(), data.size() * sizeof(float));
    
    pinfo("      Num vertices %d, stride %d", numVertices, stride);
    pinfo("      VAR size %lu", data.size() * sizeof(float));
    
    viro::Node::Geometry::Source *vertices = geo->add_source();
    vertices->set_semantic(viro::Node_Geometry_Source_Semantic_Vertex);
    vertices->set_vertex_count(numVertices);
    vertices->set_float_components(true);
    vertices->set_components_per_vertex(3);
    vertices->set_bytes_per_component(sizeof(float));
    vertices->set_data_offset(0);
    vertices->set_data_stride(stride);
    
    viro::Node::Geometry::Source *texCoords = geo->add_source();
    texCoords->set_semantic(viro::Node_Geometry_Source_Semantic_Texcoord);
    texCoords->set_vertex_count(numVertices);
    texCoords->set_float_components(true);
    texCoords->set_components_per_vertex(2);
    texCoords->set_bytes_per_component(sizeof(float));
    texCoords->set_data_offset(sizeof(float) * 3);
    texCoords->set_data_stride(stride);
    
    viro::Node::Geometry::Source *normals = geo->add_source();
    normals->set_semantic(viro::Node_Geometry_Source_Semantic_Normal);
    normals->set_vertex_count(numVertices);
    normals->set_float_components(true);
    normals->set_components_per_vertex(3);
    normals->set_bytes_per_component(sizeof(float));
    normals->set_data_offset(sizeof(float) * 5);
    normals->set_data_stride(stride);
    
    viro::Node::Geometry::Source *tangents = geo->add_source();
    tangents->set_semantic(viro::Node_Geometry_Source_Semantic_Tangent);
    tangents->set_vertex_count(numVertices);
    tangents->set_float_components(true);
    tangents->set_components_per_vertex(4);
    tangents->set_bytes_per_component(sizeof(float));
    tangents->set_data_offset(sizeof(float) * 8);
    tangents->set_data_stride(stride);
    
    /*
     Export the elements and materials.
     */
    int numMaterials = node->GetMaterialCount();
    pinfo("   Exporting materials");
    pinfo("      Num materials %d", numMaterials);
    
    std::vector<int> materialMapping = readMaterialToMeshMapping(mesh, numPolygons);
    if (numMaterials > 0) {
        for (int i = 0; i < numMaterials; i++) {
            std::vector<int> triangles;
            
            for (int face = 0; face < materialMapping.size(); face++) {
                int materialIndex = materialMapping[face];
                if (materialIndex == i) {
                    triangles.push_back(face * 3 + 0);
                    triangles.push_back(face * 3 + 1);
                    triangles.push_back(face * 3 + 2);
                }
            }
            
            viro::Node::Geometry::Element *element = geo->add_element();
            element->set_data(triangles.data(), triangles.size() * sizeof(int));
            element->set_primitive(viro::Node_Geometry_Element_Primitive_Triangle);
            element->set_bytes_per_index(sizeof(int));
            element->set_primitive_count((int)triangles.size() / 3);
            
            pinfo("      Primitive count for material %d: %d", i, element->primitive_count());
            
            FbxSurfaceMaterial *fbxMaterial = node->GetMaterial(i);
            viro::Node::Geometry::Material *material = geo->add_material();
            
            exportMaterial(fbxMaterial, compressTextures, material);
        }
    }
    else {
        // If there are no materials, export a default (blank) material
        std::vector<int> triangles;
        for (int face = 0; face < materialMapping.size(); face++) {
            triangles.push_back(face * 3 + 0);
            triangles.push_back(face * 3 + 1);
            triangles.push_back(face * 3 + 2);
        }
        
        viro::Node::Geometry::Element *element = geo->add_element();
        element->set_data(triangles.data(), triangles.size() * sizeof(int));
        element->set_primitive(viro::Node_Geometry_Element_Primitive_Triangle);
        element->set_bytes_per_index(sizeof(int));
        element->set_primitive_count((int)triangles.size() / 3);
        
        viro::Node::Geometry::Material *material = geo->add_material();
        material->set_transparency(1.0);

        viro::Node::Geometry::Material::Visual *diffuse = material->mutable_diffuse();
        diffuse->add_color(1.0);
        diffuse->add_color(1.0);
        diffuse->add_color(1.0);
        diffuse->set_intensity(1.0);
    }
}

// It turns out we can just use mesh->GetPolygonVertexNormal() instead of this function,
// but keeping this here for educational purposes, on how to read normals directly from
// a mesh.
FbxVector4 VROFBXExporter::readNormal(FbxMesh *mesh, int controlPointIndex, int cornerCounter) {
    if (mesh->GetElementNormalCount() < 1) {
        return {};
    }
    
    FbxGeometryElementNormal *normalsLayer = mesh->GetElementNormal(0);
    
    switch (normalsLayer->GetMappingMode()) {
        case FbxGeometryElement::eByControlPoint:
            switch(normalsLayer->GetReferenceMode()) {
                case FbxGeometryElement::eDirect: {
                    return normalsLayer->GetDirectArray().GetAt(controlPointIndex);
                }
                case FbxGeometryElement::eIndexToDirect: {
                    int index = normalsLayer->GetIndexArray().GetAt(controlPointIndex);
                    return normalsLayer->GetDirectArray().GetAt(index);
                }
                default:
                    pabort("Invalid reference mode");
        }
        break;
            
        case FbxGeometryElement::eByPolygonVertex:
            switch(normalsLayer->GetReferenceMode()) {
                case FbxGeometryElement::eDirect: {
                    return normalsLayer->GetDirectArray().GetAt(cornerCounter);
                }
                break;
                case FbxGeometryElement::eIndexToDirect: {
                    int index = normalsLayer->GetIndexArray().GetAt(cornerCounter);
                    return normalsLayer->GetDirectArray().GetAt(index);
                }
                break;
                default:
                    pabort("Invalid reference mode");
        }
            
        default:
            pabort("Invalid mapping mode");
            break;
    }
    
    return {};
}

FbxVector4 VROFBXExporter::readTangent(FbxMesh *mesh, int controlPointIndex, int cornerCounter) {
    if(mesh->GetElementTangentCount() < 1) {
        return {};
    }
    
    FbxGeometryElementTangent *tangentsLayer = mesh->GetElementTangent(0);
    
    switch (tangentsLayer->GetMappingMode()) {
        case FbxGeometryElement::eByControlPoint:
            switch(tangentsLayer->GetReferenceMode()) {
                case FbxGeometryElement::eDirect: {
                    return tangentsLayer->GetDirectArray().GetAt(controlPointIndex);
                }
                case FbxGeometryElement::eIndexToDirect: {
                    int index = tangentsLayer->GetIndexArray().GetAt(controlPointIndex);
                    return tangentsLayer->GetDirectArray().GetAt(index);
                }
                default:
                    pabort("Invalid reference mode");
            }
            break;
            
        case FbxGeometryElement::eByPolygonVertex:
            switch(tangentsLayer->GetReferenceMode()) {
                case FbxGeometryElement::eDirect: {
                    return tangentsLayer->GetDirectArray().GetAt(cornerCounter);
                }
                    break;
                case FbxGeometryElement::eIndexToDirect: {
                    int index = tangentsLayer->GetIndexArray().GetAt(cornerCounter);
                    return tangentsLayer->GetDirectArray().GetAt(index);
                }
                    break;
                default:
                    pabort("Invalid reference mode");
            }
            
        default:
            pabort("Invalid mapping mode");
            break;
    }
    
    return {};
}

FbxAMatrix VROFBXExporter::getGeometryMatrix(FbxNode *node) {
    /*
     The geometry transform only applies to the geometry of a node, and does
     not cascade into child nodes.
     */
    const FbxVector4 lT = node->GetGeometricTranslation(FbxNode::eSourcePivot);
    const FbxVector4 lR = node->GetGeometricRotation(FbxNode::eSourcePivot);
    const FbxVector4 lS = node->GetGeometricScaling(FbxNode::eSourcePivot);
    
    return FbxAMatrix(lT, lR, lS);
}

#pragma mark - Export Skeleton and Animations

void VROFBXExporter::exportSkeleton(FbxNode *rootNode, viro::Node::Skeleton *outSkeleton) {
    for (int i = 0; i < rootNode->GetChildCount(); ++i) {
        FbxNode *node = rootNode->GetChild(i);
        exportSkeletonRecursive(node, 0, 0, -1, outSkeleton);
    }
}

void VROFBXExporter::exportSkeletonRecursive(FbxNode *node, int depth, int index, int parentIndex, viro::Node::Skeleton *outSkeleton) {
    if (node->GetNodeAttribute() &&
        node->GetNodeAttribute()->GetAttributeType() &&
        node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton) {
        
        printf("   %*sExporting skeleton node [%s]\n", depth * 3, "", node->GetName());

        viro::Node::Skeleton::Bone *bone = outSkeleton->add_bone();
        bone->set_name(node->GetName());
        bone->set_parent_index(parentIndex);
    }
    for (int i = 0; i < node->GetChildCount(); i++) {
        exportSkeletonRecursive(node->GetChild(i), depth + 1, outSkeleton->bone_size(), index, outSkeleton);
    }
}

bool SortByBoneWeight(const VROBoneIndexWeight &i, const VROBoneIndexWeight &j) {
    return (i.weight > j.weight);
}

void VROFBXExporter::exportSkin(FbxNode *node, const viro::Node::Skeleton &skeleton, viro::Node::Geometry::Skin *outSkin) {
    FbxMesh *mesh = node->GetMesh();
    
    /*
     Each FBX deformer contains clusters. Clusters each contain a link (which is a
     bone). Normally only one deformer per mesh.
     */
    unsigned int numDeformers = mesh->GetDeformerCount();
    std::map<int, std::vector<VROBoneIndexWeight>> bones;
    
    for (unsigned int deformerIndex = 0; deformerIndex < numDeformers; ++deformerIndex) {
        // We only use skin deformers for skeletal animation
        FbxSkin *skin = reinterpret_cast<FbxSkin*>(mesh->GetDeformer(deformerIndex, FbxDeformer::eSkin));
        if (!skin) {
            pinfo("Unsupported deformer");
            continue;
        }
        
        unsigned int numClusters = skin->GetClusterCount();
        pinfo("      Number of clusters %d", numClusters);
        
        // We will be filling in the bind transform for each bone
        for (int b = 0; b < skeleton.bone_size(); b++) {
            outSkin->add_bind_transform();
        }
        
        /*
         Encode the transform that gets us from model space, original position 
         to world space, bind position.
         */
        FbxAMatrix geometryBindingTransform;
        if (numClusters > 0) {
            skin->GetCluster(0)->GetTransformMatrix(geometryBindingTransform);
            geometryBindingTransform *= getGeometryMatrix(node);
            
            viro::Node::Matrix *gbt = outSkin->mutable_geometry_bind_transform();
            for (int i = 0; i < 16; i++) {
                gbt->add_value(geometryBindingTransform.Get(i / 4, i % 4));
            }
        }
        
        for (unsigned int clusterIndex = 0; clusterIndex < numClusters; ++clusterIndex) {
            FbxCluster *cluster = skin->GetCluster(clusterIndex);
            
            /*
             Set the bind transform for the bone. The bind transform transforms
             the mesh from its encoded position in model space to the bind position
             in bone local space.
             */
            std::string boneName = cluster->GetLink()->GetName();
            unsigned int boneIndex = findBoneIndexUsingName(boneName, skeleton);
            
            /*
             If a model has more bones that we support, we ignore the bones that are beyond
             our max bone index. This will likely ruin the animation, so log a warning.
             */
            if (boneIndex >= kMaxBones) {
                pinfo("Warning: Viro only supports %d bones: bone %d will be discarded, animations may be skewed",
                      kMaxBones, boneIndex);
                continue;
            }
        
            /*
             Sanity check: the transform matrix for each cluster's parent node should
             be the same (it's the same parent!).
             */
            FbxAMatrix meshBindingTransform;
            cluster->GetTransformMatrix(meshBindingTransform);
            meshBindingTransform *= getGeometryMatrix(node);
            
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    passert (fabs(geometryBindingTransform.Get(i, j) - meshBindingTransform.Get(i, j)) < .0001);
                }
            }
            
            /*
             Encode the matrix that, for each bone, takes us from world space, bind position 
             to bone local space, bind position.
             */
            FbxAMatrix boneBindingTransform;
            cluster->GetTransformLinkMatrix(boneBindingTransform);
            FbxAMatrix inverseBoneBindingTransform = boneBindingTransform.Inverse();
            
            viro::Node::Matrix *bt = outSkin->mutable_bind_transform(boneIndex);
            for (int i = 0; i < 16; i++) {
                bt->add_value(inverseBoneBindingTransform.Get(i / 4, i % 4));
            }
            
            /*
             Associate each bone with the control points it affects.
             */
            unsigned int numIndices = cluster->GetControlPointIndicesCount();
            for (unsigned int i = 0; i < numIndices; ++i) {
                int controlPointIndex = cluster->GetControlPointIndices()[i];
                float boneWeight = cluster->GetControlPointWeights()[i];
                
                /*
                 Associates a control point (vertex) with a bone, and gives
                 the bone a weight on that control point.
                 */
                VROBoneIndexWeight bone(boneIndex, boneWeight);
                bones[controlPointIndex].push_back(bone);
            }
        }
    }
    
    int numPointsWithTooManyBones = 0;
    for (auto &kv : bones) {
        std::vector<VROBoneIndexWeight> &bones = kv.second;
        
        // If there are more than kMaxBoneInfluences bones, use only those with greatest
        // weight, and renormalize
        if (bones.size() > kMaxBoneInfluences) {
            ++numPointsWithTooManyBones;
            if (kDebugBones) {
                pinfo("Control point has %d bones", (int)bones.size());
            }
            
            std::sort(bones.begin(), bones.end(), SortByBoneWeight);
            bones.erase(bones.begin() + kMaxBoneInfluences, bones.end());
            
            float total = 0;
            for (VROBoneIndexWeight &bone : bones) {
                total += bone.weight;
            }
            for (VROBoneIndexWeight &bone : bones) {
                if (kDebugBones) {
                    pinfo("   Refactoring bone weight from %f to %f", bone.weight, (bone.weight / total));
                }
                bone.weight = (bone.weight / total);
            }
        }
        
        // Add extra dummy bones to each control point that has fewer than
        // kMaxBoneInfluences
        for (size_t i = bones.size(); i < kMaxBoneInfluences; ++i) {
            bones.push_back({0, 0});
        }
    }
    
    pinfo("      Found bones for %lu control points", bones.size());
    if (numPointsWithTooManyBones > 0) {
        pinfo("************************");
        pinfo("WARN: Found %d control points with too many bone influences. Max 4 supported!", numPointsWithTooManyBones);
        pinfo("************************");
    }
    
    /*
     Export the geometry sources for the skin. Note this must be in the same order we 
     follow in exportGeometry.
     */
    std::vector<int> boneIndicesData;
    std::vector<float> boneWeightsData;
    std::set<int> controlPointsNoBones;
    
    int numPolygons = mesh->GetPolygonCount();
    for (unsigned int i = 0; i < numPolygons; i++) {
        // We only support triangles
        int polygonSize = mesh->GetPolygonSize(i);
        passert (polygonSize == 3);
        
        for (int j = 0; j < polygonSize; j++) {
            int controlPointIndex = mesh->GetPolygonVertex(i, j);
            
            auto it = bones.find(controlPointIndex);
            if (it == bones.end()) {
                // This is just so we only log the message once per control point instead
                // of once per polygon
                if (controlPointsNoBones.find(controlPointIndex) == controlPointsNoBones.end()) {
                    controlPointsNoBones.insert(controlPointIndex);
                    pinfo("         No bones found for control point %d", controlPointIndex);
                }
                
                for (int b = 0; b < kMaxBoneInfluences; b++) {
                    boneIndicesData.push_back(0);
                    boneWeightsData.push_back(0);
                }
            }
            else {
                std::vector<VROBoneIndexWeight> &boneData = it->second;
                passert (boneData.size() == kMaxBoneInfluences);
                
                if (kDebugGeometrySource) {
                    pinfo("      Control point %d", controlPointIndex);
                }
                for (VROBoneIndexWeight &bone : boneData) {
                    boneIndicesData.push_back(bone.index);
                    boneWeightsData.push_back(bone.weight);
                    
                    if (kDebugGeometrySource) {
                        pinfo("         bone-index: %d", bone.index);
                        pinfo("         bone-weight: %f", bone.weight);
                    }
                }
            }
        }
    }
    
    int intsPerVertex = kMaxBoneInfluences;
    int numVertices = (uint32_t)boneIndicesData.size() / intsPerVertex;
    
    viro::Node::Geometry::Source *boneIndices = outSkin->mutable_bone_indices();
    boneIndices->set_semantic(viro::Node_Geometry_Source_Semantic_BoneIndices);
    boneIndices->set_vertex_count(numVertices);
    boneIndices->set_float_components(false);
    boneIndices->set_components_per_vertex(kMaxBoneInfluences);
    boneIndices->set_bytes_per_component(sizeof(int));
    boneIndices->set_data_offset(0);
    boneIndices->set_data_stride(sizeof(int) * kMaxBoneInfluences);
    boneIndices->set_data(boneIndicesData.data(), boneIndicesData.size() * sizeof(int));

    viro::Node::Geometry::Source *boneWeights = outSkin->mutable_bone_weights();
    boneWeights->set_semantic(viro::Node_Geometry_Source_Semantic_BoneWeights);
    boneWeights->set_vertex_count(numVertices);
    boneWeights->set_float_components(true);
    boneWeights->set_components_per_vertex(kMaxBoneInfluences);
    boneWeights->set_bytes_per_component(sizeof(float));
    boneWeights->set_data_offset(0);
    boneWeights->set_data_stride(sizeof(float) * kMaxBoneInfluences);
    boneWeights->set_data(boneWeightsData.data(), boneWeightsData.size() * sizeof(float));
}

void VROFBXExporter::exportSkeletalAnimations(FbxScene *scene, FbxNode *node, const viro::Node::Skeleton &skeleton, viro::Node *outNode) {
    FbxMesh *mesh = node->GetMesh();
    
    unsigned int numDeformers = mesh->GetDeformerCount();
    if (numDeformers <= 0) {
        pinfo("   Mesh had no deformers, not exporting animation");
        return;
    }

    FbxAnimEvaluator *evaluator = scene->GetAnimationEvaluator();
    
    /*
     Iterate through each animation stack. Each stack corresponds to a separate skeletal animation.
     */
    int numStacks = scene->GetSrcObjectCount(FbxCriteria::ObjectType(FbxAnimStack::ClassId));
    for (int i = 0; i < numStacks; i++) {
        viro::Node::SkeletalAnimation *skeletalAnimation = outNode->add_skeletal_animation();
        
        /*
         Get metadata for the animation.
         */
        FbxAnimStack *animStack = scene->GetSrcObject<FbxAnimStack>(i);
        FbxString animStackName = animStack->GetName();
        FbxTakeInfo *take = scene->GetTakeInfo(animStackName);
        FbxTime start = take->mLocalTimeSpan.GetStart();
        FbxTime end = take->mLocalTimeSpan.GetStop();
        FbxLongLong duration = end.GetMilliSeconds() - start.GetMilliSeconds();
        
        pinfo("      Animation duration %lld ms", duration);
        
        skeletalAnimation->set_name(animStackName.Buffer());
        skeletalAnimation->set_duration(duration);
        skeletalAnimation->set_has_scaling(false);

        for (FbxLongLong i = start.GetFrameCount(FbxTime::eFrames30); i <= end.GetFrameCount(FbxTime::eFrames30); ++i) {
            viro::Node::SkeletalAnimation::Frame *frame = skeletalAnimation->add_frame();
            
            FbxTime time;
            time.SetFrame(i, FbxTime::eFrames30);
            
            frame->set_time(fmin(1.0, (float) time.GetMilliSeconds() / (float) duration));
            for (unsigned int deformerIndex = 0; deformerIndex < numDeformers; ++deformerIndex) {
                // We only use skin deformers for skeletal animation
                FbxSkin *skin = reinterpret_cast<FbxSkin*>(mesh->GetDeformer(deformerIndex, FbxDeformer::eSkin));
                if (!skin) {
                    continue;
                }
                
                unsigned int numClusters = skin->GetClusterCount();
                for (unsigned int clusterIndex = 0; clusterIndex < numClusters; ++clusterIndex) {
                    FbxCluster *cluster = skin->GetCluster(clusterIndex);
                    
                    FbxNode *bone = cluster->GetLink();
                    unsigned int boneIndex = findBoneIndexUsingName(bone->GetName(), skeleton);
                    
                    frame->add_bone_index(boneIndex);
                    
                    /*
                     This computes the transform matrix for a given bone at a specific time. 
                     When animating, we first multiply each vertex by the geometryBindingTranform:
                     this takes the mesh from model space to its binding position in *world space*. 
                     From there we can move to the bone's local space by multiplying by the inverse 
                     of the bone's world transform. Once in the binding position, in bone local
                     space, we can apply the animation.
                     
                     Note: the step described above is performed in the renderer. We encode the
                     matrices required for this when exporting the skin.
                     
                     The transform that applies the animation is the boneWorldTransform: this 
                     transform is *global*: it includes all bone transforms of its parents up the
                     skeleton hierarchy, along with all node transforms. Therefore, when we multiply
                     our vertex (in bone local space) by boneWorldTransform, we end up back in
                     world space: except we've moved from our bind pose to our animated position.
                     
                     Now, in order to render in Viro, we cannot be in world space! We have to be back
                     in bone space, because Viro will itself move the geometry back to model space in
                     the skinner, and from there to world space in the shader. So to get back into bone
                     local space, we multiply the vertex again by the inverseBoneBindingTransform.
                     
                     To summarize, each transformation moves us to a different position and coordinate
                     space. This table shows the step-by-step process for getting from the model's 
                     original position in model space to its animated position in model space. The first
                     two steps are accomplished in exportSkin and are contained in the binding transform
                     that we encode in the protobuf. The next two steps are accomplished here.
                     
                     1. Model space, encoded position  --> [geometryBindingTransform]    --> World space, bind position
                     2. World space, bind position     --> [inverseBoneBindingTransform] --> Bone space, bind position
                     3. Bone space, bind position      --> [boneWorldTransform]          --> World space, animated position
                     4. World space, animated position --> [inverseBoneBindingTransform] --> Bone space, animated position
                     */
                    FbxAMatrix boneWorldTransform = evaluator->GetNodeGlobalTransform(bone, time);
        
                    FbxAMatrix boneBindingTransform;
                    cluster->GetTransformLinkMatrix(boneBindingTransform);

                    FbxAMatrix animationTransform = boneBindingTransform.Inverse() * boneWorldTransform;
                    viro::Node::Matrix *transform = frame->add_transform();
                    for (int t = 0; t < 16; t++) {
                        transform->add_value(animationTransform.Get(t / 4, t % 4));
                    }
                    
                    // Indicate if there is a scaling component of the transform
                    FbxVector4 scaleT = animationTransform.GetS();
                    if (fabs(scaleT.mData[0] - 1.0) > kEpsilon ||
                        fabs(scaleT.mData[1] - 1.0) > kEpsilon ||
                        fabs(scaleT.mData[2] - 1.0) > kEpsilon) {
                        skeletalAnimation->set_has_scaling(true);
                    }
                }
            }
        }
    }
}

void VROFBXExporter::exportBlendShapeAnimations(FbxScene *scene, FbxNode *node, viro::Node *outNode) {
    FbxMesh *mesh = node->GetMesh();
    
    unsigned int numDeformers = mesh->GetDeformerCount();
    if (numDeformers <= 0) {
        pinfo("   Mesh had no deformers, not exporting blend shape animations");
        return;
    }
    
    /*
     Iterate through each animation stack. Each stack corresponds to a separate skeletal animation.
     */
    int numStacks = scene->GetSrcObjectCount(FbxCriteria::ObjectType(FbxAnimStack::ClassId));
    for (int i = 0; i < numStacks; i++) {
        /*
         Get metadata for the animation.
         */
        FbxAnimStack *animStack = scene->GetSrcObject<FbxAnimStack>(i);
        FbxString animStackName = animStack->GetName();
        FbxTakeInfo *take = scene->GetTakeInfo(animStackName);
        FbxTime start = take->mLocalTimeSpan.GetStart();
        FbxTime end = take->mLocalTimeSpan.GetStop();
        //FbxLongLong duration = end.GetMilliSeconds() - start.GetMilliSeconds();
        
        for (FbxLongLong i = start.GetFrameCount(FbxTime::eFrames30); i <= end.GetFrameCount(FbxTime::eFrames30); ++i) {
            for (unsigned int deformerIndex = 0; deformerIndex < numDeformers; ++deformerIndex) {
                FbxBlendShape *blendShape = reinterpret_cast<FbxBlendShape *>(mesh->GetDeformer(deformerIndex, FbxDeformer::eBlendShape));
                if (!blendShape) {
                    continue;
                }
                
                pinfo("   Found blend shape deformer, but not yet supported. Will not export!");
                
                // TODO Add support for blend shape animations
            }
        }
    }
}

/*
 This method samples the animation of the global transformation matrix of the node, and returns
 keyframes accordingly. Inefficient but widely compatible.
 */
void VROFBXExporter::exportSampledKeyframeAnimations(FbxScene *scene, FbxNode *node, viro::Node *outNode) {
    FbxAnimEvaluator *evaluator = scene->GetAnimationEvaluator();
    
    /*
     Iterate through each animation stack. Each stack corresponds to a separate skeletal animation.
     */
    int numStacks = scene->GetSrcObjectCount(FbxCriteria::ObjectType(FbxAnimStack::ClassId));
    for (int i = 0; i < numStacks; i++) {
        viro::Node::KeyframeAnimation *keyframeAnimation = outNode->add_keyframe_animation();
        
        /*
         Get metadata for the animation.
         */
        FbxAnimStack *animStack = scene->GetSrcObject<FbxAnimStack>(i);
        FbxString animStackName = animStack->GetName();
        FbxTakeInfo *take = scene->GetTakeInfo(animStackName);
        FbxTime start = take->mLocalTimeSpan.GetStart();
        FbxTime end = take->mLocalTimeSpan.GetStop();
        FbxLongLong duration = end.GetMilliSeconds() - start.GetMilliSeconds();
        
        pinfo("      Animation [%s] duration %lld ms", animStackName.Buffer(), duration);
        
        keyframeAnimation->set_name(animStackName.Buffer());
        keyframeAnimation->set_duration(duration);
        
        for (FbxLongLong i = start.GetFrameCount(FbxTime::eFrames30); i <= end.GetFrameCount(FbxTime::eFrames30); ++i) {
            viro::Node::KeyframeAnimation::Frame *kf = keyframeAnimation->add_frame();
            
            FbxTime time;
            time.SetFrame(i, FbxTime::eFrames30);
            kf->set_time(fmin(1.0, (float) time.GetMilliSeconds() / (float) duration));
            
            FbxAMatrix localTransform = evaluator->GetNodeLocalTransform(node, time);
            
            FbxVector4 localScale = localTransform.GetS();
            kf->add_scale(localScale.mData[0]);
            kf->add_scale(localScale.mData[1]);
            kf->add_scale(localScale.mData[2]);
            
            //pinfo("Frame %d, scale %f, %f, %f", i, localScale.mData[0], localScale.mData[1], localScale.mData[2]);
            
            FbxVector4 localTranslation = localTransform.GetT();
            kf->add_translation(localTranslation.mData[0]);
            kf->add_translation(localTranslation.mData[1]);
            kf->add_translation(localTranslation.mData[2]);
            
            //pinfo("Frame %d, translation %f, %f, %f", i, localTranslation.mData[0], localTranslation.mData[1], localTranslation.mData[2]);
            
            FbxQuaternion localRotation = localTransform.GetQ();
            kf->add_rotation(localRotation.mData[0]);
            kf->add_rotation(localRotation.mData[1]);
            kf->add_rotation(localRotation.mData[2]);
            kf->add_rotation(localRotation.mData[3]);
        }
    }
}

/*
 This method directly exports the keyframes using the individual animation curves. Unfortunately
 it is not supported by many popular FBX export packages, e.g. Maya.
 */
void VROFBXExporter::exportKeyframeAnimations(FbxScene *scene, FbxNode *node, viro::Node *outNode) {
    /*
     Iterate through each animation stack. Each stack corresponds to a separate animation.
     */
    int numStacks = scene->GetSrcObjectCount(FbxCriteria::ObjectType(FbxAnimStack::ClassId));
    for (int i = 0; i < numStacks; i++) {
        /*
         Get metadata for the animation.
         */
        FbxAnimStack *animStack = scene->GetSrcObject<FbxAnimStack>(i);
        FbxString animStackName = animStack->GetName();
        FbxTakeInfo *take = scene->GetTakeInfo(animStackName);
        FbxTime start = take->mLocalTimeSpan.GetStart();
        FbxTime end = take->mLocalTimeSpan.GetStop();
        FbxLongLong duration = end.GetMilliSeconds() - start.GetMilliSeconds();
        
        pinfo("      Animation duration %lld ms", duration);
        
        const int layerCount = animStack->GetMemberCount<FbxAnimLayer>();
        if (layerCount > 1) {
            pinfo("Blended animations not supported, will not export all animation layers");
        }
        int layerIndex = 0;
        
        scene->SetCurrentAnimationStack(animStack);

        FbxAnimLayer *layer = animStack->GetMember<FbxAnimLayer>(layerIndex);
        
        FbxAnimCurve *translationCurve = node->LclTranslation.GetCurve(layer);
        if (translationCurve != NULL) {
            viro::Node::KeyframeAnimation *keyframeAnimation = outNode->add_keyframe_animation();
            keyframeAnimation->set_name(animStackName.Buffer());
            keyframeAnimation->set_duration(duration);
            
            int keyCount = translationCurve->KeyGetCount();
            for (int keyIndex = 0; keyIndex < keyCount; ++keyIndex) {
                FbxTime keyTime = translationCurve->KeyGetTime(keyIndex);
                FbxVector4 localTranslation = node->EvaluateLocalTranslation(keyTime);
                
                viro::Node::KeyframeAnimation::Frame *kf = keyframeAnimation->add_frame();
                kf->set_time((float) keyTime.GetMilliSeconds() / (float) duration);
                kf->add_translation(localTranslation.mData[0]);
                kf->add_translation(localTranslation.mData[1]);
                kf->add_translation(localTranslation.mData[2]);
                
                pinfo("Added time %f, translation %f, %f, %f", kf->time(), kf->translation(0), kf->translation(1), kf->translation(2));
            }
        }
        
        FbxAnimCurve *rotationCurve = node->LclRotation.GetCurve(layer);
        if (rotationCurve != NULL) {
            viro::Node::KeyframeAnimation *keyframeAnimation = outNode->add_keyframe_animation();
            keyframeAnimation->set_name(animStackName.Buffer());
            keyframeAnimation->set_duration(duration);
            
            int keyCount = rotationCurve->KeyGetCount();
            for (int keyIndex = 0; keyIndex < keyCount; ++keyIndex) {
                FbxTime keyTime = rotationCurve->KeyGetTime(keyIndex);
                
                FbxAMatrix tmp;
                tmp.SetR(node->EvaluateLocalRotation(keyTime));
                FbxQuaternion localRotation = tmp.GetQ();
                
                viro::Node::KeyframeAnimation::Frame *kf = keyframeAnimation->add_frame();
                kf->set_time((float) keyTime.GetMilliSeconds() / (float) duration);
                kf->add_rotation(localRotation.mData[0]);
                kf->add_rotation(localRotation.mData[1]);
                kf->add_rotation(localRotation.mData[2]);
                kf->add_rotation(localRotation.mData[3]);
                
                pinfo("Added time %f, rotation %f, %f, %f", kf->time(), kf->rotation(0), kf->rotation(1), kf->rotation(2));
            }

        }
        
        FbxAnimCurve *scaleCurve = node->LclScaling.GetCurve(layer);
        if (scaleCurve != NULL) {
            viro::Node::KeyframeAnimation *keyframeAnimation = outNode->add_keyframe_animation();
            keyframeAnimation->set_name(animStackName.Buffer());
            keyframeAnimation->set_duration(duration);
            
            int keyCount = scaleCurve->KeyGetCount();
            for (int keyIndex = 0; keyIndex < keyCount; ++keyIndex) {
                FbxTime keyTime = scaleCurve->KeyGetTime(keyIndex);
                FbxVector4 localScale = node->EvaluateLocalScaling(keyTime);
                
                viro::Node::KeyframeAnimation::Frame *kf = keyframeAnimation->add_frame();
                kf->set_time((float) keyTime.GetMilliSeconds() / (float) duration);
                kf->add_scale(localScale.mData[0]);
                kf->add_scale(localScale.mData[1]);
                kf->add_scale(localScale.mData[2]);
                
                pinfo("Added time %f, scale %f, %f, %f", kf->time(), kf->scale(0), kf->scale(1), kf->scale(2));
            }
        }
    }
}

unsigned int VROFBXExporter::findBoneIndexUsingName(const std::string &name, const viro::Node::Skeleton &skeleton) {
    for (unsigned int i = 0; i < skeleton.bone_size(); ++i) {
        if (skeleton.bone(i).name() == name) {
            return i;
        }
    }
    pabort("Skeleton information in FBX file is corrupted");
}

#pragma mark - Export Materials

std::vector<int> VROFBXExporter::readMaterialToMeshMapping(FbxMesh *mesh, int numPolygons) {
    std::vector<int> materialMapping;
    
    if (mesh->GetElementMaterial()) {
        FbxLayerElementArrayTemplate<int> *materialIndices = &(mesh->GetElementMaterial()->GetIndexArray());
        FbxGeometryElement::EMappingMode materialMappingMode = mesh->GetElementMaterial()->GetMappingMode();
        
        if (materialIndices) {
            switch(materialMappingMode) {
                case FbxGeometryElement::eByPolygon: {
                    passert (numPolygons == materialIndices->GetCount());
                    
                    for (unsigned int i = 0; i < materialIndices->GetCount(); ++i) {
                        unsigned int materialIndex = materialIndices->GetAt(i);
                        materialMapping.push_back(materialIndex);
                    }
                }
                    break;
                    
                case FbxGeometryElement::eAllSame: {
                    unsigned int materialIndex = materialIndices->GetAt(0);
                    for (unsigned int i = 0; i < numPolygons; ++i) {
                        materialMapping.push_back(materialIndex);
                    }
                }
                    break;
                    
                default:
                    pabort("Invalid mapping mode for material");
            }
        }
        else {
            pabort("No material indices found for mesh");
        }
    }
    else {
        pinfo("No material element found for mesh; mapping all polygons to material 0");
        for (unsigned int i = 0; i < numPolygons; ++i) {
            materialMapping.push_back(0);
        }
    }
    
    return materialMapping;
}

void VROFBXExporter::exportMaterial(FbxSurfaceMaterial *inMaterial, bool compressTextures, viro::Node::Geometry::Material *outMaterial) {
    // Check if we're using a hardware material
    const FbxImplementation *implementation = GetImplementation(inMaterial, FBXSDK_IMPLEMENTATION_HLSL);
    if (!implementation) {
        implementation = GetImplementation(inMaterial, FBXSDK_IMPLEMENTATION_CGFX);
    }
    
    if (implementation) {
        exportHardwareMaterial(inMaterial, implementation, outMaterial);
        return;
    }
    
    // Otherwise it's either Phong or Lambert
    if (inMaterial->GetClassId().Is(FbxSurfacePhong::ClassId)) {
        pinfo("      Phong material");
        FbxSurfacePhong *phong = reinterpret_cast<FbxSurfacePhong *>(inMaterial);
        outMaterial->set_lighting_model(viro::Node_Geometry_Material_LightingModel_Phong);
        
        // Diffuse Color
        viro::Node::Geometry::Material::Visual *diffuse = outMaterial->mutable_diffuse();
        
        FbxDouble3 inDiffuse = reinterpret_cast<FbxSurfacePhong *>(inMaterial)->Diffuse;
        diffuse->add_color(static_cast<float>(inDiffuse.mData[0]));
        diffuse->add_color(static_cast<float>(inDiffuse.mData[1]));
        diffuse->add_color(static_cast<float>(inDiffuse.mData[2]));
        diffuse->set_intensity(phong->DiffuseFactor);
        
        // Specular Color
        // double3 = reinterpret_cast<FbxSurfacePhong *>(inMaterial)->Specular;
        
        // Emissive Color
        // double3 = reinterpret_cast<FbxSurfacePhong *>(inMaterial)->Emissive;
        
        // Reflection
        // double3 = reinterpret_cast<FbxSurfacePhong *>(inMaterial)->Reflection;
        
        // Transparency
        if (phong->TransparencyFactor.IsValid() && phong->TransparentColor.IsValid()) {
            FbxDouble transparencyFactor = reinterpret_cast<FbxSurfaceLambert *>(inMaterial)->TransparencyFactor.Get();
            FbxDouble3 transparencyColor = reinterpret_cast<FbxSurfaceLambert *>(inMaterial)->TransparentColor.Get();
            float transparency = (transparencyColor[0] * transparencyFactor +
                                  transparencyColor[1] * transparencyFactor +
                                  transparencyColor[2] * transparencyFactor) / 3.0;
            outMaterial->set_transparency(1.0 - transparency);
        }
        else if (phong->TransparencyFactor.IsValid()) {
            outMaterial->set_transparency(1.0 - phong->TransparencyFactor.Get());
        }
        else if (phong->TransparentColor.IsValid()) {
            FbxDouble3 color = phong->TransparentColor.Get();
            outMaterial->set_transparency(1.0 - (float)((color[0] + color[1] + color[2]) / 3.0));
        }
        
        pinfo("         Opacity set to %f", outMaterial->transparency());
        
        // Shininess
        FbxDouble shininess = reinterpret_cast<FbxSurfacePhong *>(inMaterial)->Shininess;
        outMaterial->set_shininess(shininess);
        
        // Specular Factor
        // double1 = reinterpret_cast<FbxSurfacePhong *>(inMaterial)->SpecularFactor;
        
        // Reflection Factor
        FbxDouble reflectionFactor = reinterpret_cast<FbxSurfacePhong *>(inMaterial)->ReflectionFactor;
        outMaterial->set_fresnel_exponent(reflectionFactor);
    }
    else if (inMaterial->GetClassId().Is(FbxSurfaceLambert::ClassId)) {
        pinfo("      Lambert material");
        FbxSurfaceLambert *lambert = reinterpret_cast<FbxSurfaceLambert *>(inMaterial);
        outMaterial->set_lighting_model(viro::Node_Geometry_Material_LightingModel_Lambert);
        
        // Diffuse Color
        viro::Node::Geometry::Material::Visual *diffuse = outMaterial->mutable_diffuse();
        
        FbxDouble3 inDiffuse = lambert->Diffuse;
        diffuse->add_color(static_cast<float>(inDiffuse.mData[0]));
        diffuse->add_color(static_cast<float>(inDiffuse.mData[1]));
        diffuse->add_color(static_cast<float>(inDiffuse.mData[2]));
        diffuse->set_intensity(lambert->DiffuseFactor);
      
        // Emissive Color
        // double3 = reinterpret_cast<FbxSurfaceLambert *>(inMaterial)->Emissive;
        // set emissive intensity here as well
        
        // Transparency
        if (lambert->TransparencyFactor.IsValid() && lambert->TransparentColor.IsValid()) {
            FbxDouble transparencyFactor = reinterpret_cast<FbxSurfaceLambert *>(inMaterial)->TransparencyFactor.Get();
            FbxDouble3 transparencyColor = reinterpret_cast<FbxSurfaceLambert *>(inMaterial)->TransparentColor.Get();
            float transparency = (transparencyColor[0] * transparencyFactor +
                                  transparencyColor[1] * transparencyFactor +
                                  transparencyColor[2] * transparencyFactor) / 3.0;
            outMaterial->set_transparency(1.0 - transparency);
        }
        else if (lambert->TransparencyFactor.IsValid()) {
            outMaterial->set_transparency(1.0 - lambert->TransparencyFactor.Get());
        }
        else if (lambert->TransparentColor.IsValid()) {
            FbxDouble3 color = lambert->TransparentColor.Get();
            outMaterial->set_transparency(1.0 - (float)((color[0] + color[1] + color[2]) / 3.0));
        }
        
        pinfo("         Opacity set to %f", outMaterial->transparency());
    }
    
    unsigned int textureIndex = 0;
    FBXSDK_FOR_EACH_TEXTURE(textureIndex) {
        FbxProperty property = inMaterial->FindProperty(FbxLayerElement::sTextureChannelNames[textureIndex]);
        if (property.IsValid()) {
            unsigned int textureCount = property.GetSrcObjectCount<FbxTexture>();
            for (unsigned int i = 0; i < textureCount; ++i) {
                FbxLayeredTexture *layeredTexture = property.GetSrcObject<FbxLayeredTexture>(i);
                if (layeredTexture) {
                    pabort("Layered Texture is currently unsupported\n");
                }
                else {
                    FbxTexture *texture = property.GetSrcObject<FbxTexture>(i);
                    if (texture) {
                        std::string textureType = property.GetNameAsCStr();
                        FbxFileTexture *fileTexture = FbxCast<FbxFileTexture>(texture);
                        
                        if (fileTexture) {
                            std::string textureName = extractTextureName(fileTexture);
                            pinfo("         Texture index %d, texture type [%s], texture name [%s]", textureIndex, textureType.c_str(), textureName.c_str());
                            
                            if (textureType == "DiffuseColor") {
                                if (compressTextures && compressTexture(textureName)) {
                                    textureName = getFileName(textureName) + ".ktx";
                                    pinfo("         Texture successfully compressed, renamed to %s", textureName.c_str());
                                }
                                outMaterial->mutable_diffuse()->set_texture(textureName);
                                outMaterial->mutable_diffuse()->set_wrap_mode_s(convert(texture->GetWrapModeU()));
                                outMaterial->mutable_diffuse()->set_wrap_mode_t(convert(texture->GetWrapModeV()));
                                outMaterial->mutable_diffuse()->set_minification_filter(viro::Node_Geometry_Material_Visual_FilterMode_Linear);
                                outMaterial->mutable_diffuse()->set_magnification_filter(viro::Node_Geometry_Material_Visual_FilterMode_Linear);
                                outMaterial->mutable_diffuse()->set_mip_filter(viro::Node_Geometry_Material_Visual_FilterMode_Linear);
                            }
                            else if (textureType == "SpecularColor") {
                                if (compressTextures && compressTexture(textureName)) {
                                    textureName = getFileName(textureName) + ".ktx";
                                    pinfo("         Texture successfully compressed, renamed to %s", textureName.c_str());
                                }
                                outMaterial->mutable_specular()->set_texture(textureName);
                                outMaterial->mutable_specular()->set_wrap_mode_s(convert(texture->GetWrapModeU()));
                                outMaterial->mutable_specular()->set_wrap_mode_t(convert(texture->GetWrapModeV()));
                                outMaterial->mutable_specular()->set_minification_filter(viro::Node_Geometry_Material_Visual_FilterMode_Linear);
                                outMaterial->mutable_specular()->set_magnification_filter(viro::Node_Geometry_Material_Visual_FilterMode_Linear);
                                outMaterial->mutable_specular()->set_mip_filter(viro::Node_Geometry_Material_Visual_FilterMode_Linear);
                            }
                            else if (textureType == "Bump") {
                                outMaterial->mutable_normal()->set_texture(textureName);
                                outMaterial->mutable_normal()->set_wrap_mode_s(convert(texture->GetWrapModeU()));
                                outMaterial->mutable_normal()->set_wrap_mode_t(convert(texture->GetWrapModeV()));
                                outMaterial->mutable_normal()->set_minification_filter(viro::Node_Geometry_Material_Visual_FilterMode_Linear);
                                outMaterial->mutable_normal()->set_magnification_filter(viro::Node_Geometry_Material_Visual_FilterMode_Linear);
                                outMaterial->mutable_normal()->set_mip_filter(viro::Node_Geometry_Material_Visual_FilterMode_Linear);
                            }
                        }
                    }
                }
            }
        }
    }
    
    if (outMaterial->has_specular()) {
        outMaterial->mutable_specular()->set_intensity(1.0);
    }
    if (outMaterial->has_normal()) {
        outMaterial->mutable_normal()->set_intensity(1.0);
    }
}

void VROFBXExporter::exportHardwareMaterial(FbxSurfaceMaterial *inMaterial, const FbxImplementation *implementation,
                                            viro::Node::Geometry::Material *outMaterial) {
    
    // TODO This is a work in progress; there are hundreds of potential parameters and it
    //      isn't clear yet if hardware materials are widely used, and if the parameter
    //      names are stable.
    outMaterial->set_transparency(1.0);
    outMaterial->set_shininess(2.0);
    outMaterial->set_lighting_model(viro::Node_Geometry_Material_LightingModel_Phong);
    
    const FbxBindingTable *rootTable = implementation->GetRootTable();
    FbxString fileName = rootTable->DescAbsoluteURL.Get();
    FbxString techniqueName = rootTable->DescTAG.Get();
    
    const FbxBindingTable *table = implementation->GetRootTable();
    size_t numEntries = table->GetEntryCount();
    
    for (int i = 0; i < (int)numEntries; ++i) {
        const FbxBindingTableEntry &entry = table->GetEntry(i);
        const char* entrySrcType = entry.GetEntryType(true);
        
        FbxProperty fbxProp;
        FbxString entrySource = entry.GetSource();
        std::string entryText = std::string(entrySource.Buffer());
        // pinfo("            Entry: %s", entryText.c_str());
        
        if (strcmp(FbxPropertyEntryView::sEntryType, entrySrcType) == 0) {
            fbxProp = inMaterial->FindPropertyHierarchical(entry.GetSource());
            if(!fbxProp.IsValid()) {
                fbxProp = inMaterial->RootProperty.FindHierarchical(entry.GetSource());
            }
        }
        else if(strcmp(FbxConstantEntryView::sEntryType, entrySrcType) == 0) {
            fbxProp = implementation->GetConstants().FindHierarchical(entry.GetSource());
        }
        
        if (fbxProp.IsValid()) {
            if (fbxProp.GetSrcObjectCount<FbxTexture>() > 0) {
                for(int j = 0; j < fbxProp.GetSrcObjectCount<FbxFileTexture>(); ++j) {
                    FbxFileTexture *fileTexture = fbxProp.GetSrcObject<FbxFileTexture>(j);
                    std::string textureName = extractTextureName(fileTexture);
                    pinfo("         Texture [%s] found for entry [%s]", textureName.c_str(), entryText.c_str());
                    
                    if (endsWith(entryText, "DiffuseTexture")) {
                        outMaterial->mutable_diffuse()->set_texture(textureName);
                        outMaterial->mutable_diffuse()->set_wrap_mode_s(viro::Node_Geometry_Material_Visual_WrapMode_Clamp);
                        outMaterial->mutable_diffuse()->set_wrap_mode_t(viro::Node_Geometry_Material_Visual_WrapMode_Clamp);
                        outMaterial->mutable_diffuse()->set_minification_filter(viro::Node_Geometry_Material_Visual_FilterMode_Linear);
                        outMaterial->mutable_diffuse()->set_magnification_filter(viro::Node_Geometry_Material_Visual_FilterMode_Linear);
                        outMaterial->mutable_diffuse()->set_mip_filter(viro::Node_Geometry_Material_Visual_FilterMode_Linear);
                    }
                    else if (endsWith(entryText, "SpecularTexture")) {
                        outMaterial->mutable_specular()->set_texture(textureName);
                        outMaterial->mutable_specular()->set_wrap_mode_s(viro::Node_Geometry_Material_Visual_WrapMode_Clamp);
                        outMaterial->mutable_specular()->set_wrap_mode_t(viro::Node_Geometry_Material_Visual_WrapMode_Clamp);
                        outMaterial->mutable_specular()->set_minification_filter(viro::Node_Geometry_Material_Visual_FilterMode_Linear);
                        outMaterial->mutable_specular()->set_magnification_filter(viro::Node_Geometry_Material_Visual_FilterMode_Linear);
                        outMaterial->mutable_specular()->set_mip_filter(viro::Node_Geometry_Material_Visual_FilterMode_Linear);
                    }
                    else if (endsWith(entryText, "NormalTexture")) {
                        outMaterial->mutable_normal()->set_texture(textureName);
                        outMaterial->mutable_normal()->set_wrap_mode_s(viro::Node_Geometry_Material_Visual_WrapMode_Clamp);
                        outMaterial->mutable_normal()->set_wrap_mode_t(viro::Node_Geometry_Material_Visual_WrapMode_Clamp);
                        outMaterial->mutable_normal()->set_minification_filter(viro::Node_Geometry_Material_Visual_FilterMode_Linear);
                        outMaterial->mutable_normal()->set_magnification_filter(viro::Node_Geometry_Material_Visual_FilterMode_Linear);
                        outMaterial->mutable_normal()->set_mip_filter(viro::Node_Geometry_Material_Visual_FilterMode_Linear);
                    }
                }
                for(int j = 0; j < fbxProp.GetSrcObjectCount<FbxLayeredTexture>(); ++j) {
                    pinfo("Layered texture found: not supported");
                }
                for(int j = 0; j < fbxProp.GetSrcObjectCount<FbxProceduralTexture>(); ++j) {
                    pinfo("Procedural texture found: not supported");
                }
            }
            else {
                // TODO Support properties (see printGeometry method for detail)
            }
        }
    }
    
    if (outMaterial->has_diffuse()) {
        outMaterial->mutable_diffuse()->set_intensity(1.0);
    }
    if (outMaterial->has_specular()) {
        outMaterial->mutable_specular()->set_intensity(1.0);
    }
    if (outMaterial->has_normal()) {
        outMaterial->mutable_normal()->set_intensity(1.0);
    }
}

viro::Node_Geometry_Material_Visual_WrapMode VROFBXExporter::convert(FbxTexture::EWrapMode wrapMode) {
    switch (wrapMode) {
        case FbxTexture::eRepeat:
            return viro::Node_Geometry_Material_Visual_WrapMode_Repeat;
            
        default:
            return viro::Node_Geometry_Material_Visual_WrapMode_Clamp;
    }
}

bool VROFBXExporter::compressTexture(std::string textureName) {
    // Check if the texture exists in the .fbm folder that the importer
    // automatically creates when we export an FBX with embedded textures
    std::string fbmFolder = getFileName(_fbxPath) + ".fbm";
    std::string texturePath = fbmFolder + "/" + textureName;
    std::ifstream f(texturePath.c_str());
    if (!f.good()) {
        pinfo("Texture at path %s does not exist, will not compress", texturePath.c_str());
        return false;
    }
    
    // Currently we only support compressing PNG files
    std::string name = getFileName(textureName);
    std::string extension = getFileExtension(textureName);
    if (extension != "png") {
        pinfo("Texture %s is not a PNG, will not compress", texturePath.c_str());
        return false;
    }
    
    std::string cmd = "./EtcTool " + texturePath + " -format RGBA8 -mipmaps 16 -output " + fbmFolder + "/" + name + ".ktx";
    //std::string cmd = "/Users/radvani/Source/react-viro/bin/EtcTool " + texturePath + " -format RGBA8 -mipmaps 16 -output " + fbmFolder + "/" + name + ".ktx";

    pinfo("Running texture compression command [%s]", cmd.c_str());
    
    std::array<char, 128> buffer;
    std::string result;
    std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        pinfo("Failed to run texture compression command, will not compress texture");
        return false;
    }
    
    while (!feof(pipe.get())) {
        if (fgets(buffer.data(), 128, pipe.get()) != nullptr) {
            result += buffer.data();
        }
    }
    pinfo("%s", result.c_str());
    return true;
}

#pragma mark - Printing (Debug)

void VROFBXExporter::debugPrint(std::string fbxPath) {
    _numTabs = 0;
    FbxScene *scene = loadFBX(fbxPath);
    
    // Print the nodes of the scene and their attributes recursively.
    // Note that we are not printing the root node because it should
    // not contain any attributes.
    FbxNode *rootNode = scene->GetRootNode();
    if (rootNode) {
        for (int i = 0; i < rootNode->GetChildCount(); i++) {
            printNode(rootNode->GetChild(i));
        }
    }
}

void VROFBXExporter::printTabs() {
    for(int i = 0; i < _numTabs; i++)
        printf("\t");
}

void VROFBXExporter::printAttribute(FbxNodeAttribute *pAttribute) {
    if(!pAttribute) return;
    
    FbxString typeName = GetAttributeTypeName(pAttribute->GetAttributeType());
    FbxString attrName = pAttribute->GetName();
    printTabs();
    // Note: to retrieve the character array of a FbxString, use its Buffer() method.
    printf("<attribute type='%s' name='%s'/>\n", typeName.Buffer(), attrName.Buffer());
}

void VROFBXExporter::printNode(FbxNode *pNode) {
    printTabs();
    const char* nodeName = pNode->GetName();
    FbxDouble3 translation = pNode->LclTranslation.Get();
    FbxDouble3 rotation = pNode->LclRotation.Get();
    FbxDouble3 scaling = pNode->LclScaling.Get();
    
    // Print the contents of the node.
    printf("<node name='%s' translation='(%f, %f, %f)' rotation='(%f, %f, %f)' scaling='(%f, %f, %f)'>\n",
           nodeName,
           translation[0], translation[1], translation[2],
           rotation[0], rotation[1], rotation[2],
           scaling[0], scaling[1], scaling[2]
           );
    _numTabs++;
    
    // Print the node's attributes.
    for (int i = 0; i < pNode->GetNodeAttributeCount(); i++) {
        printAttribute(pNode->GetNodeAttributeByIndex(i));
    }
    
    // Print the node's geometry
    if (pNode->GetGeometry() != nullptr) {
        printGeometry(pNode->GetGeometry());
    }
    
    // Recursively print the children.
    for (int j = 0; j < pNode->GetChildCount(); j++) {
        printNode(pNode->GetChild(j));
    }
    
    _numTabs--;
    printTabs();
    printf("</node>\n");
}

void VROFBXExporter::printGeometry(FbxGeometry *pGeometry) {
    int lMaterialCount = 0;
    FbxNode* lNode = NULL;
    if(pGeometry){
        lNode = pGeometry->GetNode();
        if(lNode)
            lMaterialCount = lNode->GetMaterialCount();
    }
    
    if (lMaterialCount > 0) {
        FbxPropertyT<FbxDouble3> lKFbxDouble3;
        FbxPropertyT<FbxDouble> lKFbxDouble1;
        FbxColor theColor;
        
        for (int lCount = 0; lCount < lMaterialCount; lCount ++)
        {
            pinfo("        Material %d", lCount);
            
            FbxSurfaceMaterial *lMaterial = lNode->GetMaterial(lCount);
            
            pinfo("            Name: %s", (char *) lMaterial->GetName());
            
            //Get the implementation to see if it's a hardware shader.
            const FbxImplementation* lImplementation = GetImplementation(lMaterial, FBXSDK_IMPLEMENTATION_HLSL);
            FbxString lImplemenationType = "HLSL";
            if(!lImplementation) {
                lImplementation = GetImplementation(lMaterial, FBXSDK_IMPLEMENTATION_CGFX);
                lImplemenationType = "CGFX";
            }
            if(lImplementation) {
                //Now we have a hardware shader, let's read it
                pinfo("            Hardware Shader Type: %s", lImplemenationType.Buffer());
                const FbxBindingTable* lRootTable = lImplementation->GetRootTable();
                FbxString lFileName = lRootTable->DescAbsoluteURL.Get();
                FbxString lTechniqueName = lRootTable->DescTAG.Get();
                
                
                const FbxBindingTable* lTable = lImplementation->GetRootTable();
                size_t lEntryNum = lTable->GetEntryCount();
                
                for(int i=0;i <(int)lEntryNum; ++i)
                {
                    const FbxBindingTableEntry& lEntry = lTable->GetEntry(i);
                    const char* lEntrySrcType = lEntry.GetEntryType(true);
                    FbxProperty lFbxProp;
                    
                    
                    FbxString lTest = lEntry.GetSource();
                    pinfo("            Entry: %s", lTest.Buffer());
                    
                    
                    if ( strcmp( FbxPropertyEntryView::sEntryType, lEntrySrcType ) == 0 )
                    {
                        lFbxProp = lMaterial->FindPropertyHierarchical(lEntry.GetSource());
                        if(!lFbxProp.IsValid())
                        {
                            lFbxProp = lMaterial->RootProperty.FindHierarchical(lEntry.GetSource());
                        }
                        
                        
                    }
                    else if( strcmp( FbxConstantEntryView::sEntryType, lEntrySrcType ) == 0 )
                    {
                        lFbxProp = lImplementation->GetConstants().FindHierarchical(lEntry.GetSource());
                    }
                    if(lFbxProp.IsValid())
                    {
                        if( lFbxProp.GetSrcObjectCount<FbxTexture>() > 0 )
                        {
                            //do what you want with the textures
                            for(int j=0; j<lFbxProp.GetSrcObjectCount<FbxFileTexture>(); ++j)
                            {
                                FbxFileTexture *lTex = lFbxProp.GetSrcObject<FbxFileTexture>(j);
                                FBXSDK_printf("           File Texture: %s\n", lTex->GetFileName());
                            }
                            for(int j=0; j<lFbxProp.GetSrcObjectCount<FbxLayeredTexture>(); ++j)
                            {
                                FbxLayeredTexture *lTex = lFbxProp.GetSrcObject<FbxLayeredTexture>(j);
                                FBXSDK_printf("        Layered Texture: %s\n", lTex->GetName());
                            }
                            for(int j=0; j<lFbxProp.GetSrcObjectCount<FbxProceduralTexture>(); ++j)
                            {
                                FbxProceduralTexture *lTex = lFbxProp.GetSrcObject<FbxProceduralTexture>(j);
                                FBXSDK_printf("     Procedural Texture: %s\n", lTex->GetName());
                            }
                        }
                        else
                        {
                            FbxDataType lFbxType = lFbxProp.GetPropertyDataType();
                            FbxString blah = lFbxType.GetName();
                            if(FbxBoolDT == lFbxType)
                            {
                                pinfo("                Bool: %d", lFbxProp.Get<FbxBool>() );
                            }
                            else if ( FbxIntDT == lFbxType ||  FbxEnumDT  == lFbxType )
                            {
                                pinfo("                Int: %d", lFbxProp.Get<FbxInt>());
                            }
                            else if ( FbxFloatDT == lFbxType)
                            {
                                pinfo("                Float: %f", lFbxProp.Get<FbxFloat>());
                                
                            }
                            else if ( FbxDoubleDT == lFbxType)
                            {
                                pinfo("                Double: %f", lFbxProp.Get<FbxDouble>());
                            }
                            else if ( FbxStringDT == lFbxType
                                     ||  FbxUrlDT  == lFbxType
                                     ||  FbxXRefUrlDT  == lFbxType )
                            {
                                pinfo("                String: %s", lFbxProp.Get<FbxString>().Buffer());
                            }
                            else if ( FbxDouble2DT == lFbxType)
                            {
                                FbxDouble2 lDouble2 = lFbxProp.Get<FbxDouble2>();
                                FbxVector2 lVect;
                                lVect[0] = lDouble2[0];
                                lVect[1] = lDouble2[1];
                                
                                pinfo("                2D vector %f, %f: ", lVect[0], lVect[1]);
                            }
                            else if ( FbxDouble3DT == lFbxType || FbxColor3DT == lFbxType)
                            {
                                FbxDouble3 lDouble3 = lFbxProp.Get<FbxDouble3>();
                                
                                
                                FbxVector4 lVect;
                                lVect[0] = lDouble3[0];
                                lVect[1] = lDouble3[1];
                                lVect[2] = lDouble3[2];
                                pinfo("                3D vector: %f, %f, %f", lVect[0], lVect[1], lVect[2]);
                            }
                            
                            else if ( FbxDouble4DT == lFbxType || FbxColor4DT == lFbxType)
                            {
                                FbxDouble4 lDouble4 = lFbxProp.Get<FbxDouble4>();
                                FbxVector4 lVect;
                                lVect[0] = lDouble4[0];
                                lVect[1] = lDouble4[1];
                                lVect[2] = lDouble4[2];
                                lVect[3] = lDouble4[3];
                                pinfo("                4D vector: %f, %f, %f, %f ", lVect[0], lVect[1], lVect[2], lVect[3]);
                            }
                            else if ( FbxDouble4x4DT == lFbxType)
                            {
                                FbxDouble4x4 lDouble44 = lFbxProp.Get<FbxDouble4x4>();
                                for(int j=0; j<4; ++j)
                                {
                                    
                                    FbxVector4 lVect;
                                    lVect[0] = lDouble44[j][0];
                                    lVect[1] = lDouble44[j][1];
                                    lVect[2] = lDouble44[j][2];
                                    lVect[3] = lDouble44[j][3];
                                    pinfo("                4x4D vector: %f, %f, %f, %f", lVect[0], lVect[1], lVect[2], lVect[3]);
                                }
                                
                            }
                        }
                        
                    }
                }
            }
            else if (lMaterial->GetClassId().Is(FbxSurfacePhong::ClassId))
            {
                // We found a Phong material.  Display its properties.
                
                // Display the Ambient Color
                lKFbxDouble3 =((FbxSurfacePhong *) lMaterial)->Ambient;
                theColor.Set(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);
                pinfo("            Ambient: %f, %f, %f", theColor.mRed, theColor.mGreen, theColor.mBlue);
                
                // Display the Diffuse Color
                lKFbxDouble3 =((FbxSurfacePhong *) lMaterial)->Diffuse;
                theColor.Set(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);
                pinfo("            Diffuse: %f, %f, %f", theColor.mRed, theColor.mGreen, theColor.mBlue);
                
                // Display the Specular Color (unique to Phong materials)
                lKFbxDouble3 =((FbxSurfacePhong *) lMaterial)->Specular;
                theColor.Set(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);
                pinfo("            Specular: %f, %f, %f", theColor.mRed, theColor.mGreen, theColor.mBlue);
                
                // Display the Emissive Color
                lKFbxDouble3 =((FbxSurfacePhong *) lMaterial)->Emissive;
                theColor.Set(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);
                pinfo("            Emissive: %f, %f, %f", theColor.mRed, theColor.mGreen, theColor.mBlue);
                
                //Opacity is Transparency factor now
                lKFbxDouble1 =((FbxSurfacePhong *) lMaterial)->TransparencyFactor;
                pinfo("            Transparency: %f", lKFbxDouble1.Get());
                
                // Display the Shininess
                lKFbxDouble1 =((FbxSurfacePhong *) lMaterial)->Shininess;
                pinfo("            Shininess: %f", lKFbxDouble1.Get());
                
                // Display the Reflectivity
                lKFbxDouble1 =((FbxSurfacePhong *) lMaterial)->ReflectionFactor;
                pinfo("            Reflectivity: %f", lKFbxDouble1.Get());
            }
            else if(lMaterial->GetClassId().Is(FbxSurfaceLambert::ClassId) )
            {
                // We found a Lambert material. Display its properties.
                // Display the Ambient Color
                lKFbxDouble3=((FbxSurfaceLambert *)lMaterial)->Ambient;
                theColor.Set(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);
                pinfo("            Ambient: %f, %f, %f", theColor.mRed, theColor.mGreen, theColor.mBlue);
                
                // Display the Diffuse Color
                lKFbxDouble3 =((FbxSurfaceLambert *)lMaterial)->Diffuse;
                theColor.Set(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);
                pinfo("            Diffuse: %f, %f, %f", theColor.mRed, theColor.mGreen, theColor.mBlue);
                
                // Display the Emissive
                lKFbxDouble3 =((FbxSurfaceLambert *)lMaterial)->Emissive;
                theColor.Set(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);
                pinfo("            Emissive: %f, %f, %f", theColor.mRed, theColor.mGreen, theColor.mBlue);
                
                // Display the Opacity
                lKFbxDouble1 =((FbxSurfaceLambert *)lMaterial)->TransparencyFactor;
                pinfo("            Transparency: %f", lKFbxDouble1.Get());
            }
            else {
                pinfo("Unknown type of Material");
            }
            
            FbxPropertyT<FbxString> lString;
            lString = lMaterial->ShadingModel;
            pinfo("            Shading Model: %s", lString.Get().Buffer());
            pinfo("");
        }
    }
}

std::string VROFBXExporter::extractTextureName(FbxFileTexture *texture) {
    std::string fileName = std::string(texture->GetFileName());
    std::string textureName = fileName;
    
    // First check '/' path separator
    size_t lastPathComponent = fileName.find_last_of("/");
    if (lastPathComponent != std::string::npos) {
        textureName = fileName.substr(lastPathComponent + 1, fileName.size() - lastPathComponent - 1);
    }
    // Otherwise try '\\' path separator
    else {
        lastPathComponent = fileName.find_last_of("\\");
        if (lastPathComponent != std::string::npos) {
            textureName = fileName.substr(lastPathComponent + 1, fileName.size() - lastPathComponent - 1);
        }
    }
    
    return textureName;
}

std::string VROFBXExporter::getFileName(std::string file) {
    std::string::size_type index = file.rfind('.');
    if (index != std::string::npos) {
        return file.substr(0, index);
    }
    else {
        return file;
    }
}

std::string VROFBXExporter::getFileExtension(std::string file) {
    std::string::size_type index = file.rfind('.');
    if (index != std::string::npos) {
        return file.substr(index + 1);
    }
    else {
        return "";
    }
}

bool VROFBXExporter::endsWith(const std::string& candidate, const std::string& ending) {
    if (candidate.length() < ending.length()) {
        return false;
    }
    return 0 == candidate.compare(candidate.length() - ending.length(), ending.length(), ending);
}


