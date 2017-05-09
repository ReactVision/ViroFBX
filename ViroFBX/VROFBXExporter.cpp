//
//  VROFBXExporter.cpp
//  ViroFBX
//
//  Created by Raj Advani on 4/25/17.
//  Copyright © 2017 Viro. All rights reserved.
//

#include "VROFBXExporter.h"
#include "VROLog.h"
#include <fstream>

static const bool kDebugGeometrySource = false;

#pragma mark - Initialization

VROFBXExporter::VROFBXExporter() {
    _fbxManager = FbxManager::Create();

}

VROFBXExporter::~VROFBXExporter() {
    _fbxManager->Destroy();
}

FbxScene *VROFBXExporter::loadFBX(std::string fbxPath) {
    FbxIOSettings *ios = FbxIOSettings::Create(_fbxManager, IOSROOT);
    _fbxManager->SetIOSettings(ios);
    
    FbxImporter *importer = FbxImporter::Create(_fbxManager, "");
    
    pinfo("Loading file [%s]", fbxPath.c_str());
    bool importStatus = importer->Initialize(fbxPath.c_str());
    
    if (!importStatus) {
        pinfo("Call to FBXImporter::Initialize() failed");
        pinfo("Error returned: %s", importer->GetStatus().GetErrorString());
    }
    else {
        pinfo("Import successful");
    }
    
    FbxScene *scene = FbxScene::Create(_fbxManager, "scene");
    importer->Import(scene);
    importer->Destroy();
    
    return scene;
}

#pragma mark - Export

void VROFBXExporter::exportFBX(std::string fbxPath, std::string destPath) {
    FbxScene *scene = loadFBX(fbxPath);
    
    pinfo("Triangulating scene...");
    
    FbxGeometryConverter converter(_fbxManager);
    converter.Triangulate(scene, true);
    
    pinfo("Exporting FBX...");
    viro::Node *outNode = new viro::Node();

    FbxNode *rootNode = scene->GetRootNode();
    if (rootNode) {
        for (int i = 0; i < rootNode->GetChildCount(); i++) {
            exportNode(rootNode->GetChild(i), outNode->add_subnode());
        }
    }
    
    int byteSize = outNode->ByteSize();
    
    pinfo("Writing protobuf [%d bytes]...", byteSize);
    std::string encoded = outNode->SerializeAsString();
    std::ofstream(destPath.c_str(), std::ios::binary).write(encoded.c_str(), encoded.size());
    
    pinfo("Export complete");
}

void VROFBXExporter::exportNode(FbxNode *node, viro::Node *outNode) {
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
    
    if (node->GetMesh() != nullptr) {
        exportGeometry(node, outNode->mutable_geometry());
    }
    for (int i = 0; i < node->GetChildCount(); i++) {
        exportNode(node->GetChild(i), outNode->add_subnode());
    }
}

void VROFBXExporter::exportGeometry(FbxNode *node, viro::Node::Geometry *geo) {
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
    
    pinfo("   UV set name %s", uvSetName);
    
    /*
     Export the vertex, normal, tex-coord, and tangent data (the geometry
     sources).
     */
    std::vector<float> data;
    
    int numPolygons = mesh->GetPolygonCount();
    pinfo("   Polygon count %d", numPolygons);

    for (unsigned int i = 0; i < numPolygons; i++) {
        if (kDebugGeometrySource) {
            pinfo("   Reading polygon %d", i);
        }
        
        // We only support triangles
        int polygonSize = mesh->GetPolygonSize(i);
        passert (polygonSize == 3);
        
        for (int j = 0; j < polygonSize; j++) {
            if (kDebugGeometrySource) {
                pinfo("      V%d", j);
            }
            int controlPointIndex = mesh->GetPolygonVertex(i, j);
            
            FbxVector4 vertex = mesh->GetControlPointAt(controlPointIndex);
            data.push_back(vertex.mData[0]);
            data.push_back(vertex.mData[1]);
            data.push_back(vertex.mData[2]);
            
            if (kDebugGeometrySource) {
                pinfo("         Read vertex %f, %f, %f", vertex.mData[0], vertex.mData[1], vertex.mData[2]);
            }
            
            FbxVector2 uv;
            bool unmapped;
            bool hasUV = mesh->GetPolygonVertexUV(i, j, uvSetName, uv, unmapped);
            
            if (kDebugGeometrySource) {
                pinfo("         Read UV %f, %f", uv.mData[0], uv.mData[1]);
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
                pinfo("         Read normal %f, %f, %f", normal.mData[0], normal.mData[1], normal.mData[2]);
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
                pinfo("         Read tangent %f, %f, %f, %f", tangent.mData[0], tangent.mData[1], tangent.mData[2], tangent.mData[3]);
            }
            ++cornerCounter;
        }
    }
    
    int floatsPerVertex = 12;
    int numVertices = (uint32_t)data.size() / floatsPerVertex;
    int stride = floatsPerVertex * sizeof(float);
    
    passert (numPolygons * 3 * stride == data.size() * sizeof(float));
    geo->set_data(data.data(), data.size() * sizeof(float));
    
    pinfo("   Num vertices %d, stride %d", numVertices, stride);
    pinfo("   VAR size %lu", data.size() * sizeof(float));
    
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
    std::vector<int> materialMapping = readMaterialToMeshMapping(mesh, numPolygons);
    
    int numMaterials = node->GetMaterialCount();
    pinfo("   Num materials %d", numMaterials);
    
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
        
        exportMaterial(fbxMaterial, material);
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
        pabort("No material element found for mesh");
    }
    
    return materialMapping;
}

void VROFBXExporter::exportMaterial(FbxSurfaceMaterial *inMaterial, viro::Node::Geometry::Material *outMaterial) {
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
        outMaterial->set_lighting_model(viro::Node_Geometry_Material_LightingModel_Phong);
        
        // Diffuse Color
        viro::Node::Geometry::Material::Visual *diffuse = outMaterial->mutable_diffuse();

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
        outMaterial->set_lighting_model(viro::Node_Geometry_Material_LightingModel_Lambert);

        // Diffuse Color
        viro::Node::Geometry::Material::Visual *diffuse = outMaterial->mutable_diffuse();

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
                            pinfo("Texture index %d, texture type [%s], texture name [%s]", textureIndex, textureType.c_str(),  textureName.c_str());

                            if (textureType == "DiffuseColor") {
                                outMaterial->mutable_diffuse()->set_texture(textureName);
                            }
                            else if (textureType == "SpecularColor") {
                                outMaterial->mutable_specular()->set_texture(textureName);
                            }
                            else if (textureType == "Bump") {
                                outMaterial->mutable_normal()->set_texture(textureName);
                            }
                        }
                    }
                }
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
        pinfo("            Entry: %s", entryText.c_str());
        
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
                    pinfo("Texture [%s] found for entry [%s]", textureName.c_str(), entryText.c_str());
                    
                    if (endsWith(entryText, "DiffuseTexture")) {
                        outMaterial->mutable_diffuse()->set_texture(textureName);
                    }
                    else if (endsWith(entryText, "SpecularTexture")) {
                        outMaterial->mutable_specular()->set_texture(textureName);
                    }
                    else if (endsWith(entryText, "NormalTexture")) {
                        outMaterial->mutable_normal()->set_texture(textureName);
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

void VROFBXExporter::printTabs() {
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
    size_t lastPathComponent = fileName.find_last_of("/");
    
    std::string textureName = fileName;
    if (lastPathComponent != std::string::npos) {
        textureName = fileName.substr(lastPathComponent + 1, fileName.size() - lastPathComponent - 1);
    }
    return textureName;
}

bool VROFBXExporter::endsWith(const std::string& candidate, const std::string& ending) {
    if (candidate.length() < ending.length()) {
        return false;
    }
    return 0 == candidate.compare(candidate.length() - ending.length(), ending.length(), ending);
}

