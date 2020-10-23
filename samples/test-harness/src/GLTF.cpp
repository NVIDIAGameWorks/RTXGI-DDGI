/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

#include "GLTF.h"
#include "Textures.h"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

using namespace DirectX;

namespace GLTF
{

//----------------------------------------------------------------------------------------------------------
// Private Functions
//----------------------------------------------------------------------------------------------------------

/**
* Parse the glTF cameras.
*/
void ParseGLTFCameras(const tinygltf::Model &gltfData, Scene &scene)
{
    for (size_t cameraIndex = 0; cameraIndex < gltfData.cameras.size(); cameraIndex++)
    {
        // Get the glTF camera
        const tinygltf::Camera gltfCamera = gltfData.cameras[cameraIndex];
        if (strcmp(gltfCamera.type.c_str(), "perspective") == 0)
        {
            Camera camera;
            camera.fov = (float)gltfCamera.perspective.yfov * (180.f / XM_PI);
            camera.tanHalfFovY = tanf(camera.fov * (XM_PI / 180.f) * 0.5f);

            scene.cameras.push_back(camera);
        }
    }
}

/**
* Parse the glTF nodes.
*/
void ParseGLTFNodes(const tinygltf::Model &gltfData, Scene &scene)
{
    // Get the default scene
    const tinygltf::Scene gltfScene = gltfData.scenes[gltfData.defaultScene];

    // Get the indices of the scene's root nodes
    for (size_t rootIndex = 0; rootIndex < gltfScene.nodes.size(); rootIndex++)
    {
        scene.roots.push_back(gltfScene.nodes[rootIndex]);
    }

    // Get all the nodes
    for (size_t nodeIndex = 0; nodeIndex < gltfData.nodes.size(); nodeIndex++)
    {
        // Get the glTF node
        const tinygltf::Node gltfNode = gltfData.nodes[nodeIndex];

        Node node;

        // Get the node's local transform data
        if (gltfNode.translation.size() > 0) node.translation = XMFLOAT3((float)gltfNode.translation[0], (float)gltfNode.translation[1], (float)gltfNode.translation[2]);
        if (gltfNode.rotation.size() > 0) node.rotation = XMFLOAT4((float)gltfNode.rotation[0], (float)gltfNode.rotation[1], (float)gltfNode.rotation[2], (float)gltfNode.rotation[3]);
        if (gltfNode.scale.size() > 0) node.scale = XMFLOAT3((float)gltfNode.scale[0], (float)gltfNode.scale[1], (float)gltfNode.scale[2]);

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
        // Invert z-coordinate to convert from right hand to left hand
        node.translation.z *= -1.f;
        node.rotation.z *= -1.f;
        node.rotation.w *= -1.f;
#endif
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
        // Convert to left hand, z-up (unreal)
        node.translation = { node.translation.z, node.translation.x, node.translation.y };
        node.rotation = { node.rotation.z, node.rotation.x, node.rotation.y, node.rotation.w };
#elif  RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
        // Convert to right hand, z-up
        node.translation = { node.translation.x, -node.translation.z, node.translation.y };
        node.rotation = { node.rotation.x, -node.rotation.z, node.rotation.y, node.rotation.w };
#endif

        // Camera node, store the transforms
        if (gltfNode.camera != -1)
        {
            node.camera = gltfNode.camera;
            scene.cameras[node.camera].position = node.translation;

            XMMATRIX xform = XMMatrixRotationQuaternion(XMLoadFloat4(&node.rotation));
            XMStoreFloat3(&scene.cameras[node.camera].right, xform.r[0]);
            XMStoreFloat3(&scene.cameras[node.camera].up, xform.r[1]);
            XMStoreFloat3(&scene.cameras[node.camera].forward, xform.r[2]);
        }

        // When at a leaf node, add an instance to the scene (if a mesh exists for it)
        if (gltfNode.children.size() == 0 && gltfNode.mesh != -1)
        {
            // Write the instance data
            Instance instance;
            instance.name = gltfNode.name;
            instance.mesh = gltfNode.mesh;

            node.instance = (int)scene.instances.size();
            scene.instances.push_back(instance);
        }

        // Gather the child node indices
        for (size_t childIndex = 0; childIndex < gltfNode.children.size(); childIndex++)
        {
            node.children.push_back(gltfNode.children[childIndex]);
        }

        // Add the new node to the scene graph
        scene.nodes.push_back(node);
    }
}

/*
* Parse the glTF materials into our format.
*/
void ParseGLTFMaterials(const tinygltf::Model &gltfData, Scene &scene)
{
    for (size_t i = 0; i < gltfData.materials.size(); i++)
    {
        const tinygltf::Material gltfMaterial = gltfData.materials[i];
        const tinygltf::PbrMetallicRoughness pbr = gltfMaterial.pbrMetallicRoughness;

        // Transform glTF material into our material format
        Material material;
        material.name = gltfMaterial.name;
        material.data.doubleSided = (int)gltfMaterial.doubleSided;

        // Albedo and Opacity
        material.data.albedo = XMFLOAT3((float)pbr.baseColorFactor[0], (float)pbr.baseColorFactor[1], (float)pbr.baseColorFactor[2]);
        material.data.opacity = (float)pbr.baseColorFactor[3];
        material.data.albedoTexIdx = pbr.baseColorTexture.index;

        // Alpha
        material.data.alphaCutoff = static_cast<float>(gltfMaterial.alphaCutoff);
        if (strcmp(gltfMaterial.alphaMode.c_str(), "OPAQUE") == 0) material.data.alphaMode = 0;
        else if (strcmp(gltfMaterial.alphaMode.c_str(), "BLEND") == 0) material.data.alphaMode = 1;
        else if (strcmp(gltfMaterial.alphaMode.c_str(), "MASK") == 0) material.data.alphaMode = 2;

        // Roughness and Metallic
        material.data.roughness = (float)pbr.roughnessFactor;
        material.data.metallic = (float)pbr.metallicFactor;
        material.data.roughnessMetallicTexIdx = pbr.metallicRoughnessTexture.index;

        // Normals
        material.data.normalTexIdx = gltfMaterial.normalTexture.index;

        // Emissive
        material.data.emissiveColor = XMFLOAT3((float)gltfMaterial.emissiveFactor[0], (float)gltfMaterial.emissiveFactor[1], (float)gltfMaterial.emissiveFactor[2]);
        material.data.emissiveTexIdx = gltfMaterial.emissiveTexture.index;

        scene.materials.push_back(material);
    }

    // If there are no materials, create a default material
    if (scene.materials.size() == 0)
    {
        Material default;
        scene.materials.push_back(default);
    }
}

/**
* Parse glTF textures and load the images.
*/
void ParseGLFTextures(const tinygltf::Model &gltfData, const ConfigInfo &config, Scene &scene)
{
    for (size_t textureIndex = 0; textureIndex < gltfData.textures.size(); textureIndex++)
    {
        const tinygltf::Texture gltfTexture = gltfData.textures[textureIndex];

        Texture texture;
        texture.name = gltfTexture.name;

        // Skip this texture if the source image doesn't exist
        if (gltfTexture.source == -1 || gltfData.images.size() <= gltfTexture.source) continue;

        // Construct the texture image filepath
        texture.filepath = config.root;
        texture.filepath.append(config.scenePath);
        texture.filepath.append(gltfData.images[gltfTexture.source].uri);

        if (strcmp(texture.name.c_str(), "") == 0)
        {
            texture.name = gltfData.images[gltfTexture.source].uri;
        }

        // Load the texture from disk
        Textures::LoadTexture(texture);

        // Add the texture to the scene
        scene.textures.push_back(texture);
    }
}

/**
* Parse the glTF meshes.
*/
void ParseGLTFMeshes(const tinygltf::Model &gltfData, Scene &scene)
{
    // Note: GTLF 2.0's default coordinate system is Right Handed, Y-Up
    // https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#coordinate-system-and-units
    // Meshes are converted from this coordinate system to the chosen coordinate system.

    UINT geometryIndex = 0;
    for (size_t meshIndex = 0; meshIndex < gltfData.meshes.size(); meshIndex++)
    {
        const tinygltf::Mesh gltfMesh = gltfData.meshes[meshIndex];

        Mesh mesh;
        mesh.name = gltfMesh.name;
        for (size_t primitiveIndex = 0; primitiveIndex < gltfMesh.primitives.size(); primitiveIndex++)
        {
            tinygltf::Primitive p = gltfMesh.primitives[primitiveIndex];
            tinygltf::Material  mat = gltfData.materials[p.material];

            MeshPrimitive m;
            m.index = geometryIndex;
            m.material = p.material;

            // Set to the default material if one is not assigned
            if (m.material == -1) m.material = 0;

            // If the mesh material is blended or masked, it is not opaque
            if (strcmp(mat.alphaMode.c_str(), "OPAQUE") != 0) m.opaque = false;

            // Get data indices
            int indicesIndex = p.indices;
            int positionIndex = -1;
            int normalIndex = -1;
            int tangentIndex = -1;
            int uv0Index = -1;

            if (p.attributes.count("POSITION") > 0)
            {
                positionIndex = p.attributes["POSITION"];
            }

            if (p.attributes.count("NORMAL") > 0)
            {
                normalIndex = p.attributes["NORMAL"];
            }

            if (p.attributes.count("TANGENT"))
            {
                tangentIndex = p.attributes["TANGENT"];
            }

            if (p.attributes.count("TEXCOORD_0") > 0)
            {
                uv0Index = p.attributes["TEXCOORD_0"];
            }

            // Bounding Box
            if (positionIndex > -1)
            {
                std::vector<double> min = gltfData.accessors[positionIndex].minValues;
                std::vector<double> max = gltfData.accessors[positionIndex].maxValues;

                m.boundingBox.min = { (float)min[0], (float)min[1], (float)min[2] };
                m.boundingBox.max = { (float)max[0], (float)max[1], (float)max[2] };
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
                // Invert the z-coordinate to convert from right hand to left hand
                m.boundingBox.min.z *= -1.f;
                m.boundingBox.max.z *= -1.f;
#endif
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
                // Convert to left hand, z-up (unreal)
                m.boundingBox.min = { m.boundingBox.min.z, m.boundingBox.min.x, m.boundingBox.min.y };
                m.boundingBox.max = { m.boundingBox.max.z, m.boundingBox.max.x, m.boundingBox.max.y };
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
                // Convert to right hand, z-up
                m.boundingBox.min = { m.boundingBox.min.x, -m.boundingBox.min.z, m.boundingBox.min.y };
                m.boundingBox.max = { m.boundingBox.max.x, -m.boundingBox.max.z, m.boundingBox.max.y };
#endif
            }

            // Vertex positions
            tinygltf::Accessor positionAccessor = gltfData.accessors[positionIndex];
            tinygltf::BufferView positionBufferView = gltfData.bufferViews[positionAccessor.bufferView];
            tinygltf::Buffer positionBuffer = gltfData.buffers[positionBufferView.buffer];
            int positionStride = tinygltf::GetComponentSizeInBytes(positionAccessor.componentType) * tinygltf::GetNumComponentsInType(positionAccessor.type);
            assert(positionStride == 12);

            // Vertex indices
            tinygltf::Accessor indexAccessor = gltfData.accessors[indicesIndex];
            tinygltf::BufferView indexBufferView = gltfData.bufferViews[indexAccessor.bufferView];
            tinygltf::Buffer indexBuffer = gltfData.buffers[indexBufferView.buffer];
            int indexStride = tinygltf::GetComponentSizeInBytes(indexAccessor.componentType) * tinygltf::GetNumComponentsInType(indexAccessor.type);
            m.indices.resize(indexAccessor.count);

            // Vertex normals
            tinygltf::Accessor normalAccessor;
            tinygltf::BufferView normalBufferView;
            tinygltf::Buffer normalBuffer;
            int normalStride = -1;
            if (normalIndex > -1)
            {
                normalAccessor = gltfData.accessors[normalIndex];
                normalBufferView = gltfData.bufferViews[normalAccessor.bufferView];
                normalBuffer = gltfData.buffers[normalBufferView.buffer];
                normalStride = tinygltf::GetComponentSizeInBytes(normalAccessor.componentType) * tinygltf::GetNumComponentsInType(normalAccessor.type);
                assert(normalStride == 12);
            }

            // Vertex tangents
            tinygltf::Accessor tangentAccessor;
            tinygltf::BufferView tangentBufferView;
            tinygltf::Buffer tangentBuffer;
            int tangentStride = -1;
            if (tangentIndex > -1)
            {
                tangentAccessor = gltfData.accessors[tangentIndex];
                tangentBufferView = gltfData.bufferViews[tangentAccessor.bufferView];
                tangentBuffer = gltfData.buffers[tangentBufferView.buffer];
                tangentStride = tinygltf::GetComponentSizeInBytes(tangentAccessor.componentType) * tinygltf::GetNumComponentsInType(tangentAccessor.type);
                assert(tangentStride == 16);
            }

            // Vertex texture coordinates
            tinygltf::Accessor uv0Accessor;
            tinygltf::BufferView uv0BufferView;
            tinygltf::Buffer uv0Buffer;
            int uv0Stride = -1;
            if (uv0Index > -1)
            {
                uv0Accessor = gltfData.accessors[uv0Index];
                uv0BufferView = gltfData.bufferViews[uv0Accessor.bufferView];
                uv0Buffer = gltfData.buffers[uv0BufferView.buffer];
                uv0Stride = tinygltf::GetComponentSizeInBytes(uv0Accessor.componentType) * tinygltf::GetNumComponentsInType(uv0Accessor.type);
                assert(uv0Stride == 8);
            }

            // Get the vertex data
            for (size_t vertexIndex = 0; vertexIndex < positionAccessor.count; vertexIndex++)
            {
                Vertex v;

                UINT8* address = positionBuffer.data.data() + positionBufferView.byteOffset + positionAccessor.byteOffset + (vertexIndex * positionStride);
                memcpy(&v.position, address, positionStride);

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
                // Invert z-coordinate to convert from right hand to left hand
                v.position.z *= -1.f;
#endif
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
                // Convert to left hand, z-up (unreal)
                v.position = { v.position.z, v.position.x, v.position.y };
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
                // Convert to right hand, z-up
                v.position = { v.position.x, -v.position.z, v.position.y };
#endif

                if (normalIndex > -1)
                {
                    address = normalBuffer.data.data() + normalBufferView.byteOffset + normalAccessor.byteOffset + (vertexIndex * normalStride);
                    memcpy(&v.normal, address, normalStride);

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
                    // Invert the z-coordinate to convert from right hand to left hand
                    v.normal.z *= -1.f;
#endif
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
                    // Convert to left hand, z-up (unreal)
                    v.normal = { v.normal.z, v.normal.x, v.normal.y };
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
                    // Convert to right hand, z-up
                    v.normal = { v.normal.x, -v.normal.z, v.normal.y };
#endif
                }

                if (tangentIndex > -1)
                {
                    address = tangentBuffer.data.data() + tangentBufferView.byteOffset + tangentAccessor.byteOffset + (vertexIndex * tangentStride);
                    memcpy(&v.tangent, address, tangentStride);

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
                    // Invert the z-coordinate to convert from right hand to left hand
                    v.tangent.z *= -1.f;
#endif
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
                    // Convert to left hand, z-up (unreal)
                    v.tangent = { v.tangent.z, v.tangent.x, v.tangent.y, v.tangent.w };
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
                    // Convert to right hand, z-up
                    v.tangent = { v.tangent.x, -v.tangent.z, v.tangent.y, v.tangent.w };
#endif
                }

                if (uv0Index > -1)
                {
                    address = uv0Buffer.data.data() + uv0BufferView.byteOffset + uv0Accessor.byteOffset + (vertexIndex * uv0Stride);
                    memcpy(&v.uv0, address, uv0Stride);
                }

                m.vertices.push_back(v);
            }

            // Get the index data
            // Indices can be either unsigned char, unsigned short, or unsigned long
            // Converting to full precision for easy use on GPU
            UINT8* baseAddress = indexBuffer.data.data() + indexBufferView.byteOffset + indexAccessor.byteOffset;
            if (indexStride == 1)
            {
                std::vector<UINT8> quarter;
                quarter.resize(indexAccessor.count);

                memcpy(quarter.data(), baseAddress, (indexAccessor.count * indexStride));

                // Convert quarter precision indices to full precision
                for (size_t i = 0; i < indexAccessor.count; i++)
                {
                    m.indices[i] = quarter[i];
                }
            }
            else if (indexStride == 2)
            {
                std::vector<UINT16> half;
                half.resize(indexAccessor.count);

                memcpy(half.data(), baseAddress, (indexAccessor.count * indexStride));

                // Convert half precision indices to full precision
                for (size_t i = 0; i < indexAccessor.count; i++)
                {
                    m.indices[i] = half[i];
                }
            }
            else
            {
                memcpy(m.indices.data(), baseAddress, (indexAccessor.count * indexStride));
            }

            // Add the mesh primitive
            mesh.primitives.push_back(m);

            geometryIndex++;
        }

        scene.meshes.push_back(mesh);
    }

    scene.numGeometries = geometryIndex;
}

/**
* Parse the various data of a GLTF file.
*/
void ParseGLTF(const tinygltf::Model gltfData, const ConfigInfo &config, Scene &scene)
{
    // Parse Cameras
    ParseGLTFCameras(gltfData, scene);

    // Parse Nodes
    ParseGLTFNodes(gltfData, scene);

    // Parse Materials
    ParseGLTFMaterials(gltfData, scene);

    // Parse and Load Textures
    ParseGLFTextures(gltfData, config, scene);

    // Parse Meshes
    ParseGLTFMeshes(gltfData, scene);
}

/**
* Traverse the scene graph and update the instance transforms.
*/
void TraverseScene(size_t nodeIndex, XMMATRIX &transform, Scene &scene)
{
    // Get the node
    Node node = scene.nodes[nodeIndex];

    // Get the node's transforms
    XMFLOAT3 translation = node.translation;
    XMFLOAT4 rotation = node.rotation;
    XMFLOAT3 scale = node.scale;

    // Compose the node's local transform, M = T * R * S
    XMMATRIX t = XMMatrixTranslation(translation.x, translation.y, translation.z);
    XMMATRIX r = XMMatrixRotationQuaternion(XMLoadFloat4(&rotation));
    XMMATRIX s = XMMatrixScaling(scale.x, scale.y, scale.z);
    XMMATRIX nodeTransform = XMMatrixMultiply(XMMatrixMultiply(s, r), t);

    // Compose the global transform
    transform = XMMatrixMultiply(nodeTransform, transform);

    // When at a leaf node with a mesh, update the mesh instance's transform
    // Not currently supporting nested transforms for camera nodes
    if (node.children.size() == 0 && node.instance > -1)
    {
        Instance* instance = &scene.instances[node.instance];

        // Update the instance's transform data
        XMMATRIX transpose = XMMatrixTranspose(transform);
        memcpy(instance->transform, &transpose, sizeof(XMFLOAT4) * 3);
        return;
    }

    // Recursively traverse the scene graph
    for (size_t i = 0; i < node.children.size(); i++)
    {
        TraverseScene(node.children[i], transform, scene);
    }
}

//----------------------------------------------------------------------------------------------------------
// Public Functions
//----------------------------------------------------------------------------------------------------------

/**
* Loads and parse a glTF scene.
*/
bool Load(const ConfigInfo &config, Scene &scene)
{
    tinygltf::Model gltfData;
    tinygltf::TinyGLTF gltfLoader;
    std::string err, warn, filepath;

    // Build the path to to GLTF file
    filepath = config.root;
    filepath.append(config.scenePath);
    filepath.append(config.sceneFile);

    bool binary = false;
    if (config.sceneFile.find(".glb") != std::string::npos) binary = true;
    else if (config.sceneFile.find(".gltf")) binary = false;
    else return false; // Unknown file format

    // Load the scene
    bool result = false;
    if (binary)
    {
        result = gltfLoader.LoadBinaryFromFile(&gltfData, &err, &warn, filepath);
    }
    else
    {
        result = gltfLoader.LoadASCIIFromFile(&gltfData, &err, &warn, filepath);
    }

    if (!result)
    {
        // An error occurred
        std::string msg = std::string(err.begin(), err.end());
        MessageBox(NULL, msg.c_str(), "Error", MB_OK);
        return false;
    }
    else if (warn.length() > 0)
    {
        // Warning
        std::string msg = std::string(warn.begin(), warn.end());
        MessageBox(NULL, msg.c_str(), "Warning", MB_OK);
        return false;
    }

    // Parse the GLTF data
    ParseGLTF(gltfData, config, scene);

    // Traverse the scene graph and update instance transforms
    for(size_t rootIndex = 0; rootIndex < scene.roots.size(); rootIndex++)
    {
        XMMATRIX transform = XMMatrixIdentity();
        int nodeIndex = scene.roots[rootIndex];
        TraverseScene(nodeIndex, transform, scene);
    }

    return true;
}

/*
* Releases memory used by the glTF scene.
*/
void Cleanup(Scene &scene)
{
    // Release texture memory
    for (size_t textureIndex = 0; textureIndex < scene.textures.size(); textureIndex++)
    {
        Textures::UnloadTexture(scene.textures[textureIndex]);
    }
}

}
