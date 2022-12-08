/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "include/Common.hlsl"
#include "include/Descriptors.hlsl"
#include "include/Lighting.hlsl"
#include "include/RayTracing.hlsl"

// ---[ Ray Generation Shader ]---

[shader("raygeneration")]
void RayGen()
{
    uint2 LaunchIndex = DispatchRaysIndex().xy;
    uint2 LaunchDimensions = DispatchRaysDimensions().xy;

    // Get the lights
    StructuredBuffer<Light> Lights = GetLights();

    // Get the (bindless) resources
    RWTexture2D<float4> GBufferA = GetRWTex2D(GBUFFERA_INDEX);
    RWTexture2D<float4> GBufferB = GetRWTex2D(GBUFFERB_INDEX);
    RWTexture2D<float4> GBufferC = GetRWTex2D(GBUFFERC_INDEX);
    RWTexture2D<float4> GBufferD = GetRWTex2D(GBUFFERD_INDEX);
    RaytracingAccelerationStructure SceneTLAS = GetAccelerationStructure(SCENE_TLAS_INDEX);

    // Setup the primary ray
    RayDesc ray = (RayDesc)0;
    ray.Origin = GetCamera().position;
    ray.TMin = 0.f;
    ray.TMax = 1e27f;

    // Pixel coordinates, remapped to [-1, 1] with y-direction flipped to match world-space
    // Camera basis, adjusted for the aspect ratio and vertical field of view
    float  px = (((float)LaunchIndex.x + 0.5f) / (float)LaunchDimensions.x) * 2.f - 1.f;
    float  py = (((float)LaunchIndex.y + 0.5f) / (float)LaunchDimensions.y) * -2.f + 1.f;
    float3 right = GetCamera().aspect * GetCamera().tanHalfFovY * GetCamera().right;
    float3 up = GetCamera().tanHalfFovY * GetCamera().up;

    // Compute the primary ray direction
    ray.Direction = (px * right) + (py * up) + GetCamera().forward;

    // Primary Ray Trace
    PackedPayload packedPayload = (PackedPayload)0;
    packedPayload.hitT = (float)LaunchIndex.x;
    packedPayload.worldPosition = float3((float)LaunchIndex.y, (float2)LaunchDimensions);
    TraceRay(
        SceneTLAS,
        RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
        0xFF,
        0,
        0,
        0,
        ray,
        packedPayload);

    // Ray Miss, early out.
    if (packedPayload.hitT < 0.f)
    {
        // Convert albedo to sRGB before storing
        GBufferA[LaunchIndex] = float4(LinearToSRGB(GetGlobalConst(app, skyRadiance)), COMPOSITE_FLAG_POSTPROCESS_PIXEL);
        GBufferB[LaunchIndex].w = -1.f;

        // Optional clear writes. Not necessary for final image, but
        // useful for image comparisons during regression testing.
        GBufferB[LaunchIndex] = float4(0.f, 0.f, 0.f, -1.f);
        GBufferC[LaunchIndex] = float4(0.f, 0.f, 0.f, 0.f);
        GBufferD[LaunchIndex] = float4(0.f, 0.f, 0.f, 0.f);

        return;
    }

    // Unpack the payload
    Payload payload = UnpackPayload(packedPayload);

    // Compute direct diffuse lighting
    float3 diffuse = DirectDiffuseLighting(payload, GetGlobalConst(pt, rayNormalBias), GetGlobalConst(pt, rayViewBias), SceneTLAS, Lights);

    // Convert albedo to sRGB before storing
    payload.albedo = LinearToSRGB(payload.albedo);

    // Write the GBuffer
    GBufferA[LaunchIndex] = float4(payload.albedo, COMPOSITE_FLAG_LIGHT_PIXEL);
    GBufferB[LaunchIndex] = float4(payload.worldPosition, payload.hitT);
    GBufferC[LaunchIndex] = float4(payload.normal, 1.f);
    GBufferD[LaunchIndex] = float4(diffuse, 1.f);
}
