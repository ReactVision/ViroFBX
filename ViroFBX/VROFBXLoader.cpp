//
//  VROFBXLoader.cpp
//  ViroFBX
//
//  Created by Raj Advani on 4/25/17.
//  Copyright © 2017 Viro. All rights reserved.
//

#include "VROFBXLoader.h"
#include "VROLog.h"
#include <fstream>

static const bool kDebugGeometrySource = false;

#pragma mark - Initialization

VROFBXLoader::VROFBXLoader(std::string fbxPath) : _fbxPath(fbxPath) {
    
}

VROFBXLoader::~VROFBXLoader() {

}

FbxScene *VROFBXLoader::loadFBX(FbxManager *sdkManager) {
    FbxIOSettings *ios = FbxIOSettings::Create(sdkManager, IOSROOT);
    sdkManager->SetIOSettings(ios);
    
    FbxImporter *importer = FbxImporter::Create(sdkManager, "");
    
    pinfo("Loading file [%s]", _fbxPath.c_str());
    bool importStatus = importer->Initialize(_fbxPath.c_str());
    
    if (!importStatus) {
        pinfo("Call to FBXImporter::Initialize() failed");
        pinfo("Error returned: %s", importer->GetStatus().GetErrorString());
    }
    else {
        pinfo("Import successful");
    }
    
    FbxScene *scene = FbxScene::Create(sdkManager, "scene");
    importer->Import(scene);
    importer->Destroy();
    
    return scene;
}

#pragma mark - Export

void VROFBXLoader::exportFBX(std::string destPath) {
    FbxManager *sdkManager = FbxManager::Create();
    FbxScene *scene = loadFBX(sdkManager);
    
    viro::Node *outNode = new viro::Node();
    
    pinfo("Reading FBX...");
    
    // TODO The root node has no data. Currently we export only the first child
    //      of the root. Instead, export an empty root node and all children
    FbxNode *rootNode = scene->GetRootNode();
    if (rootNode) {
        for (int i = 0; i < rootNode->GetChildCount(); i++) {
            exportNode(rootNode->GetChild(i), outNode);
            break;
        }
    }
    
    int byteSize = outNode->ByteSize();
    
    pinfo("Writing protobuf [%d bytes]...", byteSize);
    std::string encoded = outNode->SerializeAsString();
    std::ofstream(destPath.c_str(), std::ios::binary).write(encoded.c_str(), encoded.size());
    
    sdkManager->Destroy();
    pinfo("Export complete");
}

void VROFBXLoader::exportNode(FbxNode *node, viro::Node *outNode) {
    FbxDouble3 translation = node->LclTranslation.Get();
    outNode->add_position(translation[0]);
    outNode->add_position(translation[1]);
    outNode->add_position(translation[2]);
    
    FbxDouble3 scaling = node->LclScaling.Get();
    outNode->add_scale(scaling[0]);
    outNode->add_scale(scaling[1]);
    outNode->add_scale(scaling[2]);
    
    FbxDouble3 rotation = node->LclRotation.Get();
    outNode->add_rotation(rotation[0]);
    outNode->add_rotation(rotation[1]);
    outNode->add_rotation(rotation[2]);
    
    outNode->set_rendering_order(0);
    outNode->set_opacity(1.0);
    
    viro::Node::Geometry *geometry = new viro::Node::Geometry();
    exportGeometry(node, geometry);
    outNode->set_allocated_geometry(geometry);
}

void VROFBXLoader::exportGeometry(FbxNode *node, viro::Node::Geometry *geo) {
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
    
    pinfo("UV set name %s", uvSetName);
    
    /*
     Export the vertex, normal, tex-coord, and tangent data (the geometry
     sources).
     */
    viro::Node::Geometry::Source *vertices = geo->add_source();
    vertices->set_semantic(viro::Node_Geometry_Source_Semantic_Vertex);
    
    std::vector<float> vertexData;
    
    viro::Node::Geometry::Source *normals = geo->add_source();
    normals->set_semantic(viro::Node_Geometry_Source_Semantic_Normal);
    
    std::vector<float> normalData;
    
    viro::Node::Geometry::Source *texCoords = geo->add_source();
    texCoords->set_semantic(viro::Node_Geometry_Source_Semantic_Texcoord);
    
    std::vector<float> texCoordData;
    
    viro::Node::Geometry::Source *tangents = geo->add_source();
    tangents->set_semantic(viro::Node_Geometry_Source_Semantic_Tangent);
    
    std::vector<float> tangentData;
    
    int numPolygons = mesh->GetPolygonCount();
    pinfo("Polygon count %d", numPolygons);

    for (unsigned int i = 0; i < numPolygons; i++) {
        if (kDebugGeometrySource) {
            pinfo("Reading polygon %d", i);
        }
        int polygonSize = mesh->GetPolygonSize(i);
        
        // We only support triangles
        if (polygonSize != 3) {
            if (kDebugGeometrySource) {
                pinfo("   Encountered non-triangle polygon (size %d)", polygonSize);
            }
        }
        
        // TODO remove, and support polygonSize 4, abort on <3 or >4
        polygonSize = 3;
        
        for (int j = 0; j < polygonSize; j++) {
            if (kDebugGeometrySource) {
                pinfo("   V%d", j);
            }
            int controlPointIndex = mesh->GetPolygonVertex(i, j);
            
            FbxVector4 vertex = mesh->GetControlPointAt(controlPointIndex);
            vertexData.push_back(vertex.mData[0]);
            vertexData.push_back(vertex.mData[1]);
            vertexData.push_back(vertex.mData[2]);
            
            if (kDebugGeometrySource) {
                pinfo("      Read vertex %f, %f, %f", vertex.mData[0], vertex.mData[1], vertex.mData[2]);
            }
            
            FbxVector4 normal;
            bool hasNormal = mesh->GetPolygonVertexNormal(i, j, normal);
            
            if (kDebugGeometrySource) {
                pinfo("      Read normal %f, %f, %f", normal.mData[0], normal.mData[1], normal.mData[2]);
            }

            if (hasNormal) {
                normalData.push_back(normal.mData[0]);
                normalData.push_back(normal.mData[1]);
                normalData.push_back(normal.mData[2]);
            }
            else {
                normalData.push_back(0);
                normalData.push_back(0);
                normalData.push_back(0);
            }
            
            FbxVector2 uv;
            bool unmapped;
            bool hasUV = mesh->GetPolygonVertexUV(i, j, uvSetName, uv, unmapped);
            
            if (kDebugGeometrySource) {
                pinfo("      Read UV %f, %f", uv.mData[0], uv.mData[1]);
            }
            
            if (hasUV && !unmapped) {
                texCoordData.push_back(uv.mData[0]);
                texCoordData.push_back(uv.mData[1]);
            }
            else {
                texCoordData.push_back(0);
                texCoordData.push_back(0);
            }
            
            FbxVector4 tangent = readTangent(mesh, controlPointIndex, cornerCounter);
            tangentData.push_back(tangent.mData[0]);
            tangentData.push_back(tangent.mData[1]);
            tangentData.push_back(tangent.mData[2]);
            tangentData.push_back(tangent.mData[3]);
            
            if (kDebugGeometrySource) {
                pinfo("      Read tangent %f, %f, %f, %f", tangent.mData[0], tangent.mData[1], tangent.mData[2], tangent.mData[3]);
            }
            ++cornerCounter;
        }
    }
    
    int stride = 12 * sizeof(float);
    
    vertices->set_data(vertexData.data(), vertexData.size() * sizeof(float));
    vertices->set_vertex_count((uint32_t)vertexData.size() / 3);
    vertices->set_float_components(true);
    vertices->set_components_per_vertex(3);
    vertices->set_bytes_per_component(sizeof(float));
    vertices->set_data_offset(0);
    vertices->set_data_stride(stride);
    
    texCoords->set_data(texCoordData.data(), texCoordData.size() * sizeof(float));
    texCoords->set_vertex_count((uint32_t)texCoordData.size() / 2);
    texCoords->set_float_components(true);
    texCoords->set_components_per_vertex(2);
    texCoords->set_bytes_per_component(sizeof(float));
    texCoords->set_data_offset(sizeof(float) * 3);
    texCoords->set_data_stride(stride);
    
    normals->set_data(normalData.data(), normalData.size() * sizeof(float));
    normals->set_vertex_count((uint32_t)normalData.size() / 3);
    normals->set_float_components(true);
    normals->set_components_per_vertex(3);
    normals->set_bytes_per_component(sizeof(float));
    normals->set_data_offset(sizeof(float) * 5);
    normals->set_data_stride(stride);
    
    tangents->set_data(tangentData.data(), tangentData.size() * sizeof(float));
    tangents->set_vertex_count((uint32_t)tangentData.size() / 4);
    tangents->set_float_components(true);
    tangents->set_components_per_vertex(4);
    tangents->set_bytes_per_component(sizeof(float));
    tangents->set_data_offset(sizeof(float) * 8);
    tangents->set_data_stride(stride);
    
    /*
     Export the elements and materials.
     */
    std::vector<int> materialMapping = readMaterialToMeshMapping(mesh, numPolygons);
    
    int numMaterials = node->GetMaterialCount();
    pinfo("Num materials %d", numMaterials);
    
    for (int i = 0; i < numMaterials; i++) {
        std::vector<int> triangles;
        
        for (int m = 0; m < materialMapping.size(); m++) {
            int materialIndex = materialMapping[m];
            if (materialIndex == i) {
                triangles.push_back(m);
            }
        }
        
        viro::Node::Geometry::Element *element = geo->add_element();
        element->set_data(triangles.data(), triangles.size() * sizeof(int));
        element->set_primitive(viro::Node_Geometry_Element_Primitive_Triangle);
        element->set_bytes_per_index(sizeof(int));
        element->set_primitive_count((int)triangles.size() / 3);
        
        FbxSurfaceMaterial *fbxMaterial = node->GetMaterial(i);
        viro::Node::Geometry::Material *material = geo->add_material();
        
        exportMaterial(fbxMaterial, material);
    }
}

// It turns out we can just use mesh->GetPolygonVertexNormal() instead of this function,
// but keeping this here for educational purposes, on how to read normals directly from
// a mesh.
FbxVector4 VROFBXLoader::readNormal(FbxMesh *mesh, int controlPointIndex, int cornerCounter) {
    if(mesh->GetElementNormalCount() < 1) {
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

FbxVector4 VROFBXLoader::readTangent(FbxMesh *mesh, int controlPointIndex, int cornerCounter) {
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

std::vector<int> VROFBXLoader::readMaterialToMeshMapping(FbxMesh *mesh, int numPolygons) {
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
        pabort("No material element found for mesh");
    }
    
    return materialMapping;
}

void VROFBXLoader::exportMaterial(FbxSurfaceMaterial *inMaterial, viro::Node::Geometry::Material *outMaterial) {
    viro::Node::Geometry::Material::Visual *diffuse = nullptr;
    viro::Node::Geometry::Material::Visual *specular = nullptr;
    viro::Node::Geometry::Material::Visual *normal = nullptr;
    
    if (inMaterial->GetClassId().Is(FbxSurfacePhong::ClassId)) {
        // Diffuse Color
        diffuse = outMaterial->add_visual();

        FbxDouble3 inDiffuse = reinterpret_cast<FbxSurfacePhong *>(inMaterial)->Diffuse;
        diffuse->add_color(static_cast<float>(inDiffuse.mData[0]));
        diffuse->add_color(static_cast<float>(inDiffuse.mData[1]));
        diffuse->add_color(static_cast<float>(inDiffuse.mData[2]));
        
        // Specular Color
        // double3 = reinterpret_cast<FbxSurfacePhong *>(inMaterial)->Specular;
        
        // Emissive Color
        // double3 = reinterpret_cast<FbxSurfacePhong *>(inMaterial)->Emissive;
        
        // Reflection
        // double3 = reinterpret_cast<FbxSurfacePhong *>(inMaterial)->Reflection;
        
        // Transparency Factor
        FbxDouble transparency = reinterpret_cast<FbxSurfacePhong *>(inMaterial)->TransparencyFactor;
        outMaterial->set_transparency(transparency);
        
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
        // Diffuse Color
        diffuse = outMaterial->add_visual();

        FbxDouble3 inDiffuse = reinterpret_cast<FbxSurfaceLambert *>(inMaterial)->Diffuse;
        diffuse->add_color(static_cast<float>(inDiffuse.mData[0]));
        diffuse->add_color(static_cast<float>(inDiffuse.mData[1]));
        diffuse->add_color(static_cast<float>(inDiffuse.mData[2]));
        
        // Emissive Color
        // double3 = reinterpret_cast<FbxSurfaceLambert *>(inMaterial)->Emissive;
        
        // Transparency Factor
        FbxDouble transparency = reinterpret_cast<FbxSurfaceLambert *>(inMaterial)->TransparencyFactor;
        outMaterial->set_transparency(transparency);
    }
    
    unsigned int textureIndex = 0;
    
    FBXSDK_FOR_EACH_TEXTURE(textureIndex) {
        FbxProperty property = inMaterial->FindProperty(FbxLayerElement::sTextureChannelNames[textureIndex]);
        if(property.IsValid()) {
            unsigned int textureCount = property.GetSrcObjectCount<FbxTexture>();
            for(unsigned int i = 0; i < textureCount; ++i) {
                FbxLayeredTexture *layeredTexture = property.GetSrcObject<FbxLayeredTexture>(i);
                if(layeredTexture) {
                    pabort("Layered Texture is currently unsupported\n");
                }
                else {
                    FbxTexture *texture = property.GetSrcObject<FbxTexture>(i);
                    if (texture) {
                        std::string textureType = property.GetNameAsCStr();
                        FbxFileTexture *fileTexture = FbxCast<FbxFileTexture>(texture);
                        
                        if (fileTexture) {
                            std::string fileName = std::string(fileTexture->GetFileName());
                            size_t lastPathComponent = fileName.find_last_of("/");
                            
                            std::string textureName = fileName;
                            if (lastPathComponent != std::string::npos) {
                                textureName = fileName.substr(lastPathComponent + 1, fileName.size() - lastPathComponent - 1);
                            }
                            
                            pinfo("Texture index %d, texture type [%s], texture name [%s]", textureIndex, textureType.c_str(),  textureName.c_str());

                            
                            if (textureType == "DiffuseColor") {
                                if (!diffuse) {
                                    diffuse = outMaterial->add_visual();
                                }
                                diffuse->set_texture(textureName);
                            }
                            else if (textureType == "SpecularColor") {
                                if (!specular) {
                                    specular = outMaterial->add_visual();
                                }
                                specular->set_texture(textureName);
                            }
                            else if (textureType == "Bump") {
                                if (!normal) {
                                    normal = outMaterial->add_visual();
                                }
                                normal->set_texture(textureName);
                            }
                        }
                    }
                }
            }
        }
    }
}

#pragma mark - Printing (Debug)

void VROFBXLoader::debugPrint() {
    _numTabs = 0;
    FbxManager *sdkManager = FbxManager::Create();
    
    FbxScene *scene = loadFBX(sdkManager);
    
    // Print the nodes of the scene and their attributes recursively.
    // Note that we are not printing the root node because it should
    // not contain any attributes.
    FbxNode *rootNode = scene->GetRootNode();
    if (rootNode) {
        for (int i = 0; i < rootNode->GetChildCount(); i++) {
            printNode(rootNode->GetChild(i));
        }
    }

    sdkManager->Destroy();
    
    // TODO Delete this below
    /*
     int numStacks = scene->GetSrcObjectCount(FbxCriteria::ObjectType(FbxAnimStack::ClassId));
     pinfo("Num animation stacks %d", numStacks);
     
     for (int i = 0; i < numStacks; i++) {
     FbxAnimStack *animStack = FbxCast<FbxAnimStack>(scene->GetSrcObject(FbxCriteria::ObjectType(FbxAnimStack::ClassId), i));
     int numLayers = animStack->GetMemberCount(FbxCriteria::ObjectType(FbxAnimLayer::ClassId));
     
     pinfo("  num layers %d", numLayers);
     
     for (int n = 0; n < numLayers; n++) {
     FbxAnimLayer *animLayer = FbxCast<FbxAnimLayer>(animStack->GetMember(FbxCriteria::ObjectType(FbxAnimLayer::ClassId), n));
     
     }
     }
     */
}

void VROFBXLoader::printTabs() {
    for(int i = 0; i < _numTabs; i++)
        printf("\t");
}

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

void VROFBXLoader::printAttribute(FbxNodeAttribute *pAttribute) {
    if(!pAttribute) return;
    
    FbxString typeName = GetAttributeTypeName(pAttribute->GetAttributeType());
    FbxString attrName = pAttribute->GetName();
    printTabs();
    // Note: to retrieve the character array of a FbxString, use its Buffer() method.
    printf("<attribute type='%s' name='%s'/>\n", typeName.Buffer(), attrName.Buffer());
}

void VROFBXLoader::printNode(FbxNode *pNode) {
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
    for(int i = 0; i < pNode->GetNodeAttributeCount(); i++)
        printAttribute(pNode->GetNodeAttributeByIndex(i));
    
    // Recursively print the children.
    for(int j = 0; j < pNode->GetChildCount(); j++)
        printNode(pNode->GetChild(j));
    
    _numTabs--;
    printTabs();
    printf("</node>\n");
}
