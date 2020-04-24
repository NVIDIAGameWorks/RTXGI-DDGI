/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "../../../rtxgi-sdk/shaders/ddgi/ProbeCommon.hlsl"

#include "include/RTCommon.hlsl"
#include "include/RTGlobalRS.hlsl"
#include "include/Random.hlsl"

float AOQuery(int2 BNPixel, float3 worldPos, float3 normal)
{
    static const float c_numAngles = 10.f;

    float  blueNoiseValue = BlueNoiseRGB.Load(int3(BNPixel, 0) % 256).r;
    float3 blueNoiseUnitVector = DDGISphericalFibonacci(clamp(blueNoiseValue * c_numAngles, 0, c_numAngles - 1), c_numAngles);
    float3 rayDirection = normalize(normal + blueNoiseUnitVector);

    // Setup the ray
    RayDesc ray;
    ray.Origin = worldPos;
    ray.Direction = rayDirection;
    ray.TMin = AOBias;
    ray.TMax = AORadius;

    // Shoot a visibility ray
    PayloadData payload = (PayloadData)0;
    TraceRay(
        SceneBVH,
        RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
        0x01,
        0,
        1,
        0,
        ray,
        payload);

    // Put the linear distance percentage through a pow() to convert it to an AO value
    return (payload.hitT < 0.f) ? 1.f : pow(clamp(payload.hitT / AORadius, 0.f, 1.f), AOPower);
}

// ---[ Ray Generation Shader ]---

[shader("raygeneration")]
void RayGen()
{
    int2 LaunchIndex = int2(DispatchRaysIndex().xy);

    // Early exit for pixels without a primary ray intersection
    if (RTGBufferA.Load(LaunchIndex).w == 0.f)
    {
        RTGBufferD[LaunchIndex].a = 1.f;
        return;
    }

    // Get world position and normal of pixel
    float3 worldPos = RTGBufferB.Load(LaunchIndex).xyz;
    float3 normal = RTGBufferC.Load(LaunchIndex).xyz;

    // Two Sample AO
    float AOResult1 = AOQuery(LaunchIndex, worldPos, normal);
    float AOResult2 = AOQuery(LaunchIndex + int2(89, 73), worldPos, normal);
    float AO = (AOResult1 + AOResult2) / 2.f;

    // Store the AO
    RTAORaw[LaunchIndex] = AO;
}
