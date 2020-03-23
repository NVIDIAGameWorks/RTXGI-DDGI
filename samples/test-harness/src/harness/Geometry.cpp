/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "Common.h"
#include "Deserialize.h"

#include <rtxgi/Defines.h>

const static UINT longitudes = 30;
const static UINT latitudes = 30;

//----------------------------------------------------------------------------------------------------------
// Private Functions
//----------------------------------------------------------------------------------------------------------

/**
* Generate the vertices for a Cornell Box.
*/
vector<Vertex> GetCornellVertices()
{
    vector<XMFLOAT3> positions;
    {
        // walls
        positions.insert(positions.end(), { -1.f, 0.f, -1.f });
        positions.insert(positions.end(), {  1.f, 0.f, -1.f });
        positions.insert(positions.end(), { -1.f, 0.f,  1.f });
        positions.insert(positions.end(), {  1.f, 0.f,  1.f });

        positions.insert(positions.end(), { -1.f, 2.f, -1.f });
        positions.insert(positions.end(), {  1.f, 2.f, -1.f });
        positions.insert(positions.end(), { -1.f, 2.f,  1.f });
        positions.insert(positions.end(), {  1.f, 2.f,  1.f });

        // short box
        positions.insert(positions.end(), { -0.05f, -0.02f, -0.57f });
        positions.insert(positions.end(), {  0.53f, -0.02f, -0.75f });
        positions.insert(positions.end(), {  0.13f, -0.02f, -0.00f });
        positions.insert(positions.end(), {  0.70f, -0.02f, -0.17f });

        positions.insert(positions.end(), { -0.05f, 0.6f, -0.57f });
        positions.insert(positions.end(), {  0.53f, 0.6f, -0.75f });
        positions.insert(positions.end(), {  0.13f, 0.6f, -0.00f });
        positions.insert(positions.end(), {  0.70f, 0.6f, -0.17f });

        // tall box
        positions.insert(positions.end(), { -0.71f, -0.02f,  0.49f });
        positions.insert(positions.end(), { -0.53f, -0.02f, -0.09f });
        positions.insert(positions.end(), { -0.14f, -0.02f,  0.67f });
        positions.insert(positions.end(), {  0.04f, -0.02f,  0.09f });

        positions.insert(positions.end(), { -0.71f, 1.20f,  0.49f });
        positions.insert(positions.end(), { -0.53f, 1.20f, -0.09f });
        positions.insert(positions.end(), { -0.14f, 1.20f,  0.67f });
        positions.insert(positions.end(), {  0.04f, 1.20f,  0.09f });
    }

    vector<XMFLOAT3> normals;
    {
        normals.insert(normals.end(), {  0.f,  0.f, -1.f });
        normals.insert(normals.end(), {  0.f,  0.f,  1.f });
        normals.insert(normals.end(), {  0.f, -1.f,  0.f });
        normals.insert(normals.end(), {  0.f,  1.f,  0.f });
        normals.insert(normals.end(), { -1.f,  0.f,  0.f });
        normals.insert(normals.end(), {  1.f,  0.f,  0.f });

        normals.insert(normals.end(), { -0.9536f, 0.f,  0.3011f });
        normals.insert(normals.end(), {  0.2858f, 0.f,  0.9583f });
        normals.insert(normals.end(), {  0.9596f, 0.f, -0.2813f });
        normals.insert(normals.end(), {  0.2964f, 0.f, -0.9551f });

        normals.insert(normals.end(), { -0.3011f, 0.f,  0.9536f });
        normals.insert(normals.end(), {  0.9551f, 0.f,  0.2964f });
        normals.insert(normals.end(), {  0.3011f, 0.f, -0.9536f });
        normals.insert(normals.end(), { -0.9551f, 0.f, -0.2964f });
    }

    vector<Vertex> vertices;
    // walls
    {
        // floor
        vertices.insert(vertices.end(), { positions[0], normals[3] });
        vertices.insert(vertices.end(), { positions[1], normals[3] });
        vertices.insert(vertices.end(), { positions[2], normals[3] });
        vertices.insert(vertices.end(), { positions[3], normals[3] });

        // left wall
        vertices.insert(vertices.end(), { positions[0], normals[5] });
        vertices.insert(vertices.end(), { positions[2], normals[5] });
        vertices.insert(vertices.end(), { positions[4], normals[5] });
        vertices.insert(vertices.end(), { positions[6], normals[5] });

        // back wall
        vertices.insert(vertices.end(), { positions[2], normals[0] });
        vertices.insert(vertices.end(), { positions[3], normals[0] });
        vertices.insert(vertices.end(), { positions[6], normals[0] });
        vertices.insert(vertices.end(), { positions[7], normals[0] });

        // right wall
        vertices.insert(vertices.end(), { positions[3], normals[4] });
        vertices.insert(vertices.end(), { positions[1], normals[4] });
        vertices.insert(vertices.end(), { positions[7], normals[4] });
        vertices.insert(vertices.end(), { positions[5], normals[4] });

        // ceiling
        vertices.insert(vertices.end(), { positions[5], normals[2] });
        vertices.insert(vertices.end(), { positions[4], normals[2] });
        vertices.insert(vertices.end(), { positions[7], normals[2] });
        vertices.insert(vertices.end(), { positions[6], normals[2] });

        // front wall
        /*vertices.insert(vertices.end(), { positions[1], normals[1] });
        vertices.insert(vertices.end(), { positions[0], normals[1] });
        vertices.insert(vertices.end(), { positions[5], normals[1] });
        vertices.insert(vertices.end(), { positions[4], normals[1] });*/
    }

    // short box
    {
        // bottom
        vertices.insert(vertices.end(), { positions[9],  normals[2] });
        vertices.insert(vertices.end(), { positions[8],  normals[2] });
        vertices.insert(vertices.end(), { positions[11], normals[2] });
        vertices.insert(vertices.end(), { positions[10], normals[2] });

        // left
        vertices.insert(vertices.end(), { positions[10], normals[6] });
        vertices.insert(vertices.end(), { positions[8],  normals[6] });
        vertices.insert(vertices.end(), { positions[14], normals[6] });
        vertices.insert(vertices.end(), { positions[12], normals[6] });

        // back
        vertices.insert(vertices.end(), { positions[11], normals[7] });
        vertices.insert(vertices.end(), { positions[10], normals[7] });
        vertices.insert(vertices.end(), { positions[15], normals[7] });
        vertices.insert(vertices.end(), { positions[14], normals[7] });

        // right
        vertices.insert(vertices.end(), { positions[9],  normals[8] });
        vertices.insert(vertices.end(), { positions[11], normals[8] });
        vertices.insert(vertices.end(), { positions[13], normals[8] });
        vertices.insert(vertices.end(), { positions[15], normals[8] });

        // top
        vertices.insert(vertices.end(), { positions[12], normals[3] });
        vertices.insert(vertices.end(), { positions[13], normals[3] });
        vertices.insert(vertices.end(), { positions[14], normals[3] });
        vertices.insert(vertices.end(), { positions[15], normals[3] });

        // front
        vertices.insert(vertices.end(), { positions[8],  normals[9] });
        vertices.insert(vertices.end(), { positions[9],  normals[9] });
        vertices.insert(vertices.end(), { positions[12], normals[9] });
        vertices.insert(vertices.end(), { positions[13], normals[9] });
    }

    // tall box
    {
        // bottom
        vertices.insert(vertices.end(), { positions[17], normals[2] });
        vertices.insert(vertices.end(), { positions[16], normals[2] });
        vertices.insert(vertices.end(), { positions[19], normals[2] });
        vertices.insert(vertices.end(), { positions[18], normals[2] });

        // left
        vertices.insert(vertices.end(), { positions[18], normals[10] });
        vertices.insert(vertices.end(), { positions[16], normals[10] });
        vertices.insert(vertices.end(), { positions[22], normals[10] });
        vertices.insert(vertices.end(), { positions[20], normals[10] });

        // back
        vertices.insert(vertices.end(), { positions[19], normals[11] });
        vertices.insert(vertices.end(), { positions[18], normals[11] });
        vertices.insert(vertices.end(), { positions[23], normals[11] });
        vertices.insert(vertices.end(), { positions[22], normals[11] });

        // right
        vertices.insert(vertices.end(), { positions[17], normals[12] });
        vertices.insert(vertices.end(), { positions[19], normals[12] });
        vertices.insert(vertices.end(), { positions[21], normals[12] });
        vertices.insert(vertices.end(), { positions[23], normals[12] });

        // top
        vertices.insert(vertices.end(), { positions[20], normals[3] });
        vertices.insert(vertices.end(), { positions[21], normals[3] });
        vertices.insert(vertices.end(), { positions[22], normals[3] });
        vertices.insert(vertices.end(), { positions[23], normals[3] });

        // front
        vertices.insert(vertices.end(), { positions[16], normals[13] });
        vertices.insert(vertices.end(), { positions[17], normals[13] });
        vertices.insert(vertices.end(), { positions[20], normals[13] });
        vertices.insert(vertices.end(), { positions[21], normals[13] });
    }

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    for (int i = 0; i < vertices.size(); i++)
    {
        vertices[i].position.z *= -1.f;
        vertices[i].normal.z *= -1.f;
    }
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_UNREAL
    for (int i = 0; i < vertices.size(); i++)
    {
        vertices[i] =
        { 
            { vertices[i].position.z, vertices[i].position.x, vertices[i].position.y }, 
            { vertices[i].normal.z,   vertices[i].normal.x,   vertices[i].normal.y }
        };
    }
#endif

    return vertices;
}

/**
* Generate the indices for a Cornell Box.
*/
vector<UINT> GetCornellIndices()
{
    vector<UINT> indices;
    UINT vertIndex = 0;
    for (UINT quadIndex = 0; quadIndex < 17; quadIndex++)
    {
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
        indices.insert(indices.end(), { vertIndex, vertIndex + 2, vertIndex + 1, vertIndex + 1, vertIndex + 2, vertIndex + 3 });
#else
        indices.insert(indices.end(), { vertIndex, vertIndex + 1, vertIndex + 2, vertIndex + 2, vertIndex + 1, vertIndex + 3 });

#endif
        vertIndex += 4;
    }

    return indices;
}

/**
* Generate the vertices for a sphere.
*/
vector<Vertex> GetSphereVertices()
{
    vector<Vertex> vertices;
    float x, y, z;
    float theta, phi;

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    vertices.insert(vertices.end(), { { 0.f, 0.5f, 0.f } });
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_UNREAL
    vertices.insert(vertices.end(), { { 0.f, 0.f, 0.5f } });
#endif

    // Iterate across latitudes (elevations)
    for (UINT i = 0; i < (latitudes - 2); i++)
    {
        float ratio = (float)(i + 1) / (float)(latitudes - 1);

        theta = (XM_PI * ratio);
        float sinTheta = sinf(theta);
        float cosTheta = cosf(theta);

        // Iterate across longitudes
        for (UINT j = 0; j < longitudes; j++)
        {
            phi = 2.f * XM_PI * (float)j / (float)longitudes;
            float sinPhi = sinf(phi);
            float cosPhi = cosf(phi);

            x = 0.5f * sinTheta * cosPhi;
            y = 0.5f * cosTheta;
            z = 0.5f * sinTheta * sinPhi;
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT 
            vertices.insert(vertices.end(), { { x, y, z } });
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
            vertices.insert(vertices.end(), { { x, y, -z } });
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_UNREAL
            vertices.insert(vertices.end(), { { z, x, y } });
#endif
        }
    }

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    vertices.insert(vertices.end(), { { 0.f, -0.5f, 0.f } });
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_UNREAL
    vertices.insert(vertices.end(), { {0.f, 0.f, -0.5f } });
#endif

    return vertices;
}

/**
* Generate the indices for a sphere.
*/
vector<UINT> GetSphereIndices(UINT numVertices)
{
    vector<UINT> indices;
    UINT v1, v2, v3, v4, startVertex, endVertex;
    UINT i, j;

    // Add triangles at the north pole
    for (i = 0; i < longitudes; i++)
    {
        v1 = (i + 1) % longitudes + 1;
        v2 = (i + 1);
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
        indices.insert(indices.end(), { 0, v2, v1 });
#else
        indices.insert(indices.end(), { 0, v1, v2 });
#endif
    }

    // Add internal triangles
    for (i = 0; i < (latitudes - 3); i++)
    {
        UINT aStart = i * longitudes + 1;
        UINT bStart = (i + 1) * longitudes + 1;

        for (j = 0; j < longitudes; j++)
        {
            v1 = aStart + j;
            v2 = aStart + (j + 1) % longitudes;
            v3 = bStart + j;
            v4 = bStart + (j + 1) % longitudes;

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
            indices.insert(indices.end(), { v1, v4, v2, v3, v4, v1 });
#else
            indices.insert(indices.end(), { v1, v2, v4, v4, v3, v1 });
#endif
        }
    }

    // Add triangles at the south pole
    for (i = 0; i < longitudes; i++)
    {
        startVertex = longitudes * (latitudes - 3) + 1;
        endVertex = (numVertices - 1);
        v1 = startVertex + (i % longitudes);
        v2 = startVertex + ((i + 1) % longitudes);
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
        indices.insert(indices.end(), { endVertex, v2, v1 });
#else
        indices.insert(indices.end(), { endVertex, v1, v2 });
#endif
    }

    return indices;
}

/**
 * Create the scene vertex buffer.
 */
bool CreateVertexBuffer(D3D12Info &d3d, ID3D12Resource** vertexBuffer, D3D12_VERTEX_BUFFER_VIEW &view, RuntimeMesh &mesh)
{
    UINT stride = sizeof(Vertex);

    D3D12BufferCreateInfo info(mesh.numVertices * stride, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!D3D12::CreateBuffer(d3d, info, vertexBuffer)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    std::wstring name = std::wstring(mesh.name.begin(), mesh.name.end());
    name.append(L" Vertex Buffer");
    (*vertexBuffer)->SetName(name.c_str());
#endif

    // Copy the vertex data to the GPU
    UINT8* pDataBegin;
    D3D12_RANGE readRange = {};
    HRESULT hr = (*vertexBuffer)->Map(0, &readRange, reinterpret_cast<void**>(&pDataBegin));
    if (FAILED(hr)) return false;

    memcpy(pDataBegin, mesh.vertices, info.size);
    (*vertexBuffer)->Unmap(0, nullptr);

    // Initialize the vertex buffer view
    view.BufferLocation = (*vertexBuffer)->GetGPUVirtualAddress();
    view.StrideInBytes = stride;
    view.SizeInBytes = static_cast<UINT>(info.size);

    return true;
}

/**
 * Create the scene index buffer.
 */
bool CreateIndexBuffer(D3D12Info &d3d, ID3D12Resource** indexBuffer, D3D12_INDEX_BUFFER_VIEW &view, RuntimeMesh &mesh)
{
    UINT stride = sizeof(UINT);

    D3D12BufferCreateInfo info(mesh.numIndices * stride, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!D3D12::CreateBuffer(d3d, info, indexBuffer)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    std::wstring name = std::wstring(mesh.name.begin(), mesh.name.end());
    name.append(L" Index Buffer");
    (*indexBuffer)->SetName(name.c_str());
#endif

    // Copy the index data to the GPU
    UINT8* pDataBegin;
    D3D12_RANGE readRange = {};
    HRESULT hr = (*indexBuffer)->Map(0, &readRange, reinterpret_cast<void**>(&pDataBegin));
    if (FAILED(hr)) return false;

    memcpy(pDataBegin, mesh.indices, info.size);
    (*indexBuffer)->Unmap(0, nullptr);

    // Initialize the index buffer view
    view.BufferLocation = (*indexBuffer)->GetGPUVirtualAddress();
    view.SizeInBytes = static_cast<UINT>(info.size);
    view.Format = DXGI_FORMAT_R32_UINT;

    return true;
}

//----------------------------------------------------------------------------------------------------------
// Public Functions
//----------------------------------------------------------------------------------------------------------

namespace Geometry
{

/**
* Creates the geometry for a Cornell Box.
*/
bool CreateCornellBox(D3D12Info &d3d, D3D12Resources &resources)
{
    vector<Vertex> vertices = GetCornellVertices();
    vector<UINT> indices = GetCornellIndices();

    RuntimeMesh mesh = {};
    mesh.name = "Cornell Box";
    mesh.numVertices = (UINT)vertices.size();
    mesh.numIndices = (UINT)indices.size();
    mesh.vertices = vertices.data();
    mesh.indices = indices.data();

    // Create the vertex buffer 
    resources.vertexBuffers.resize(1);
    resources.vertexBufferViews.resize(1);
    if (!CreateVertexBuffer(d3d, &resources.vertexBuffers[0], resources.vertexBufferViews[0], mesh))
    {
        return false;
    }

    // Create the index buffer  
    resources.indexBuffers.resize(1);
    resources.indexBufferViews.resize(1);
    if (!CreateIndexBuffer(d3d, &resources.indexBuffers[0], resources.indexBufferViews[0], mesh))
    {
        return false;
    }

    return true;
}

/**
* Creates the geometry for a unit sphere centered about the origin.
*/
bool CreateSphere(D3D12Info &d3d, D3D12Resources &resources) 
{
    vector<Vertex> vertices = GetSphereVertices();
    
    UINT numIndices = (UINT)vertices.size();
    vector<UINT> indices = GetSphereIndices(numIndices);

    RuntimeMesh mesh = {};
    mesh.name = "Sphere";
    mesh.numVertices = (UINT)vertices.size();
    mesh.numIndices = (UINT)indices.size();
    mesh.vertices = vertices.data();
    mesh.indices = indices.data();

    // Create the vertex buffer
    if (!CreateVertexBuffer(d3d, &resources.sphereVertexBuffer, resources.sphereVertexBufferView, mesh))
    {
        return false;
    }

    // Create the index buffer  
    if (!CreateIndexBuffer(d3d, &resources.sphereIndexBuffer, resources.sphereIndexBufferView, mesh))
    {
        return false;
    }

    return true;
}

/**
 * Loads scene geometry from a binary file and creates the vertex/index buffers.
 */
bool LoadSceneBinary(string filepath, D3D12Info &d3d, D3D12Resources &resources)
{
    // Read the scene binary
    HRESULT hr = Serialization::ReadBinary(filepath, resources.geometry);
    if (FAILED(hr)) return false;

    // Create the D3D resources for each mesh
    resources.vertexBuffers.resize(resources.geometry.meshes.size());
    resources.vertexBufferViews.resize(resources.geometry.meshes.size());
    resources.indexBuffers.resize(resources.geometry.meshes.size());
    resources.indexBufferViews.resize(resources.geometry.meshes.size());
    for (UINT meshIndex = 0; meshIndex < resources.geometry.meshes.size(); meshIndex++)
    {
        // Create the vertex buffer
        if (!CreateVertexBuffer(d3d, 
            &resources.vertexBuffers[meshIndex], 
            resources.vertexBufferViews[meshIndex], 
            resources.geometry.meshes[meshIndex]))
        {
            return false;
        }

        // Create the index buffer
        if (!CreateIndexBuffer(d3d, 
            &resources.indexBuffers[meshIndex], 
            resources.indexBufferViews[meshIndex], 
            resources.geometry.meshes[meshIndex]))
        {
            return false;
        }
    }

    return true;
}

}
