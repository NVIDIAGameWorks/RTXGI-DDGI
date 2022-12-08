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
#include "include/RayTracing.hlsl"

/**
 * Computes a low discrepancy spherically distributed direction on the unit sphere,
 * for the given index in a set of samples. Each direction is unique in
 * the set, but the set of directions is always the same.
 */
float3 SphericalFibonacci(float index, float numSamples)
{
    const float b = (sqrt(5.f) * 0.5f + 0.5f) - 1.f;
    float phi = TWO_PI * frac(index * b);
    float cosTheta = 1.f - (2.f * index + 1.f) * (1.f / numSamples);
    float sinTheta = sqrt(saturate(1.f - (cosTheta * cosTheta)));

    return float3((cos(phi) * sinTheta), (sin(phi) * sinTheta), cosTheta);
}

/**
 * Trace a ray to determine occlusion.
 */
float GetOcclusion(int2 screenPos, float3 worldPos, float3 normal)
{
    static const float c_numAngles = 10.f;

    // Get the (bindless) resources
    Texture2D<float4> BlueNoise = GetTex2D(BLUE_NOISE_INDEX);
    RaytracingAccelerationStructure SceneTLAS = GetAccelerationStructure(SCENE_TLAS_INDEX);

    // Load a value from the noise texture
    float  blueNoiseValue = BlueNoise.Load(int3(screenPos.xy, 0) % 256).r;
    float3 blueNoiseUnitVector = SphericalFibonacci(clamp(blueNoiseValue * c_numAngles, 0, c_numAngles - 1), c_numAngles);

    // Use the noise vector to perturb the normal, creating a new direction
    float3 rayDirection = normalize(normal + blueNoiseUnitVector);

    // Setup the ray
    RayDesc ray;
    ray.Origin = worldPos + (normal * GetGlobalConst(rtao, rayNormalBias)); // TODO: not using viewBias!
    ray.Direction = rayDirection;
    ray.TMin = 0.f;
    ray.TMax = GetGlobalConst(rtao, rayLength);

    // Trace a visibility ray
    PackedPayload packedPayload = (PackedPayload)0;
    TraceRay(
        SceneTLAS,
        RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
        0xFF,
        0,
        0,
        0,
        ray,
        packedPayload);

    // Put the linear hit distance ratio through a pow() to convert it to an occlusion value
    return (packedPayload.hitT < 0.f) ? 1.f : pow(saturate(packedPayload.hitT / GetGlobalConst(rtao, rayLength)), GetGlobalConst(rtao, power));
}

// ---[ Ray Generation Shader ]---

[shader("raygeneration")]
void RayGen()
{
    int2 LaunchIndex = int2(DispatchRaysIndex().xy);

    // Get the (bindless) resources
    RWTexture2D<float4> GBufferA = GetRWTex2D(GBUFFERA_INDEX);
    RWTexture2D<float4> GBufferB = GetRWTex2D(GBUFFERB_INDEX);
    RWTexture2D<float4> GBufferC = GetRWTex2D(GBUFFERC_INDEX);
    RWTexture2D<float4> GBufferD = GetRWTex2D(GBUFFERD_INDEX);
    RWTexture2D<float4> RTAORaw = GetRWTex2D(RTAO_RAW_INDEX);

    // Early exit for pixels without a primary ray intersection
    if (GBufferA.Load(LaunchIndex).w < COMPOSITE_FLAG_LIGHT_PIXEL)
    {
        GBufferD[LaunchIndex].a = 1.f;  // No occlusion
        return;
    }

    // Load the world position and normal from the GBuffer
    float3 worldPos = GBufferB.Load(LaunchIndex).xyz;
    float3 normal = GBufferC.Load(LaunchIndex).xyz;

    // Store the occlusion
    RTAORaw[LaunchIndex] = GetOcclusion(LaunchIndex, worldPos, normal);
}
