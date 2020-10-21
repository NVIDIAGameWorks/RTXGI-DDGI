/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "Geometry.h"

#include <rtxgi/Defines.h>

using namespace DirectX;

const static UINT longitudes = 30;
const static UINT latitudes = 30;

//----------------------------------------------------------------------------------------------------------
// Private Functions
//----------------------------------------------------------------------------------------------------------

/**
* Generate the vertices for a sphere.
*/
std::vector<Vertex> GetSphereVertices()
{
    std::vector<Vertex> vertices;
    float x, y, z;
    float theta, phi;

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    vertices.insert(vertices.end(), { { 0.f, 0.5f, 0.f } });
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
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
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
            vertices.insert(vertices.end(), { { z, x, y } });
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
            vertices.insert(vertices.end(), { { z, y, x } });
#endif
        }
    }

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    vertices.insert(vertices.end(), { { 0.f, -0.5f, 0.f } });
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
    vertices.insert(vertices.end(), { {0.f, 0.f, -0.5f } });
#endif

    return vertices;
}

/**
* Generate the indices for a sphere with the given number of vertices.
*/
std::vector<UINT> GetSphereIndices(UINT numVertices)
{
    std::vector<UINT> indices;
    UINT v1, v2, v3, v4, startVertex, endVertex;
    UINT i, j;

    // Add triangles at the north pole
    for (i = 0; i < longitudes; i++)
    {
        v1 = (i + 1) % longitudes + 1;
        v2 = (i + 1);
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
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

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
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
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
        indices.insert(indices.end(), { endVertex, v2, v1 });
#else
        indices.insert(indices.end(), { endVertex, v1, v2 });
#endif
    }

    return indices;
}

//----------------------------------------------------------------------------------------------------------
// Public Functions
//----------------------------------------------------------------------------------------------------------

namespace Geometry
{

/**
* Creates the geometry for a unit sphere centered about the origin.
*/
bool CreateSphere(D3D12Global &d3d, D3D12Resources &resources) 
{
    MeshPrimitive primitive;
    primitive.vertices = GetSphereVertices();
    primitive.indices = GetSphereIndices(static_cast<UINT>(primitive.vertices.size()));

    // Create the vertex and index buffer
    if (!D3D12::CreateVertexBuffer(d3d, &resources.sphereVB, resources.sphereVBView, primitive)) return false;
    if (!D3D12::CreateIndexBuffer(d3d, &resources.sphereIB, resources.sphereIBView, primitive)) return false;
    return true;
}

}
