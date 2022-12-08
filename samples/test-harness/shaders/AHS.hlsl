/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "include/Descriptors.hlsl"
#include "include/RayTracing.hlsl"

[shader("anyhit")]
void AHS_LOD0(inout PackedPayload packedPayload, BuiltInTriangleIntersectionAttributes attrib)
{
    // Load the intersected mesh geometry's data
    GeometryData geometry;
    GetGeometryData(InstanceID(), GeometryIndex(), geometry);

    // Load the material
    Material material = GetMaterial(geometry);

    float alpha = material.opacity;
    if (material.alphaMode == 2)
    {
        // Load and interpolate the triangle's texture coordinates
        float3 barycentrics = float3((1.f - attrib.barycentrics.x - attrib.barycentrics.y), attrib.barycentrics.x, attrib.barycentrics.y);
        float2 uv0 = LoadAndInterpolateUV0(InstanceID(), PrimitiveIndex(), geometry, barycentrics);
        if (material.albedoTexIdx > -1)
        {
            alpha *= GetTex2D(material.albedoTexIdx).SampleLevel(GetBilinearWrapSampler(), uv0, 0).a;
        }
    }

    if (alpha < material.alphaCutoff) IgnoreHit();
}

[shader("anyhit")]
void AHS_PRIMARY(inout PackedPayload payload, BuiltInTriangleIntersectionAttributes attrib)
{
    // Load the intersected mesh geometry's data
    GeometryData geometry;
    GetGeometryData(InstanceID(), GeometryIndex(), geometry);

    // Load the material
    Material material = GetMaterial(geometry);

    float alpha = material.opacity;
    if (material.alphaMode == 2)
    {
        // Load the vertices
        Vertex vertices[3];
        LoadVerticesPosUV0(InstanceID(), PrimitiveIndex(), geometry, vertices);

        // Compute texture coordinate differentials
        float2 dUVdx, dUVdy;
        ComputeUV0Differentials(vertices, WorldRayDirection(), RayTCurrent(), dUVdx, dUVdy);

        // TODO-ACM: passing ConstantBuffer<T> to functions crashes DXC HLSL->SPIRV
        //ConstantBuffer<Camera> camera = GetCamera();
        //ComputeUV0Differentials(vertices, camera, WorldRayDirection(), RayTCurrent(), dUVdx, dUVdy);

        // Interpolate the triangle's texture coordinates
        float3 barycentrics = float3((1.f - attrib.barycentrics.x - attrib.barycentrics.y), attrib.barycentrics.x, attrib.barycentrics.y);
        Vertex v = InterpolateVertexUV0(vertices, barycentrics);

        // Sample the texture
        if (material.albedoTexIdx > -1)
        {
            alpha *= GetTex2D(material.albedoTexIdx).SampleGrad(GetAnisoWrapSampler(), v.uv0, dUVdx, dUVdy).a;
        }
    }

    if (alpha < material.alphaCutoff) IgnoreHit();
}

[shader("anyhit")]
void AHS_GI(inout PackedPayload payload, BuiltInTriangleIntersectionAttributes attrib)
{
    // Load the intersected mesh geometry's data
    GeometryData geometry;
    GetGeometryData(InstanceID(), GeometryIndex(), geometry);

    // Load the surface material
    Material material = GetMaterial(geometry);

    float alpha = material.opacity;
    if (material.alphaMode == 2)
    {
        // Load the vertices
        Vertex vertices[3];
        LoadVerticesPosUV0(InstanceID(), PrimitiveIndex(), geometry, vertices);

        // Interpolate the triangle's texture coordinates
        float3 barycentrics = float3((1.f - attrib.barycentrics.x - attrib.barycentrics.y), attrib.barycentrics.x, attrib.barycentrics.y);
        Vertex v = InterpolateVertexUV0(vertices, barycentrics);

        // Sample the texture
        if (material.albedoTexIdx > -1)
        {
            // Get the number of mip levels
            uint width, height, numLevels;
            GetTex2D(material.albedoTexIdx).GetDimensions(0, width, height, numLevels);

            // Sample the texture
            alpha *= GetTex2D(material.albedoTexIdx).SampleLevel(GetBilinearWrapSampler(), v.uv0, numLevels * 0.6667f).a;
        }
    }

    if (alpha < material.alphaCutoff) IgnoreHit();
}
