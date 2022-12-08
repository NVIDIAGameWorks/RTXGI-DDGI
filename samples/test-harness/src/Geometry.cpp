/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "Geometry.h"

//----------------------------------------------------------------------------------------------------------
// Private Functions
//----------------------------------------------------------------------------------------------------------

namespace Geometry
{

    //----------------------------------------------------------------------------------------------------------
    // Private Functions
    //----------------------------------------------------------------------------------------------------------

    /**
     * Generate the vertices for a sphere.
     */
    std::vector<Graphics::Vertex> GetSphereVertices(uint32_t latitudes, uint32_t longitudes)
    {
        std::vector<Graphics::Vertex> vertices;
        float x, y, z;
        float theta, phi;

    #if COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT || COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT
        vertices.insert(vertices.end(), { { 0.f, 0.5f, 0.f } });
    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT_Z_UP || COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT_Z_UP
        vertices.insert(vertices.end(), { { 0.f, 0.f, 0.5f } });
    #endif

        // Iterate across latitudes (elevations)
        for (uint32_t i = 0; i < (latitudes - 2); i++)
        {
            float ratio = (float)(i + 1) / (float)(latitudes - 1);

            theta = (XM_PI * ratio);
            float sinTheta = sinf(theta);
            float cosTheta = cosf(theta);

            // Iterate across longitudes
            for (uint32_t j = 0; j < longitudes; j++)
            {
                phi = 2.f * XM_PI * (float)j / (float)longitudes;
                float sinPhi = sinf(phi);
                float cosPhi = cosf(phi);

                x = 0.5f * sinTheta * cosPhi;
                y = 0.5f * cosTheta;
                z = 0.5f * sinTheta * sinPhi;
            #if COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT
                vertices.insert(vertices.end(), { { x, y, z } });
            #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT
                vertices.insert(vertices.end(), { { x, y, -z } });
            #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT_Z_UP
                vertices.insert(vertices.end(), { { z, x, y } });
            #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT_Z_UP
                vertices.insert(vertices.end(), { { z, y, x } });
            #endif
            }
        }

    #if COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT || COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT
        vertices.insert(vertices.end(), { { 0.f, -0.5f, 0.f } });
    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT_Z_UP || COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT_Z_UP
        vertices.insert(vertices.end(), { {0.f, 0.f, -0.5f } });
    #endif

        return vertices;
    }

    /**
     * Generate the indices for a sphere with the given number of vertices.
     */
    std::vector<uint32_t> GetSphereIndices(uint32_t latitudes, uint32_t longitudes, uint32_t numVertices)
    {
        std::vector<uint32_t> indices;
        uint32_t v1, v2, v3, v4, startVertex, endVertex;
        uint32_t i, j;

        // Add triangles at the north pole
        for (i = 0; i < longitudes; i++)
        {
            v1 = (i + 1) % longitudes + 1;
            v2 = (i + 1);
            indices.insert(indices.end(), { 0, v2, v1 });
        }

        // Add internal triangles
        for (i = 0; i < (latitudes - 3); i++)
        {
            uint32_t aStart = i * longitudes + 1;
            uint32_t bStart = (i + 1) * longitudes + 1;

            for (j = 0; j < longitudes; j++)
            {
                v1 = aStart + j;
                v2 = aStart + (j + 1) % longitudes;
                v3 = bStart + j;
                v4 = bStart + (j + 1) % longitudes;
                indices.insert(indices.end(), { v1, v4, v2, v3, v4, v1 });
            }
        }

        // Add triangles at the south pole
        for (i = 0; i < longitudes; i++)
        {
            startVertex = longitudes * (latitudes - 3) + 1;
            endVertex = (numVertices - 1);
            v1 = startVertex + (i % longitudes);
            v2 = startVertex + ((i + 1) % longitudes);
            indices.insert(indices.end(), { endVertex, v2, v1 });
        }

        return indices;
    }

    //----------------------------------------------------------------------------------------------------------
    // Public Functions
    //----------------------------------------------------------------------------------------------------------

    void CreateSphere(uint32_t latitudes, uint32_t longitudes, Scenes::Mesh& mesh)
    {
        Scenes::MeshPrimitive& primitive = mesh.primitives.emplace_back();
        primitive.vertices = GetSphereVertices(latitudes, longitudes);
        primitive.indices = GetSphereIndices(latitudes, longitudes, static_cast<uint32_t>(primitive.vertices.size()));

        mesh.numVertices = static_cast<int>(primitive.vertices.size());
        mesh.numIndices = static_cast<int>(primitive.indices.size());
    }

}
