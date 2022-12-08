/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

// Note: you must include Descriptors.hlsl before this file

#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

/**
 * Pack the payload into a compressed format.
 * Complement of UnpackPayload().
 */
PackedPayload PackPayload(Payload input)
{
    PackedPayload output = (PackedPayload)0;
    output.hitT = input.hitT;
    output.worldPosition = input.worldPosition;

    output.packed0.x  = f32tof16(input.albedo.r);
    output.packed0.x |= f32tof16(input.albedo.g) << 16;
    output.packed0.y  = f32tof16(input.albedo.b);
    output.packed0.y |= f32tof16(input.normal.x) << 16;
    output.packed0.z  = f32tof16(input.normal.y);
    output.packed0.z |= f32tof16(input.normal.z) << 16;
    output.packed0.w  = f32tof16(input.metallic);
    output.packed0.w |= f32tof16(input.roughness) << 16;

    output.packed1.x  = f32tof16(input.shadingNormal.x);
    output.packed1.x |= f32tof16(input.shadingNormal.y) << 16;
    output.packed1.y  = f32tof16(input.shadingNormal.z);
    output.packed1.y |= f32tof16(input.opacity) << 16;
    output.packed1.z  = f32tof16(input.hitKind);

    return output;
}

/**
 * Unpack the compressed payload into the full sized payload format.
 * Complement of PackPayload().
 */
Payload UnpackPayload(PackedPayload input)
{
    Payload output = (Payload)0;
    output.hitT = input.hitT;
    output.worldPosition = input.worldPosition;

    output.albedo.r = f16tof32(input.packed0.x);
    output.albedo.g = f16tof32(input.packed0.x >> 16);
    output.albedo.b = f16tof32(input.packed0.y);
    output.normal.x = f16tof32(input.packed0.y >> 16);
    output.normal.y = f16tof32(input.packed0.z);
    output.normal.z = f16tof32(input.packed0.z >> 16);
    output.metallic = f16tof32(input.packed0.w);
    output.roughness = f16tof32(input.packed0.w >> 16);

    output.shadingNormal.x = f16tof32(input.packed1.x);
    output.shadingNormal.y = f16tof32(input.packed1.x >> 16);
    output.shadingNormal.z = f16tof32(input.packed1.y);
    output.opacity = f16tof32(input.packed1.y >> 16);
    output.hitKind = f16tof32(input.packed1.z);

    return output;
}

/**
 * Load a triangle's indices.
 */
uint3 LoadIndices(uint meshIndex, uint primitiveIndex, GeometryData geometry)
{
    uint address = geometry.indexByteAddress + (primitiveIndex * 3) * 4;  // 3 indices per primitive, 4 bytes for each index
    return GetIndexBuffer(meshIndex).Load3(address); // Mesh index buffers start at index 4 and alternate with vertex buffer pointers
}

/**
 * Load a triangle's vertex data (all: position, normal, tangent, uv0).
 */
void LoadVertices(uint meshIndex, uint primitiveIndex, GeometryData geometry, out Vertex vertices[3])
{
    // Get the indices
    uint3 indices = LoadIndices(meshIndex, primitiveIndex, geometry);

    // Load the vertices
    uint address;
    for (uint i = 0; i < 3; i++)
    {
        vertices[i] = (Vertex)0;
        address = geometry.vertexByteAddress + (indices[i] * 12) * 4;  // Vertices contain 12 floats / 48 bytes

        // Load the position
        vertices[i].position = asfloat(GetVertexBuffer(meshIndex).Load3(address));
        address += 12;

        // Load the normal
        vertices[i].normal = asfloat(GetVertexBuffer(meshIndex).Load3(address));
        address += 12;

        // Load the tangent
        vertices[i].tangent = asfloat(GetVertexBuffer(meshIndex).Load4(address));
        address += 16;

        // Load the texture coordinates
        vertices[i].uv0 = asfloat(GetVertexBuffer(meshIndex).Load2(address));
    }
}

/**
 * Load a triangle's vertex data (only position and uv0).
 */
void LoadVerticesPosUV0(uint meshIndex, uint primitiveIndex, GeometryData geometry, out Vertex vertices[3])
{
    // Get the indices
    uint3 indices = LoadIndices(meshIndex, primitiveIndex, geometry);

    // Load the vertices
    uint address;
    for (uint i = 0; i < 3; i++)
    {
        vertices[i] = (Vertex)0;
        address = geometry.vertexByteAddress + (indices[i] * 12) * 4;  // Vertices contain 12 floats / 48 bytes

        // Load the position
        vertices[i].position = asfloat(GetVertexBuffer(meshIndex).Load3(address));
        address += 40; // skip normal and tangent

        // Load the texture coordinates
        vertices[i].uv0 = asfloat(GetVertexBuffer(meshIndex).Load2(address));
    }
}

/**
 * Load (only) a triangle's texture coordinates and return the barycentric interpolated texture coordinates.
 */
float2 LoadAndInterpolateUV0(uint meshIndex, uint primitiveIndex, GeometryData geometry, float3 barycentrics)
{
    // Get the triangle indices
    uint3 indices = LoadIndices(meshIndex, primitiveIndex, geometry);

    // Interpolate the texture coordinates
    int address;
    float2 uv0 = float2(0.f, 0.f);
    for (uint i = 0; i < 3; i++)
    {
        address = geometry.vertexByteAddress + (indices[i] * 12) * 4;  // 12 floats (3: pos, 3: normals, 4:tangent, 2:uv0)
        address += 40;                                                // 40 bytes (10 * 4): skip position, normal, and tangent
        uv0 += asfloat(GetVertexBuffer(meshIndex).Load2(address)) * barycentrics[i];
    }

    return uv0;
}

/**
 * Return interpolated vertex attributes (all).
 */
Vertex InterpolateVertex(Vertex vertices[3], float3 barycentrics)
{
    // Interpolate the vertex attributes
    Vertex v = (Vertex)0;
    for (uint i = 0; i < 3; i++)
    {
        v.position += vertices[i].position * barycentrics[i];
        v.normal += vertices[i].normal * barycentrics[i];
        v.tangent.xyz += vertices[i].tangent.xyz * barycentrics[i];
        v.uv0 += vertices[i].uv0 * barycentrics[i];
    }

    // Normalize normal and tangent vectors, set tangent direction component
    v.normal = normalize(v.normal);
    v.tangent.xyz = normalize(v.tangent.xyz);
    v.tangent.w = vertices[0].tangent.w;

    return v;
}

/**
 * Return interpolated vertex attributes (uv0 only)
 */
Vertex InterpolateVertexUV0(Vertex vertices[3], float3 barycentrics)
{
    // Interpolate the vertex attributes
    Vertex v = (Vertex)0;
    for (uint i = 0; i < 3; i++)
    {
        v.uv0 += vertices[i].uv0 * barycentrics[i];
    }

    return v;
}

// --- Ray Differentials ---

struct RayDiff
{
    float3 dOdx;
    float3 dOdy;
    float3 dDdx;
    float3 dDdy;
};

/**
 * Get the ray direction differentials.
 */
void ComputeRayDirectionDifferentials(float3 nonNormalizedCameraRaydir, float3 right, float3 up, float2 viewportDims, out float3 dDdx, out float3 dDdy)
{
    // Igehy Equation 8
    float dd = dot(nonNormalizedCameraRaydir, nonNormalizedCameraRaydir);
    float divd = 2.f / (dd * sqrt(dd));
    float dr = dot(nonNormalizedCameraRaydir, right);
    float du = dot(nonNormalizedCameraRaydir, up);
    dDdx = ((dd * right) - (dr * nonNormalizedCameraRaydir)) * divd / viewportDims.x;
    dDdy = -((dd * up) - (du * nonNormalizedCameraRaydir)) * divd / viewportDims.y;
}

/**
 * Propogate the ray differential to the current hit point.
 */
void PropagateRayDiff(float3 D, float t, float3 N, inout RayDiff rd)
{
    // Part of Igehy Equation 10
    float3 dodx = rd.dOdx + t * rd.dDdx;
    float3 dody = rd.dOdy + t * rd.dDdy;

    // Igehy Equations 10 and 12
    float rcpDN = 1.f / dot(D, N);
    float dtdx = -dot(dodx, N) * rcpDN;
    float dtdy = -dot(dody, N) * rcpDN;
    dodx += D * dtdx;
    dody += D * dtdy;

    // Store differential origins
    rd.dOdx = dodx;
    rd.dOdy = dody;
}

/**
 * Apply instance transforms to geometry, compute triangle edges and normal.
 */
void PrepVerticesForRayDiffs(Vertex vertices[3], out float3 edge01, out float3 edge02, out float3 faceNormal)
{
    // Apply instance transforms
    vertices[0].position = mul(ObjectToWorld3x4(), float4(vertices[0].position, 1.f)).xyz;
    vertices[1].position = mul(ObjectToWorld3x4(), float4(vertices[1].position, 1.f)).xyz;
    vertices[2].position = mul(ObjectToWorld3x4(), float4(vertices[2].position, 1.f)).xyz;

    // Find edges and face normal
    edge01 = vertices[1].position - vertices[0].position;
    edge02 = vertices[2].position - vertices[0].position;
    faceNormal = cross(edge01, edge02);
}

/**
 * Get the barycentric differentials.
 */
void ComputeBarycentricDifferentials(RayDiff rd, float3 rayDir, float3 edge01, float3 edge02, float3 faceNormalW, out float2 dBarydx, out float2 dBarydy)
{
    // Igehy "Normal-Interpolated Triangles"
    float3 Nu = cross(edge02, faceNormalW);
    float3 Nv = cross(edge01, faceNormalW);

    // Plane equations for the triangle edges, scaled in order to make the dot with the opposite vertex equal to 1
    float3 Lu = Nu / (dot(Nu, edge01));
    float3 Lv = Nv / (dot(Nv, edge02));

    dBarydx.x = dot(Lu, rd.dOdx);     // du / dx
    dBarydx.y = dot(Lv, rd.dOdx);     // dv / dx
    dBarydy.x = dot(Lu, rd.dOdy);     // du / dy
    dBarydy.y = dot(Lv, rd.dOdy);     // dv / dy
}

/**
 * Get the interpolated texture coordinate differentials.
 */
void InterpolateTexCoordDifferentials(float2 dBarydx, float2 dBarydy, Vertex vertices[3], out float2 dx, out float2 dy)
{
    float2 delta1 = vertices[1].uv0 - vertices[0].uv0;
    float2 delta2 = vertices[2].uv0 - vertices[0].uv0;
    dx = dBarydx.x * delta1 + dBarydx.y * delta2;
    dy = dBarydy.x * delta1 + dBarydy.y * delta2;
}

/**
 * Get the texture coordinate differentials using ray differentials.
 */
//void ComputeUV0Differentials(Vertex vertices[3], ConstantBuffer<Camera> camera, float3 rayDirection, float hitT, out float2 dUVdx, out float2 dUVdy)
void ComputeUV0Differentials(Vertex vertices[3], float3 rayDirection, float hitT, out float2 dUVdx, out float2 dUVdy)
{
    // Initialize a ray differential
    RayDiff rd = (RayDiff)0;

    // Get ray direction differentials
    //ComputeRayDirectionDifferentials(rayDirection, camera.right, camera.up, camera.resolution, rd.dDdx, rd.dDdy);
    ComputeRayDirectionDifferentials(rayDirection, GetCamera().right, GetCamera().up, GetCamera().resolution, rd.dDdx, rd.dDdy);

    // Get the triangle edges and face normal
    float3 edge01, edge02, faceNormal;
    PrepVerticesForRayDiffs(vertices, edge01, edge02, faceNormal);

    // Propagate the ray differential to the current hit point
    PropagateRayDiff(rayDirection, hitT, faceNormal, rd);

    // Get the barycentric differentials
    float2 dBarydx, dBarydy;
    ComputeBarycentricDifferentials(rd, rayDirection, edge01, edge02, faceNormal, dBarydx, dBarydy);

    // Interpolate the texture coordinate differentials
    InterpolateTexCoordDifferentials(dBarydx, dBarydy, vertices, dUVdx, dUVdy);
}

#endif // RAYTRACING_HLSL
