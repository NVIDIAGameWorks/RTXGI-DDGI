/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "Caches.h"
#include "Scenes.h"
#include "UI.h"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#include <regex>
#include <math.h>

using namespace DirectX;

namespace Scenes
{

    //----------------------------------------------------------------------------------------------------------
    // Private Functions
    //----------------------------------------------------------------------------------------------------------

    void SetTranslation(const tinygltf::Node& gltfNode, XMFLOAT3& translation)
    {
    #if COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT
        translation = XMFLOAT3((float)gltfNode.translation[0], (float)gltfNode.translation[1], (float)gltfNode.translation[2]);
    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT_Z_UP
        translation = XMFLOAT3((float)gltfNode.translation[0], -(float)gltfNode.translation[2], (float)gltfNode.translation[1]);
    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT
        translation = XMFLOAT3((float)gltfNode.translation[0], (float)gltfNode.translation[1], -(float)gltfNode.translation[2]);
    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT_Z_UP
        translation = XMFLOAT3(-(float)gltfNode.translation[2], (float)gltfNode.translation[0], (float)gltfNode.translation[1]);
    #endif
    }

    void SetRotation(const tinygltf::Node& gltfNode, XMFLOAT4& rotation)
    {
    #if COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT
        rotation = XMFLOAT4((float)gltfNode.rotation[0], (float)gltfNode.rotation[1], (float)gltfNode.rotation[2], (float)gltfNode.rotation[3]);
    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT_Z_UP
        rotation = XMFLOAT4((float)gltfNode.rotation[0], -(float)gltfNode.rotation[2], (float)gltfNode.rotation[1], (float)gltfNode.rotation[3]);
    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT
        rotation = XMFLOAT4(-(float)gltfNode.rotation[0], -(float)gltfNode.rotation[1], (float)gltfNode.rotation[2], (float)gltfNode.rotation[3]);
    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT_Z_UP
        rotation = XMFLOAT4((float)gltfNode.rotation[2], -(float)gltfNode.rotation[0], -(float)gltfNode.rotation[1], (float)gltfNode.rotation[3]);
    #endif
    }

    void SetScale(const tinygltf::Node& gltfNode, XMFLOAT3& scale)
    {
    #if COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT || COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT
        scale = XMFLOAT3((float)gltfNode.scale[0], (float)gltfNode.scale[1], (float)gltfNode.scale[2]);
    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT_Z_UP
        scale = XMFLOAT3((float)gltfNode.scale[0], (float)gltfNode.scale[2], (float)gltfNode.scale[1]);
    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT_Z_UP
        scale = XMFLOAT3((float)gltfNode.scale[2], (float)gltfNode.scale[0], (float)gltfNode.scale[1]);
    #endif
    }

    /**
     * Parse a URI, removing escaped characters (e.g. %20 for spaces)
     */
    std::string ParseURI(const std::string& in)
    {
        std::string result = std::string(in.begin(), in.end());
        size_t pos = result.find("%20");
        while (pos != std::string::npos)
        {
            result.replace(pos, 3, 1, ' ');
            pos = result.find("%20");
        }
        return result;
    }

    /**
     * Parse the glTF cameras.
     */
    void ParseGLTFCameras(const tinygltf::Model& gltfData, Scene& scene)
    {
        for (uint32_t cameraIndex = 0; cameraIndex < static_cast<uint32_t>(gltfData.cameras.size()); cameraIndex++)
        {
            // Get the glTF camera
            const tinygltf::Camera gltfCamera = gltfData.cameras[cameraIndex];
            if (strcmp(gltfCamera.type.c_str(), "perspective") == 0)
            {
                Camera camera;
                camera.data.fov = (float)gltfCamera.perspective.yfov * (180.f / XM_PI);
                camera.data.tanHalfFovY = tanf(camera.data.fov * (XM_PI / 180.f) * 0.5f);

                UpdateCamera(camera);

                scene.cameras.push_back(camera);
            }
        }
    }

    /**
     * Parse the glTF nodes.
     */
    void ParseGLTFNodes(const tinygltf::Model& gltfData, Scene& scene)
    {
        // Get the default scene
        const tinygltf::Scene gltfScene = gltfData.scenes[gltfData.defaultScene];

        // Get the indices of the scene's root nodes
        for (uint32_t rootIndex = 0; rootIndex < static_cast<uint32_t>(gltfScene.nodes.size()); rootIndex++)
        {
            scene.rootNodes.push_back(gltfScene.nodes[rootIndex]);
        }

        // Get all the nodes
        for (uint32_t nodeIndex = 0; nodeIndex < static_cast<uint32_t>(gltfData.nodes.size()); nodeIndex++)
        {
            // Get the glTF node
            const tinygltf::Node gltfNode = gltfData.nodes[nodeIndex];

            // Create the scene node
            SceneNode node;

            // Get the node's local transform data
            if(gltfNode.matrix.size() > 0)
            {
                node.matrix = XMMATRIX(
                    (float)gltfNode.matrix[0],  (float)gltfNode.matrix[1],  (float)gltfNode.matrix[2],  (float)gltfNode.matrix[3],
                    (float)gltfNode.matrix[4],  (float)gltfNode.matrix[5],  (float)gltfNode.matrix[6],  (float)gltfNode.matrix[7],
                    (float)gltfNode.matrix[8],  (float)gltfNode.matrix[9],  (float)gltfNode.matrix[10], (float)gltfNode.matrix[11],
                    (float)gltfNode.matrix[12], (float)gltfNode.matrix[13], (float)gltfNode.matrix[14], (float)gltfNode.matrix[15]
                );
                node.hasMatrix = true;
            }
            else
            {
                if (gltfNode.translation.size() > 0) SetTranslation(gltfNode, node.translation);
                if (gltfNode.rotation.size() > 0) SetRotation(gltfNode, node.rotation);
                if (gltfNode.scale.size() > 0) SetScale(gltfNode, node.scale);
            }

            // Camera node, store the transforms
            if (gltfNode.camera != -1)
            {
                node.camera = gltfNode.camera;
                scene.cameras[node.camera].data.position = { node.translation.x, node.translation.y, node.translation.z };

                XMMATRIX xform = XMMatrixRotationQuaternion(XMLoadFloat4(&node.rotation));
                XMFLOAT3 right, up, forward;

                XMStoreFloat3(&right, xform.r[0]);
                XMStoreFloat3(&up, xform.r[1]);
                XMStoreFloat3(&forward, xform.r[2]);

                scene.cameras[node.camera].data.right = { right.x, right.y, right.z };
                scene.cameras[node.camera].data.up = { up.x, up.y, up.z };
                scene.cameras[node.camera].data.forward = { forward.x, forward.y, forward.z };
            }

            // When at a leaf node, add a mesh instance to the scene (if a mesh exists for the node)
            if (gltfNode.children.size() == 0 && gltfNode.mesh != -1)
            {
                // Write the instance data
                MeshInstance instance;
                instance.name = gltfNode.name;
                if (instance.name.compare("") == 0) instance.name = "Instance_" + std::to_string(static_cast<int>(scene.instances.size()));
                instance.meshIndex = gltfNode.mesh;

                node.instance = static_cast<int>(scene.instances.size());
                scene.instances.push_back(instance);
            }

            // Gather the child node indices
            for (uint32_t childIndex = 0; childIndex < static_cast<uint32_t>(gltfNode.children.size()); childIndex++)
            {
                node.children.push_back(gltfNode.children[childIndex]);
            }

            // Add the new node to the scene graph
            scene.nodes.push_back(node);
        }

        // Traverse the scene graph and update instance transforms
        for (size_t rootIndex = 0; rootIndex < scene.rootNodes.size(); rootIndex++)
        {
            XMMATRIX transform = XMMatrixIdentity();
            int nodeIndex = scene.rootNodes[rootIndex];
            Traverse(nodeIndex, transform, scene);
        }
    }

    /**
     * Parse the glTF materials into our format.
     */
    void ParseGLTFMaterials(const tinygltf::Model& gltfData, Scene& scene)
    {
        for (uint32_t materialIndex = 0; materialIndex < static_cast<uint32_t>(gltfData.materials.size()); materialIndex++)
        {
            const tinygltf::Material gltfMaterial = gltfData.materials[materialIndex];
            const tinygltf::PbrMetallicRoughness pbr = gltfMaterial.pbrMetallicRoughness;

            // Transform glTF material into our material format
            Material material;
            material.name = gltfMaterial.name;
            if (material.name.compare("") == 0) material.name = "Material_" + std::to_string(materialIndex);

            material.data.doubleSided = (int)gltfMaterial.doubleSided;

            // Albedo and Opacity
            material.data.albedo = { (float)pbr.baseColorFactor[0], (float)pbr.baseColorFactor[1], (float)pbr.baseColorFactor[2] };
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
            material.data.emissiveColor = { (float)gltfMaterial.emissiveFactor[0], (float)gltfMaterial.emissiveFactor[1], (float)gltfMaterial.emissiveFactor[2] };
            material.data.emissiveTexIdx = gltfMaterial.emissiveTexture.index;

            scene.materials.push_back(material);
        }

        // If there are no materials, create a default material
        if (scene.materials.size() == 0)
        {
            Material mat = {};
            mat.name = "Default Material";
            mat.data.albedo = { 1.f, 1.f, 1.f };
            mat.data.opacity = 1.f;
            mat.data.roughness = 1.f;
            mat.data.albedoTexIdx = -1;
            mat.data.roughnessMetallicTexIdx = -1;
            mat.data.normalTexIdx = -1;
            mat.data.emissiveTexIdx = -1;

            scene.materials.push_back(mat);
        }
    }

    /**
     * Parse glTF textures and load the images.
     */
    bool ParseGLFTextures(const tinygltf::Model& gltfData, const Configs::Config& config, Scene& scene)
    {
        for (uint32_t textureIndex = 0; textureIndex < static_cast<uint32_t>(gltfData.textures.size()); textureIndex++)
        {
            // Get the GLTF texture
            const tinygltf::Texture& gltfTexture = gltfData.textures[textureIndex];

            // Skip this texture if the source image doesn't exist
            if (gltfTexture.source == -1 || gltfData.images.size() <= gltfTexture.source) continue;

            // Get the GLTF image
            const tinygltf::Image gltfImage = gltfData.images[gltfTexture.source];

            Textures::Texture texture;
            texture.SetName(gltfImage.uri);
            texture.SetName(gltfImage.name);
            texture.SetName(gltfTexture.name);

            // Construct the texture image filepath
            texture.filepath = config.app.root + config.scene.path + ParseURI(gltfImage.uri);

            // Load the texture from disk
            if (!Textures::Load(texture)) return false;

        #if defined(WIN32) && defined(__x86_64__) || defined(_M_X64)
            // Generate mipmaps and compress the texture (Windows only)
            if(!Textures::MipmapAndCompress(texture)) return false;
        #endif

            // Add the texture to the scene
            scene.textures.push_back(texture);
        }
        return true;
    }

    /**
     * Parse the glTF meshes.
     */
    void ParseGLTFMeshes(const tinygltf::Model& gltfData, Scene& scene)
    {
        // Note: GTLF 2.0's default coordinate system is Right Handed, Y-Up
        // https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#coordinate-system-and-units
        // Meshes are converted from this coordinate system to the chosen coordinate system.

        uint32_t geometryIndex = 0;
        for (uint32_t meshIndex = 0; meshIndex < static_cast<uint32_t>(gltfData.meshes.size()); meshIndex++)
        {
            const tinygltf::Mesh& gltfMesh = gltfData.meshes[meshIndex];

            Mesh mesh;
            mesh.name = gltfMesh.name;
            mesh.numVertices = 0;
            mesh.numIndices = 0;
            if (mesh.name.compare("") == 0) mesh.name = "Mesh_" + std::to_string(meshIndex);

            // Initialize the mesh bounding box
            mesh.boundingBox.min = { FLT_MAX, FLT_MAX, FLT_MAX };
            mesh.boundingBox.max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

            uint32_t vertexByteOffset = 0;
            uint32_t indexByteOffset = 0;
            for (uint32_t primitiveIndex = 0; primitiveIndex < static_cast<uint32_t>(gltfMesh.primitives.size()); primitiveIndex++)
            {
                // Get a reference to the mesh primitive
                const tinygltf::Primitive& p = gltfMesh.primitives[primitiveIndex];

                MeshPrimitive mp;
                mp.index = geometryIndex;
                mp.material = p.material;
                mp.vertexByteOffset = vertexByteOffset;
                mp.indexByteOffset = indexByteOffset;

                // Initialize the mesh primitive bounding box
                mp.boundingBox.min = { FLT_MAX, FLT_MAX, FLT_MAX };
                mp.boundingBox.max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

                // Set the mesh primitive's material to the default material if one is not assigned or if no materials exist in the GLTF
                if (mp.material == -1) mp.material = 0;

                // Get a reference to the mesh primitive's material
                // If the mesh primitive material is blended or masked, it is not opaque
                const Material& mat = scene.materials[mp.material];
                if (mat.data.alphaMode != 0) mp.opaque = false;

                // Get data indices
                int indicesIndex = p.indices;
                int positionIndex = -1;
                int normalIndex = -1;
                int tangentIndex = -1;
                int uv0Index = -1;

                if (p.attributes.count("POSITION") > 0)
                {
                    positionIndex = p.attributes.at("POSITION");
                }

                if (p.attributes.count("NORMAL") > 0)
                {
                    normalIndex = p.attributes.at("NORMAL");
                }

                if (p.attributes.count("TANGENT"))
                {
                    tangentIndex = p.attributes.at("TANGENT");
                }

                if (p.attributes.count("TEXCOORD_0") > 0)
                {
                    uv0Index = p.attributes.at("TEXCOORD_0");
                }

                // Vertex positions
                const tinygltf::Accessor& positionAccessor = gltfData.accessors[positionIndex];
                const tinygltf::BufferView& positionBufferView = gltfData.bufferViews[positionAccessor.bufferView];
                const tinygltf::Buffer& positionBuffer = gltfData.buffers[positionBufferView.buffer];
                const uint8_t* positionBufferAddress = positionBuffer.data.data();
                int positionStride = tinygltf::GetComponentSizeInBytes(positionAccessor.componentType) * tinygltf::GetNumComponentsInType(positionAccessor.type);
                assert(positionStride == 12);

                // Vertex indices
                const tinygltf::Accessor& indexAccessor = gltfData.accessors[indicesIndex];
                const tinygltf::BufferView& indexBufferView = gltfData.bufferViews[indexAccessor.bufferView];
                const tinygltf::Buffer& indexBuffer = gltfData.buffers[indexBufferView.buffer];
                const uint8_t* indexBufferAddress = indexBuffer.data.data();
                int indexStride = tinygltf::GetComponentSizeInBytes(indexAccessor.componentType) * tinygltf::GetNumComponentsInType(indexAccessor.type);
                mp.indices.resize(indexAccessor.count);

                // Vertex normals
                tinygltf::Accessor normalAccessor;
                tinygltf::BufferView normalBufferView;
                const uint8_t* normalBufferAddress = nullptr;
                int normalStride = -1;
                if (normalIndex > -1)
                {
                    normalAccessor = gltfData.accessors[normalIndex];
                    normalBufferView = gltfData.bufferViews[normalAccessor.bufferView];
                    normalStride = tinygltf::GetComponentSizeInBytes(normalAccessor.componentType) * tinygltf::GetNumComponentsInType(normalAccessor.type);
                    assert(normalStride == 12);

                    const tinygltf::Buffer& normalBuffer = gltfData.buffers[normalBufferView.buffer];
                    normalBufferAddress = normalBuffer.data.data();
                }

                // Vertex tangents
                tinygltf::Accessor tangentAccessor;
                tinygltf::BufferView tangentBufferView;
                const uint8_t* tangentBufferAddress = nullptr;
                int tangentStride = -1;
                if (tangentIndex > -1)
                {
                    tangentAccessor = gltfData.accessors[tangentIndex];
                    tangentBufferView = gltfData.bufferViews[tangentAccessor.bufferView];
                    tangentStride = tinygltf::GetComponentSizeInBytes(tangentAccessor.componentType) * tinygltf::GetNumComponentsInType(tangentAccessor.type);
                    assert(tangentStride == 16);

                    const tinygltf::Buffer& tangentBuffer = gltfData.buffers[tangentBufferView.buffer];
                    tangentBufferAddress = tangentBuffer.data.data();
                }

                // Vertex texture coordinates
                tinygltf::Accessor uv0Accessor;
                tinygltf::BufferView uv0BufferView;
                const uint8_t* uv0BufferAddress = nullptr;
                int uv0Stride = -1;
                if (uv0Index > -1)
                {
                    uv0Accessor = gltfData.accessors[uv0Index];
                    uv0BufferView = gltfData.bufferViews[uv0Accessor.bufferView];
                    uv0Stride = tinygltf::GetComponentSizeInBytes(uv0Accessor.componentType) * tinygltf::GetNumComponentsInType(uv0Accessor.type);
                    assert(uv0Stride == 8);

                    const tinygltf::Buffer& uv0Buffer = gltfData.buffers[uv0BufferView.buffer];
                    uv0BufferAddress = uv0Buffer.data.data();
                }

                // Get the vertex data
                for (uint32_t vertexIndex = 0; vertexIndex < static_cast<uint32_t>(positionAccessor.count); vertexIndex++)
                {
                    Graphics::Vertex v;

                    const uint8_t* address = positionBufferAddress + positionBufferView.byteOffset + positionAccessor.byteOffset + (vertexIndex * positionStride);
                    memcpy(&v.position, address, positionStride);

                #if (COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT) || (COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT_Z_UP)
                    // Invert z-coordinate to convert from right hand to left hand
                    v.position.z *= -1.f;
                #endif

                #if COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT_Z_UP
                    // Convert to left hand, z-up (unreal)
                    v.position = { v.position.z, v.position.x, v.position.y };
                #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT_Z_UP
                    // Convert to right hand, z-up
                    v.position = { v.position.x, -v.position.z, v.position.y };
                #endif

                    if (normalIndex > -1)
                    {
                        address = normalBufferAddress + normalBufferView.byteOffset + normalAccessor.byteOffset + (vertexIndex * normalStride);
                        memcpy(&v.normal, address, normalStride);

                    #if (COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT) || (COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT_Z_UP)
                        // Invert the z-coordinate to convert from right hand to left hand
                        v.normal.z *= -1.f;
                    #endif

                    #if COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT_Z_UP
                        // Convert to left hand, z-up (unreal)
                        v.normal = { v.normal.z, v.normal.x, v.normal.y };
                    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT_Z_UP
                        // Convert to right hand, z-up
                        v.normal = { v.normal.x, -v.normal.z, v.normal.y };
                    #endif
                    }

                    if (tangentIndex > -1)
                    {
                        address = tangentBufferAddress + tangentBufferView.byteOffset + tangentAccessor.byteOffset + (vertexIndex * tangentStride);
                        memcpy(&v.tangent, address, tangentStride);

                    #if COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT || COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT_Z_UP
                        // Invert the z-coordinate to convert from right hand to left hand
                        v.tangent.z *= -1.f;
                    #endif

                    #if COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT_Z_UP
                        // Convert to left hand, z-up (unreal)
                        v.tangent = { v.tangent.z, v.tangent.x, v.tangent.y, v.tangent.w };
                    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT_Z_UP
                        // Convert to right hand, z-up
                        v.tangent = { v.tangent.x, -v.tangent.z, v.tangent.y, v.tangent.w };
                    #endif
                    }

                    if (uv0Index > -1)
                    {
                        address = uv0BufferAddress + uv0BufferView.byteOffset + uv0Accessor.byteOffset + (vertexIndex * uv0Stride);
                        memcpy(&v.uv0, address, uv0Stride);
                    }

                    // Update the mesh primitive's bounding box
                    mp.boundingBox.min = rtxgi::Min(mp.boundingBox.min, v.position);
                    mp.boundingBox.max = rtxgi::Max(mp.boundingBox.max, v.position);

                    mp.vertices.push_back(v);
                    mesh.numVertices++;
                }

                // Get the index data
                // Indices can be either unsigned char, unsigned short, or unsigned long
                // Converting to full precision for easy use on GPU
                const uint8_t* baseAddress = indexBufferAddress + indexBufferView.byteOffset + indexAccessor.byteOffset;
                if (indexStride == 1)
                {
                    std::vector<uint8_t> quarter;
                    quarter.resize(indexAccessor.count);

                    memcpy(quarter.data(), baseAddress, (indexAccessor.count * indexStride));

                    // Convert quarter precision indices to full precision
                    for (size_t i = 0; i < indexAccessor.count; i++)
                    {
                        mp.indices[i] = quarter[i];
                    }
                }
                else if (indexStride == 2)
                {
                    std::vector<uint16_t> half;
                    half.resize(indexAccessor.count);

                    memcpy(half.data(), baseAddress, (indexAccessor.count * indexStride));

                    // Convert half precision indices to full precision
                    for (size_t i = 0; i < indexAccessor.count; i++)
                    {
                        mp.indices[i] = half[i];
                    }
                }
                else
                {
                    memcpy(mp.indices.data(), baseAddress, (indexAccessor.count * indexStride));
                }

                // Update byte offsets
                vertexByteOffset += static_cast<uint32_t>(mp.vertices.size()) * sizeof(Graphics::Vertex);
                indexByteOffset += static_cast<uint32_t>(mp.indices.size()) * sizeof(UINT);

                // Increment the triangle count
                mesh.numIndices += static_cast<int>(indexAccessor.count);
                scene.numTriangles += mesh.numIndices / 3;

                // Update the mesh's bounding box
                mesh.boundingBox.min = rtxgi::Min(mesh.boundingBox.min, mp.boundingBox.min);
                mesh.boundingBox.max = rtxgi::Max(mesh.boundingBox.max, mp.boundingBox.max);

                // Add the mesh primitive
                mesh.primitives.push_back(mp);

                geometryIndex++;
            }

            mesh.index = static_cast<int>(scene.meshes.size());
            scene.meshes.push_back(mesh);
        }

        scene.numMeshPrimitives = geometryIndex;
    }

    /**
     * Update the scene mesh's bounding boxes, accounting for the mesh instances and instance transforms.
     */
    void UpdateSceneBoundingBoxes(Scene& scene)
    {
        // Initialize the scene bounding box
        scene.boundingBox.min = { FLT_MAX, FLT_MAX };
        scene.boundingBox.max = { -FLT_MAX, -FLT_MAX };

        XMFLOAT4 r3  = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
        for(uint32_t instanceIndex = 0; instanceIndex < static_cast<uint32_t>(scene.instances.size()); instanceIndex++)
        {
            // Get the mesh instance and mesh
            Scenes::MeshInstance& instance = scene.instances[instanceIndex];
            const Scenes::Mesh& mesh = scene.meshes[instance.meshIndex];

            // Initialize the mesh instance bounding box
            instance.boundingBox.min = { FLT_MAX, FLT_MAX };
            instance.boundingBox.max = { -FLT_MAX, -FLT_MAX };

            // Instance transform rows
            XMFLOAT4 instanceXformR0 = XMFLOAT4(&instance.transform[0][0]);
            XMFLOAT4 instanceXformR1 = XMFLOAT4(&instance.transform[1][0]);
            XMFLOAT4 instanceXformR2 = XMFLOAT4(&instance.transform[2][0]);

            // Instance transform matrix
            XMMATRIX xform;
            xform.r[0] = DirectX::XMLoadFloat4(&instanceXformR0);
            xform.r[1] = DirectX::XMLoadFloat4(&instanceXformR1);
            xform.r[2] = DirectX::XMLoadFloat4(&instanceXformR2);
            xform.r[3] = DirectX::XMLoadFloat4(&r3);

            // Remove the transpose (transforms are transposed for copying to GPU)
            XMMATRIX transpose = XMMatrixTranspose(xform);

            // Get the mesh bounding box vertices
            XMFLOAT3 min = XMFLOAT3(mesh.boundingBox.min.x, mesh.boundingBox.min.y, mesh.boundingBox.min.z);
            XMFLOAT3 max = XMFLOAT3(mesh.boundingBox.max.x, mesh.boundingBox.max.y, mesh.boundingBox.max.z);

            // Transform the mesh bounding box vertices
            DirectX::XMStoreFloat3(&min, XMVector3Transform(DirectX::XMLoadFloat3(&min), transpose));
            DirectX::XMStoreFloat3(&max, XMVector3Transform(DirectX::XMLoadFloat3(&max), transpose));

            // Update the mesh instance bounding box
            instance.boundingBox.min = rtxgi::Min(instance.boundingBox.min, { max.x, max.y, max.z });
            instance.boundingBox.min = rtxgi::Min(instance.boundingBox.min, { min.x, min.y, min.z });
            instance.boundingBox.max = rtxgi::Max(instance.boundingBox.max, { max.x, max.y, max.z });
            instance.boundingBox.max = rtxgi::Max(instance.boundingBox.max, { min.x, min.y, min.z });

            // Update the scene bounding box
            scene.boundingBox.min = rtxgi::Min(scene.boundingBox.min, instance.boundingBox.min);
            scene.boundingBox.max = rtxgi::Max(scene.boundingBox.min, instance.boundingBox.max);
        }
    }

    /**
     * Parse the various data of a GLTF file.
     */
    bool ParseGLTF(const tinygltf::Model& gltfData, const Configs::Config& config, const bool binary, Scene& scene, std::ofstream& log)
    {
        if (binary && gltfData.textures.size() > 0)
        {
            std::string msg = "\nFailed to load scene file! GLB format not supported for scenes with texture data. Use the *.gltf file format instead\n.";
            Graphics::UI::MessageBox(msg);
            return false;
        }

    #if defined(__aarch64__) || defined(_M_ARM64)
        if(gltfData.textures.size() > 0)
        {
            std::string msg = "\nScene texture mipmapping and compression not supported on ARM64. Load a scene cache file instead of *.gltf!\n";
            Graphics::UI::MessageBox(msg);
            return false;
        }
    #endif

        // Parse Cameras
        ParseGLTFCameras(gltfData, scene);

        // Parse Nodes
        ParseGLTFNodes(gltfData, scene);

        // Parse Materials
        ParseGLTFMaterials(gltfData, scene);

        // Parse and Load Textures
        if (!ParseGLFTextures(gltfData, config, scene)) return false;

        // Parse Meshes
        ParseGLTFMeshes(gltfData, scene);

        // Update the scene's bounding boxes, based on the instance transforms
        UpdateSceneBoundingBoxes(scene);

        return true;
    }

    /**
     * Adds config specific cameras and lights.
     */
    void ParseConfigCamerasLights(const Configs::Config& config, Scene& scene)
    {
        // Add scene lights from config file
        std::vector<Light> spotLights;
        std::vector<Light> pointLights;
        scene.lights.resize(config.scene.lights.size());
        for (uint32_t lightIndex = 0; lightIndex < static_cast<uint32_t>(config.scene.lights.size()); lightIndex++)
        {
            const Configs::Light& light = config.scene.lights[lightIndex];

            if (light.type == ELightType::DIRECTIONAL)
            {
                scene.hasDirectionalLight = 1;
                scene.lights[0].name = light.name;
                scene.lights[0].type = light.type;
                scene.lights[0].data.power = light.power;
                scene.lights[0].data.color = { light.color.x, light.color.y, light.color.z };
                scene.lights[0].data.direction = { light.direction.x, light.direction.y, light.direction.z };
                continue;
            }

            if (light.type == ELightType::SPOT)
            {
                spotLights.emplace_back();
                spotLights.back().name = light.name;
                spotLights.back().type = light.type;
                spotLights.back().data.power = light.power;
                spotLights.back().data.color = { light.color.x, light.color.y, light.color.z };
                spotLights.back().data.position = { light.position.x, light.position.y, light.position.z };
                spotLights.back().data.direction = { light.direction.x, light.direction.y, light.direction.z };
                spotLights.back().data.radius = light.radius;
                spotLights.back().data.umbraAngle = light.umbraAngle;
                spotLights.back().data.penumbraAngle = light.penumbraAngle;
                continue;
            }

            if (light.type == ELightType::POINT)
            {
                pointLights.emplace_back();
                pointLights.back().name = light.name;
                pointLights.back().type = light.type;
                pointLights.back().data.power = light.power;
                pointLights.back().data.color = { light.color.x, light.color.y, light.color.z };
                pointLights.back().data.position = { light.position.x, light.position.y, light.position.z };
                pointLights.back().data.radius = light.radius;
            }

        }

        uint32_t lightIndex;
        scene.numSpotLights = static_cast<uint32_t>(spotLights.size());
        scene.firstSpotLight = scene.hasDirectionalLight;
        for (lightIndex = 0; lightIndex < scene.numSpotLights; lightIndex++)
        {
            scene.lights[scene.hasDirectionalLight] = spotLights[lightIndex];
        }

        scene.numPointLights = static_cast<uint32_t>(pointLights.size());
        scene.firstPointLight = scene.hasDirectionalLight + scene.numSpotLights;
        for (lightIndex = 0; lightIndex < scene.numPointLights; lightIndex++)
        {
            scene.lights[(scene.hasDirectionalLight + scene.numSpotLights)] = pointLights[lightIndex];
        }

        // Add scene cameras from config file
        scene.cameras.resize(config.scene.cameras.size());
        for (uint32_t cameraIndex = 0; cameraIndex < static_cast<uint32_t>(config.scene.cameras.size()); cameraIndex++)
        {
            const Configs::Camera& camera = config.scene.cameras[cameraIndex];

            scene.cameras[cameraIndex].name = camera.name;
            scene.cameras[cameraIndex].data.fov = camera.fov;
            scene.cameras[cameraIndex].data.tanHalfFovY = std::tan(camera.fov * (XM_PI / 180.f) * 0.5f);
            scene.cameras[cameraIndex].data.aspect = camera.aspect;
            scene.cameras[cameraIndex].data.position = { camera.position.x, camera.position.y, camera.position.z };
            scene.cameras[cameraIndex].pitch = camera.pitch;
            scene.cameras[cameraIndex].yaw = camera.yaw;

            UpdateCamera(scene.cameras[cameraIndex]);
        }
    }

    //----------------------------------------------------------------------------------------------------------
    // Public Functions
    //----------------------------------------------------------------------------------------------------------

    /**
     * Loads and parses a glTF 2.0 scene.
     */
    bool Initialize(const Configs::Config& config, Scene& scene, std::ofstream& log)
    {
        // Set the scene name
        scene.name = config.scene.name;

        // Check for valid file formats
        bool binary = false;
        const std::regex glbFile("^[\\w-]+\\.glb$");
        const std::regex gltfFile("^[\\w-]+\\.gltf$");
        if (std::regex_match(config.scene.file, glbFile)) binary = true;
        else if (std::regex_match(config.scene.file, gltfFile)) binary = false;
        else
        {
            // Unknown file format
            std::string msg = "Unknown file format \'" + config.scene.file + "'";
            Graphics::UI::MessageBox(msg);
            return false;
        }

        // Construct the cache file name
        std::string cacheName = "";
        if(binary)
        {
            const std::regex glbExtension("\\.glb$");
            std::regex_replace(back_inserter(cacheName), config.scene.file.begin(), config.scene.file.end(), glbExtension, "");
        }
        else
        {
            const std::regex gltfExtension("\\.gltf$");
            std::regex_replace(back_inserter(cacheName), config.scene.file.begin(), config.scene.file.end(), gltfExtension, "");
        }

        // Load the scene cache file, if it exists
        std::string sceneCache = config.app.root + config.scene.path + cacheName + ".cache";
        if (Caches::Deserialize(sceneCache, scene, log))
        {
            ParseConfigCamerasLights(config, scene);
            return true;
        }

        // Load the scene GLTF (no cache file exists or the existing cache file is invalid)
        tinygltf::Model gltfData;
        tinygltf::TinyGLTF gltfLoader;
        std::string err, warn, filepath;

        // Build the path to the GLTF file
        filepath = config.app.root + config.scene.path + config.scene.file;

        // Load the scene
        bool result = false;
        if(binary) result = gltfLoader.LoadBinaryFromFile(&gltfData, &err, &warn, filepath);
        else result = gltfLoader.LoadASCIIFromFile(&gltfData, &err, &warn, filepath);

        if (!result)
        {
            // An error occurred
            std::string msg = std::string(err.begin(), err.end());
            Graphics::UI::MessageBox(msg);
            return false;
        }
        else if (warn.length() > 0)
        {
            // Warning
            std::string msg = std::string(warn.begin(), warn.end());
            Graphics::UI::MessageBox(msg);
            return false;
        }

        // Parse the GLTF data
        CHECK(ParseGLTF(gltfData, config, binary, scene, log), "parse scene file!\n", log);

        // Serialize the scene and store a cache file to speed up future loads
        if (!Caches::Serialize(sceneCache, scene, log)) return false;

        // Add config specific cameras and lights
        ParseConfigCamerasLights(config, scene);

        return true;
    }

    /**
     * Traverse the scene graph and update the instance transforms.
     */
    void Traverse(size_t nodeIndex, XMMATRIX transform, Scene& scene)
    {
        // Get the node
        SceneNode node = scene.nodes[nodeIndex];

        // Get the node's local transform
        XMMATRIX nodeTransform;
        if(node.hasMatrix)
        {
            nodeTransform = node.matrix;
        }
        else
        {
            // Compose the node's local transform, M = T * R * S
            XMMATRIX t = XMMatrixTranslation(node.translation.x, node.translation.y, node.translation.z);
            XMMATRIX r = XMMatrixRotationQuaternion(XMLoadFloat4(&node.rotation));
            XMMATRIX s = XMMatrixScaling(node.scale.x, node.scale.y, node.scale.z);    // Note: do not use negative scale factors! This will flip the object inside and cause incorrect normals.
            nodeTransform = XMMatrixMultiply(XMMatrixMultiply(s, r), t);
        }

        // Compose the global transform
        transform = XMMatrixMultiply(nodeTransform, transform);

        // When at a leaf node with a mesh, update the mesh instance's transform
        // Not currently supporting nested transforms for camera nodes
        if (node.children.size() == 0 && node.instance > -1)
        {
            // Update the instance's transform data
            MeshInstance* instance = &scene.instances[node.instance];
            XMMATRIX transpose = XMMatrixTranspose(transform);
            memcpy(instance->transform, &transpose, sizeof(XMFLOAT4) * 3);
            return;
        }

        // Recursively traverse the scene graph
        for (size_t i = 0; i < node.children.size(); i++)
        {
            Traverse(node.children[i], transform, scene);
        }
    }

    /**
     * Update the given camera based on the provided pitch and yaw values.
     */
    void UpdateCamera(Camera& camera)
    {
    #if COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT
        XMFLOAT3 up = XMFLOAT3(0.f, 1.f, 0.f);
        XMFLOAT3 forward = XMFLOAT3(0.f, 0.f, 1.f);
        XMMATRIX rotation = XMMatrixRotationRollPitchYaw(camera.pitch * (XM_PI / 180.f), camera.yaw * (XM_PI / 180.f), 0.f);
    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT
        XMFLOAT3 up = XMFLOAT3(0.f, 1.f, 0.f);
        XMFLOAT3 forward = XMFLOAT3(0.f, 0.f, -1.f);
        XMMATRIX rotation = XMMatrixRotationRollPitchYaw(-camera.pitch * (XM_PI / 180.f), -camera.yaw * (XM_PI / 180.f), 0.f);
    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT_Z_UP
        XMFLOAT3 up = XMFLOAT3(0.f, 0.f, 1.f);
        XMFLOAT3 forward = XMFLOAT3(1.f, 0.f, 0.f);
        XMMATRIX rotationY = XMMatrixRotationY(camera.pitch * (XM_PI / 180.f));
        XMMATRIX rotationZ = XMMatrixRotationZ(camera.yaw * (XM_PI / 180.f));
        XMMATRIX rotation = XMMatrixMultiply(rotationY, rotationZ);
    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT_Z_UP
        XMFLOAT3 up = XMFLOAT3(0.f, 0.f, 1.f);
        XMFLOAT3 forward = XMFLOAT3(0.f, 1.f, 0.f);
        XMMATRIX rotationX = XMMatrixRotationX(-camera.pitch * (XM_PI / 180.f));
        XMMATRIX rotationZ = XMMatrixRotationZ(-camera.yaw * (XM_PI / 180.f));
        XMMATRIX rotation = XMMatrixMultiply(rotationX, rotationZ);
    #endif

        XMFLOAT3 cameraRight, cameraUp, cameraForward;
        XMStoreFloat3(&cameraForward, XMVector3Normalize(XMVector3Transform(XMLoadFloat3(&forward), rotation)));
    #if (COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT) || (COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT_Z_UP)
        XMStoreFloat3(&cameraRight, XMVector3Normalize(XMVector3Cross(XMLoadFloat3(&cameraForward), XMLoadFloat3(&up))));
        XMStoreFloat3(&cameraUp, XMVector3Cross(-XMLoadFloat3(&cameraForward), XMLoadFloat3(&cameraRight)));
    #else
        XMStoreFloat3(&cameraRight, XMVector3Normalize(-XMVector3Cross(XMLoadFloat3(&cameraForward), XMLoadFloat3(&up))));
        XMStoreFloat3(&cameraUp, XMVector3Cross(XMLoadFloat3(&cameraForward), XMLoadFloat3(&cameraRight)));
    #endif

        camera.data.right = { cameraRight.x, cameraRight.y, cameraRight.z };
        camera.data.up = { cameraUp.x, cameraUp.y, cameraUp.z };
        camera.data.forward = { cameraForward.x, cameraForward.y, cameraForward.z };
    }

    /**
     * Releases memory used by the scene.
     */
    void Cleanup(Scene& scene)
    {
        // Release texture memory
        for (size_t textureIndex = 0; textureIndex < scene.textures.size(); textureIndex++)
        {
            Textures::Unload(scene.textures[textureIndex]);
        }
    }

}
