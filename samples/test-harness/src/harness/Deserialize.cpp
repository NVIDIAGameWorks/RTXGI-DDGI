#pragma once

#include "Deserialize.h"

#include <fstream>

#include <rtxgi/Defines.h>

using namespace std;

namespace Serialization
{

/**
* Read the mesh vertex and index data from the binary file.
*/
void ReadMesh(ifstream &in, RuntimeMesh &mesh)
{
    UINT vertexStride;
    UINT indexStride;

    Vertex* vertices = nullptr;
    UINT* indices = nullptr;

    // Vertex stride
    in.read((char*)&vertexStride, sizeof(UINT));
    in.seekg(in.tellg());

    // Index stride
    in.read((char*)&indexStride, sizeof(UINT));
    in.seekg(in.tellg());

    // Vertex data
    mesh.vertices = new Vertex[mesh.numVertices];
    in.read(reinterpret_cast<char*>(mesh.vertices), (mesh.numVertices * vertexStride));
    in.seekg(in.tellg());

    // Convert vertex data, if necessary
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    for (int i = 0; i < mesh.numVertices; i++)
    {
        mesh.vertices[i].position.z *= -1.f;
        mesh.vertices[i].normal.z *= -1.f;
    }
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_UNREAL
    for (int i = 0; i < mesh.numVertices; i++)
    {
        mesh.vertices[i] =
        {
            { mesh.vertices[i].position.z, mesh.vertices[i].position.x, mesh.vertices[i].position.y },
            { mesh.vertices[i].normal.z,   mesh.vertices[i].normal.x,   mesh.vertices[i].normal.y }
        };
    }
#endif

    // Index data
    mesh.indices = new unsigned int[mesh.numIndices];
    in.read(reinterpret_cast<char*>(mesh.indices), (mesh.numIndices * sizeof(UINT)));
    in.seekg(in.tellg());

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    for(int i = 0; i < mesh.numIndices; i+=3)
    {
        int v1 = mesh.indices[i + 1];
        int v2 = mesh.indices[i + 2];
        mesh.indices[i + 1] = v2;
        mesh.indices[i + 2] = v1;
    }
#endif

}

/**
* Read the mesh block from the binary file.
*/
void ReadMeshes(ifstream &in, UINT numMeshes, RuntimeGeometry &model)
{
    char* buffer = nullptr;
    for (UINT meshIndex = 0; meshIndex < numMeshes; meshIndex++)
    {
        RuntimeMesh mesh = {};

        // Number of vertices
        in.read((char*)&mesh.numVertices, sizeof(UINT));
        in.seekg(in.tellg());

        // Number of indices
        in.read((char*)&mesh.numIndices, sizeof(UINT));
        in.seekg(in.tellg());

        // Material index       
        in.read((char*)&mesh.materialIndex, sizeof(UINT));
        in.seekg(in.tellg());

        // Number of characters in the mesh name
        UINT numChars;
        in.read((char*)&numChars, sizeof(UINT));
        in.seekg(in.tellg());

        // Mesh name
        buffer = new char[numChars];
        in.read(buffer, numChars);
        in.seekg(in.tellg());
        mesh.name = string(buffer);
        delete[] buffer;

        // Read the vertex and index data
        ReadMesh(in, mesh);

        model.meshes.push_back(mesh);
    }
}

/**
* Read the material block from the binary file.
*/
void ReadMaterials(ifstream &in, UINT numMaterials, RuntimeGeometry &model)
{
    UINT length = 0;
    string materialName;
    char* buffer = nullptr;

    for (UINT materialIndex = 0; materialIndex < numMaterials; materialIndex++)
    {
        Material material = {};

        // Number of characters in the material name
        in.read((char*)&length, sizeof(UINT));
        in.seekg(in.tellg());

        // Material name
        buffer = new char[length];
        in.read(buffer, length);
        material.name = string(buffer);

        delete[] buffer;

        // Diffuse color
        in.seekg(in.tellg());
        in.read((char*)&material.color.x, sizeof(float));
        in.seekg(in.tellg());

        in.seekg(in.tellg());
        in.read((char*)&material.color.y, sizeof(float));
        in.seekg(in.tellg());

        in.seekg(in.tellg());
        in.read((char*)&material.color.z, sizeof(float));
        in.seekg(in.tellg());

        model.materials.push_back(material);
    }
}

/**
* Read the header block from the binary file.
*/
void ReadHeader(ifstream &in, UINT &numMaterials, UINT &numMeshes)
{
    // number of materials
    in.read((char*)&numMaterials, sizeof(UINT));
    in.seekg(in.tellg());

    // number of meshes 
    in.read((char*)&numMeshes, sizeof(UINT));
    in.seekg(in.tellg());
}

//--------------------------------------------------------------------------------------
// Public Functions
//--------------------------------------------------------------------------------------

HRESULT ReadBinary(string filePath, RuntimeGeometry &model)
{
    HRESULT hr = S_OK;

    ifstream in;
    in.open(filePath, ios::in | ios::binary);
    if (in.is_open())
    {
        in.seekg(0, ios::beg);

        UINT numMaterials, numMeshes;
        ReadHeader(in, numMaterials, numMeshes);
        ReadMaterials(in, numMaterials, model);
        ReadMeshes(in, numMeshes, model);

        in.close();
    }
    else
    {
        hr = E_FAIL;
    }

    return hr;
}

}
