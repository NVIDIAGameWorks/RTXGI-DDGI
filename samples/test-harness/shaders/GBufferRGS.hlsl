/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

    RayDesc ray = (RayDesc)0;
    ray.Origin = Camera.position;
    ray.TMin = 0.f;
    ray.TMax = 1e27f;

    // Pixel coordinates, remapped to [-1, 1] with y-direction flipped to match world-space
    // Camera basis, adjusted for the aspect ratio and vertical field of view
    float  px = (((float)LaunchIndex.x + 0.5f) / (float)LaunchDimensions.x) * 2.f - 1.f;
    float  py = (((float)LaunchIndex.y + 0.5f) / (float)LaunchDimensions.y) * -2.f + 1.f;
    float3 right = Camera.aspect * Camera.tanHalfFovY * Camera.right;
    float3 up = Camera.tanHalfFovY * Camera.up;

    // Compute the primary ray direction
    ray.Direction = (px * right) + (py * up) + Camera.forward;

    // Primary Ray Trace
    PackedPayload packedPayload = (PackedPayload)0;
    packedPayload.hitT = (float)LaunchIndex.x;
    packedPayload.worldPosition = float3((float)LaunchIndex.y, (float2)LaunchDimensions);
    TraceRay(
        SceneBVH,
        RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
        0xFF,
        0,
        1,
        0,
        ray,
        packedPayload);

    // Ray Miss, early out.
    if (packedPayload.hitT < 0.f)
    {
        // Convert albedo to sRGB before storing
        GBufferA[LaunchIndex] = float4(LinearToSRGB(GetGlobalConst(app, skyRadiance)), 0.f);
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
    float3 diffuse = DirectDiffuseLighting(payload, GetGlobalConst(pt, rayNormalBias), GetGlobalConst(pt, rayViewBias), SceneBVH);

    // Convert albedo to sRGB before storing
    payload.albedo = LinearToSRGB(payload.albedo);

    // Write the GBuffer
    GBufferA[LaunchIndex] = float4(payload.albedo, 1.f);
    GBufferB[LaunchIndex] = float4(payload.worldPosition, payload.hitT);
    GBufferC[LaunchIndex] = float4(payload.normal, 1.f);
    GBufferD[LaunchIndex] = float4(diffuse, 1.f);
}
