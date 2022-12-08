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

using namespace DirectX;

#define SCENE_CACHE_VERSION 4

namespace Caches
{

    //----------------------------------------------------------------------------------------------------------
    // Private Deserialization Functions
    //----------------------------------------------------------------------------------------------------------

    void Read(std::ifstream& in, uint32_t* value)
    {
        in.read((char*)value, sizeof(uint32_t));
        in.seekg(in.tellg());
    }

    void Read(std::ifstream& in, void* value, size_t size)
    {
        in.read((char*)value, size);
        in.seekg(in.tellg());
    }

    void ReadTexture(std::ifstream& in, Textures::Texture& texture)
    {
        // Texture name
        uint32_t numChars;
        Read(in, &numChars);

        char* buffer = new char[numChars];
        in.read(buffer, numChars);
        in.seekg(in.tellg());
        texture.name = std::string(buffer);
        delete[] buffer;

        // Texture filepath
        Read(in, &numChars);

        buffer = new char[numChars];
        in.read(buffer, numChars);
        in.seekg(in.tellg());
        texture.filepath = std::string(buffer);
        delete[] buffer;

        // Texture metadata
        Read(in, &texture.type, sizeof(Textures::ETextureType));
        Read(in, &texture.format, sizeof(Textures::ETextureFormat));
        Read(in, &texture.width);
        Read(in, &texture.height);
        Read(in, &texture.stride);
        Read(in, &texture.mips);
        Read(in, &texture.texelBytes, sizeof(uint64_t));

        texture.texels = new uint8_t[texture.texelBytes];
        memset(texture.texels, 0, texture.texelBytes);
        Read(in, texture.texels, texture.texelBytes);

        texture.cached = true;
    }

    void ReadMaterial(std::ifstream& in, Scenes::Material& material)
    {
        uint32_t numChars = 0;
        Read(in, &numChars);

        char* buffer = new char[numChars];
        in.read(buffer, numChars);
        in.seekg(in.tellg());
        material.name = std::string(buffer);
        delete[] buffer;

        Read(in, &material.data, material.GetGPUDataSize());
    }

    void ReadMesh(std::ifstream& in, Scenes::Mesh& mesh)
    {
        uint32_t numChars = 0;
        Read(in, &numChars);

        char* buffer = new char[numChars];
        in.read(buffer, numChars);
        in.seekg(in.tellg());
        mesh.name = std::string(buffer);
        delete[] buffer;

        Read(in, &mesh.index, sizeof(uint32_t));
        Read(in, &mesh.numIndices, sizeof(uint32_t));
        Read(in, &mesh.numVertices, sizeof(uint32_t));

        // Read mesh bounding box
        Read(in, &mesh.boundingBox, sizeof(rtxgi::AABB));

        // Read MeshPrimitives
        uint32_t numPrimitives;
        Read(in, &numPrimitives);
        mesh.primitives.resize(numPrimitives);
        for (uint32_t primitiveIndex = 0; primitiveIndex < numPrimitives; primitiveIndex++)
        {
            uint32_t numVertices = 0;
            uint32_t numIndices = 0;

            Scenes::MeshPrimitive& mp = mesh.primitives[primitiveIndex];

            // Read the mesh primitive data
            Read(in, &mp.index, sizeof(int));
            Read(in, &mp.material, sizeof(int));
            Read(in, &mp.opaque, sizeof(bool));
            Read(in, &mp.doubleSided, sizeof(bool));
            Read(in, &mp.indexByteOffset, sizeof(uint32_t));
            Read(in, &mp.vertexByteOffset, sizeof(uint32_t));
            Read(in, &mp.boundingBox, sizeof(rtxgi::AABB)); // post-transform bounding box

            Read(in, &numVertices);
            mp.vertices.resize(numVertices);
            Read(in, mp.vertices.data(), sizeof(Graphics::Vertex) * numVertices);

            Read(in, &numIndices);
            mp.indices.resize(numIndices);
            Read(in, mp.indices.data(), sizeof(uint32_t) * numIndices);

            // Update the mesh bounding box
            mesh.boundingBox.min = { fmin(mesh.boundingBox.min.x, mp.boundingBox.min.x), fmin(mesh.boundingBox.min.y, mp.boundingBox.min.y) };
            mesh.boundingBox.max = { fmax(mesh.boundingBox.max.x, mp.boundingBox.max.x), fmax(mesh.boundingBox.max.y, mp.boundingBox.max.y) };
        }
    }

    void ReadMeshInstance(std::ifstream& in, Scenes::MeshInstance& instance)
    {
        uint32_t numChars = 0;
        Read(in, &numChars);

        char* buffer = new char[numChars];
        in.read(buffer, numChars);
        in.seekg(in.tellg());
        instance.name = std::string(buffer);
        delete[] buffer;

        Read(in, &instance.meshIndex, sizeof(int));
        Read(in, &instance.boundingBox, sizeof(rtxgi::AABB));
        Read(in, &instance.transform, sizeof(float) * 12);
    }

    void ReadLight(std::ifstream& in, Scenes::Light& light)
    {
        uint32_t numChars = 0;
        Read(in, &numChars);

        char* buffer = new char[numChars];
        in.read(buffer, numChars);
        in.seekg(in.tellg());
        light.name = std::string(buffer);
        delete[] buffer;

        Read(in, &light.data, light.GetGPUDataSize());
    }

    void ReadCamera(std::ifstream& in, Scenes::Camera& camera)
    {
        uint32_t numChars = 0;
        Read(in, &numChars);

        char* buffer = new char[numChars];
        in.read(buffer, numChars);
        in.seekg(in.tellg());
        camera.name = std::string(buffer);
        delete[] buffer;

        Read(in, &camera.data, camera.GetGPUDataSize());
    }

    void ReadSceneNode(std::ifstream& in, Scenes::SceneNode& node)
    {
        Read(in, &node.instance, sizeof(int));
        Read(in, &node.camera, sizeof(int));
        Read(in, &node.translation, sizeof(XMFLOAT3));
        Read(in, &node.rotation, sizeof(XMFLOAT4));
        Read(in, &node.scale, sizeof(XMFLOAT3));

        // Read child node indices
        uint32_t numChildren;
        Read(in, &numChildren);
        if (numChildren <= 0) return;

        node.children.resize(numChildren);
        Read(in, node.children.data(), (sizeof(int) * numChildren));
    }

    //----------------------------------------------------------------------------------------------------------
    // Private Serialization Functions
    //----------------------------------------------------------------------------------------------------------

    void Write(std::ofstream& out, void* value, size_t size = sizeof(uint32_t))
    {
        out.write(reinterpret_cast<char*>(value), size);
        out.seekp(out.tellp());
    }

    void WriteTexture(std::ofstream& out, Textures::Texture& texture)
    {
        // Texture name
        uint32_t numChars = static_cast<uint32_t>(strlen(texture.name.c_str())) + 1;
        Write(out, &numChars);
        out.write(texture.name.c_str(), numChars);
        out.seekp(out.tellp());

        // Texture filepath
        numChars = static_cast<uint32_t>(strlen(texture.filepath.c_str())) + 1;
        Write(out, &numChars);
        out.write(texture.filepath.c_str(), numChars);
        out.seekp(out.tellp());

        // Texture metadata
        Write(out, &texture.type);
        Write(out, &texture.format);
        Write(out, &texture.width);
        Write(out, &texture.height);
        Write(out, &texture.stride);
        Write(out, &texture.mips);
        Write(out, &texture.texelBytes, sizeof(uint64_t));

        // Texels
        Write(out, texture.texels, texture.texelBytes);
    }

    void WriteMaterial(std::ofstream& out, Scenes::Material& material)
    {
        uint32_t numChars = static_cast<uint32_t>(strlen(material.name.c_str())) + 1;
        Write(out, &numChars);
        out.write(material.name.c_str(), numChars);
        out.seekp(out.tellp());

        Write(out, &material.data, material.GetGPUDataSize());
    }

    void WriteMesh(std::ofstream& out, Scenes::Mesh& mesh)
    {
        uint32_t numChars = static_cast<uint32_t>(strlen(mesh.name.c_str())) + 1;
        Write(out, &numChars);
        out.write(mesh.name.c_str(), numChars);
        out.seekp(out.tellp());

        Write(out, &mesh.index, sizeof(uint32_t));
        Write(out, &mesh.numIndices, sizeof(uint32_t));
        Write(out, &mesh.numVertices, sizeof(uint32_t));

        // Mesh bounding box
        Write(out, &mesh.boundingBox, sizeof(rtxgi::AABB));

        // Write MeshPrimitives
        uint32_t numPrimitives = static_cast<uint32_t>(mesh.primitives.size());
        Write(out, &numPrimitives);
        for (uint32_t primitiveIndex = 0; primitiveIndex < numPrimitives; primitiveIndex++)
        {
            Scenes::MeshPrimitive primitive = mesh.primitives[primitiveIndex];
            uint32_t numVertices = static_cast<uint32_t>(primitive.vertices.size());
            uint32_t numIndices = static_cast<uint32_t>(primitive.indices.size());
            Write(out, &primitive.index, sizeof(int));
            Write(out, &primitive.material, sizeof(int));
            Write(out, &primitive.opaque, sizeof(bool));
            Write(out, &primitive.doubleSided, sizeof(bool));
            Write(out, &primitive.indexByteOffset, sizeof(uint32_t));
            Write(out, &primitive.vertexByteOffset, sizeof(uint32_t));
            Write(out, &primitive.boundingBox, sizeof(rtxgi::AABB));
            Write(out, &numVertices);
            Write(out, primitive.vertices.data(), sizeof(Graphics::Vertex) * numVertices);
            Write(out, &numIndices);
            Write(out, primitive.indices.data(), sizeof(uint32_t) * numIndices);
        }

    }

    void WriteMeshInstance(std::ofstream& out, Scenes::MeshInstance& instance)
    {
        uint32_t numChars = static_cast<uint32_t>(strlen(instance.name.c_str())) + 1;
        Write(out, &numChars);
        out.write(instance.name.c_str(), numChars);
        out.seekp(out.tellp());

        Write(out, &instance.meshIndex, sizeof(int));
        Write(out, &instance.boundingBox, sizeof(rtxgi::AABB));
        Write(out, &instance.transform, sizeof(float) * 12);
    }

    void WriteLight(std::ofstream& out, Scenes::Light& light)
    {
        uint32_t numChars = static_cast<uint32_t>(strlen(light.name.c_str())) + 1;
        Write(out, &numChars);
        out.write(light.name.c_str(), numChars);
        out.seekp(out.tellp());

        Write(out, &light.data, light.GetGPUDataSize());
    }

    void WriteCamera(std::ofstream& out, Scenes::Camera& camera)
    {
        uint32_t numChars = static_cast<uint32_t>(strlen(camera.name.c_str())) + 1;
        Write(out, &numChars);
        out.write(camera.name.c_str(), numChars);
        out.seekp(out.tellp());

        Write(out, &camera.data, camera.GetGPUDataSize());
    }

    void WriteSceneNode(std::ofstream& out, Scenes::SceneNode& node)
    {
        Write(out, &node.instance, sizeof(int));
        Write(out, &node.camera, sizeof(int));
        Write(out, &node.translation, sizeof(XMFLOAT3));
        Write(out, &node.rotation, sizeof(XMFLOAT4));
        Write(out, &node.scale, sizeof(XMFLOAT3));

        // Write children node indices
        uint32_t numChildren = static_cast<uint32_t>(node.children.size());
        Write(out, &numChildren);
        Write(out, node.children.data(), (sizeof(int) * numChildren));
    }

    //----------------------------------------------------------------------------------------------------------
    // Public Functions
    //----------------------------------------------------------------------------------------------------------

    /**
     * Write the scene cache file to disk.
     */
    bool Serialize(const std::string& filepath, Scenes::Scene& scene, std::ofstream& log)
    {
        std::ofstream out;
        out.open(filepath, std::ios::out | std::ios::binary);
        if (out.is_open())
        {
            log << "\n\tWriting scene cache file \'" + filepath + "\'...";

            out.seekp(0, std::ios::beg);

            // Header
            uint32_t cacheVersion = SCENE_CACHE_VERSION;
            Write(out, &cacheVersion);

            uint32_t coordinateSystem = COORDINATE_SYSTEM;
            Write(out, &coordinateSystem);

            Write(out, &scene.activeCamera);
            Write(out, &scene.numMeshPrimitives);
            Write(out, &scene.numTriangles);
            Write(out, &scene.hasDirectionalLight);
            Write(out, &scene.numPointLights);
            Write(out, &scene.numSpotLights);

            Write(out, &scene.boundingBox, sizeof(rtxgi::AABB));

            // Root Nodes
            uint32_t numElements = static_cast<uint32_t>(scene.rootNodes.size());
            Write(out, &numElements);
            if (numElements > 0)
            {
                Write(out, scene.rootNodes.data(), sizeof(int) * numElements);
            }

            // Scene Nodes
            numElements = static_cast<uint32_t>(scene.nodes.size());
            Write(out, &numElements);
            for (uint32_t nodeIndex = 0; nodeIndex < numElements; nodeIndex++)
            {
                WriteSceneNode(out, scene.nodes[nodeIndex]);
            }

            // Cameras
            numElements = static_cast<uint32_t>(scene.cameras.size());
            Write(out, &numElements);
            for (uint32_t cameraIndex = 0; cameraIndex < numElements; cameraIndex++)
            {
                WriteCamera(out, scene.cameras[cameraIndex]);
            }

            // Lights
            numElements = static_cast<uint32_t>(scene.lights.size());
            Write(out, &numElements);
            for (uint32_t lightIndex = 0; lightIndex < numElements; lightIndex++)
            {
                WriteLight(out, scene.lights[lightIndex]);
            }

            // MeshInstances
            numElements = static_cast<uint32_t>(scene.instances.size());
            Write(out, &numElements);
            for (uint32_t instanceIndex = 0; instanceIndex < numElements; instanceIndex++)
            {
                WriteMeshInstance(out, scene.instances[instanceIndex]);
            }

            // Meshes
            numElements = static_cast<uint32_t>(scene.meshes.size());
            Write(out, &numElements);
            for (uint32_t meshIndex = 0; meshIndex < numElements; meshIndex++)
            {
                WriteMesh(out, scene.meshes[meshIndex]);
            }

            // Materials
            numElements = static_cast<uint32_t>(scene.materials.size());
            Write(out, &numElements, sizeof(uint32_t));
            for (uint32_t materialIndex = 0; materialIndex < numElements; materialIndex++)
            {
                WriteMaterial(out, scene.materials[materialIndex]);
            }

            // Textures
            numElements = static_cast<uint32_t>(scene.textures.size());
            Write(out, &numElements, sizeof(uint32_t));
            for (uint32_t textureIndex = 0; textureIndex < numElements; textureIndex++)
            {
                WriteTexture(out, scene.textures[textureIndex]);
            }

            out.close();
            return true;
        }

        log << "\nFailed to write cache file \'" + filepath + "\'";
        return false;
    }

    /**
     * Read the scene cache file from disk.
     */
    bool Deserialize(const std::string& filepath, Scenes::Scene& scene, std::ofstream& log)
    {
        std::ifstream in;
        in.open(filepath, std::ios::in | std::ios::binary);
        if (in.is_open())
        {
            in.seekg(0, std::ios::beg);

            // Header
            uint32_t cacheVersion;
            Read(in, &cacheVersion, sizeof(uint32_t));
            if(cacheVersion != SCENE_CACHE_VERSION)
            {
                log << "\n\tWarning: scene cache version '" << cacheVersion << "' does not match expected version '" << SCENE_CACHE_VERSION << "'";
                log << "\n\tRebuilding scene cache...";
                return false;
            }

            uint32_t coordinateSystem;
            Read(in, &coordinateSystem, sizeof(uint32_t));
            if(coordinateSystem != COORDINATE_SYSTEM)
            {
                log << "\n\tWarning: scene cache coordinate system '" << GetCoordinateSystemName(coordinateSystem);
                log << "' does not match current coordinate system '" << GetCoordinateSystemName(COORDINATE_SYSTEM) << "'";
                log << "\n\tRebuilding scene cache...";
                return false;
            }

            Read(in, &scene.activeCamera);
            Read(in, &scene.numMeshPrimitives);
            Read(in, &scene.numTriangles);
            Read(in, &scene.hasDirectionalLight);
            Read(in, &scene.numPointLights);
            Read(in, &scene.numSpotLights);

            Read(in, &scene.boundingBox, sizeof(rtxgi::AABB));

            uint32_t numElements = 0;

            // Root Nodes
            Read(in, &numElements);
            if (numElements > 0)
            {
                scene.rootNodes.resize(numElements);
                Read(in, reinterpret_cast<char*>(scene.rootNodes.data()), sizeof(int) * numElements);
            }

            // Scene Nodes
            Read(in, &numElements);
            if (numElements > 0)
            {
                scene.nodes.resize(numElements);
                for (uint32_t nodeIndex = 0; nodeIndex < numElements; nodeIndex++)
                {
                    ReadSceneNode(in, scene.nodes[nodeIndex]);
                }
            }

            // Cameras
            Read(in, &numElements);
            if (numElements > 0)
            {
                scene.cameras.resize(numElements);
                for (uint32_t cameraIndex = 0; cameraIndex < numElements; cameraIndex++)
                {
                    ReadCamera(in, scene.cameras[cameraIndex]);
                }
            }

            // Lights
            Read(in, &numElements);
            if (numElements > 0)
            {
                scene.lights.resize(numElements);
                for (uint32_t lightIndex = 0; lightIndex < numElements; lightIndex++)
                {
                    ReadLight(in, scene.lights[lightIndex]);
                }
            }

            // MeshInstances
            Read(in, &numElements);
            if (numElements > 0)
            {
                scene.instances.resize(numElements);
                for (uint32_t instanceIndex = 0; instanceIndex < numElements; instanceIndex++)
                {
                    ReadMeshInstance(in, scene.instances[instanceIndex]);
                }
            }

            // Meshes
            Read(in, &numElements);
            if (numElements > 0)
            {
                scene.meshes.resize(numElements);
                for (uint32_t meshIndex = 0; meshIndex < numElements; meshIndex++)
                {
                    ReadMesh(in, scene.meshes[meshIndex]);
                }
            }

            // Materials
            Read(in, &numElements);
            if (numElements > 0)
            {
                scene.materials.resize(numElements);
                for (uint32_t materialIndex = 0; materialIndex < numElements; materialIndex++)
                {
                    ReadMaterial(in, scene.materials[materialIndex]);
                }
            }

            // Textures
            Read(in, &numElements);
            if (numElements > 0)
            {
                scene.textures.resize(numElements);
                for (uint32_t textureIndex = 0; textureIndex < numElements; textureIndex++)
                {
                    ReadTexture(in, scene.textures[textureIndex]);
                }
            }

            in.close();
            return true;
        }
        else
        {
            log << "\n\tWarning: no scene cache file exists!";
        }
        return false;
    }

}
