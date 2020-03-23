/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "include/Common.hlsl"
#include "include/RTCommon.hlsl"
#include "include/RTGlobalRS.hlsl"
#include "include/LightingCommon.hlsl"
#include "include/Random.hlsl"

// ---[ Helper Functions ]---

float3 TracePath(RayDesc ray, uint seed)
{
    // Trace path (20 bounces)
    float3 throughput = float3(1.f, 1.f, 1.f);
    float3 color = float3(0.f, 0.f, 0.f);
    for (int i = 0; i < NumBounces; i++)
    {
        // Trace the ray
        PayloadData payload = (PayloadData)0;
        TraceRay(
            SceneBVH,
            RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
            0xFF,
            0,
            1,
            0,
            ray,
            payload);
        
        // Miss, exit loop
        if (payload.hitT < 0.f)
        {
            throughput *= payload.baseColor;
            color += payload.baseColor * throughput;
            break;
        }

        // Light the surface
        float3 diffuse = DirectDiffuseLighting(
            payload.baseColor,
            payload.worldPosition,
            payload.normal,
            NormalBias,
            ViewBias,
            SceneBVH);

        // Attenuate the color
        color += diffuse * throughput;
        throughput *= (payload.baseColor / PI);

        seed += i;

        // Set the ray origin for the next bounce
        ray.Origin = payload.worldPosition;

        float3 direction = GetRandomDirectionOnHemisphere(payload.normal, seed);
        ray.Origin += (payload.normal * NormalBias);
        float3 target = ray.Origin + payload.normal + direction;

        // Compute the ray direction for the next bounce
        ray.Direction = normalize(target - ray.Origin);
    }

    return saturate(color);
}

// ---[ Ray Generation Shader ]---

[shader("raygeneration")]
void RayGen()
{
    uint2 LaunchIndex = DispatchRaysIndex().xy;
    uint2 LaunchDimensions = DispatchRaysDimensions().xy;

    // Initialize the random seed
    uint seed = (LaunchIndex.y * LaunchDimensions.x) + LaunchIndex.x;
    seed *= FrameNumber;

    // Setup the ray
    RayDesc ray = (RayDesc)0;
    ray.Origin = cameraOrigin;
    ray.TMin = 0.f;
    ray.TMax = 1e27f;

    // Compute the primary ray direction
    float  halfHeight = cameraTanHalfFovY;
    float  halfWidth = (cameraAspect * halfHeight);
    float3 lowerLeftCorner = cameraOrigin - (halfWidth * cameraRight) - (halfHeight * cameraUp) + cameraForward;
    float3 horizontal = (2.f * halfWidth) * cameraRight;
    float3 vertical = (2.f * halfHeight) * cameraUp;

    float s = ((float)LaunchIndex.x + 0.5f) / (float)LaunchDimensions.x;
    float t = 1.f - (((float)LaunchIndex.y + 0.5f) / (float)LaunchDimensions.y);

    ray.Direction = (lowerLeftCorner + s * horizontal + t * vertical) - ray.Origin;

    // Trace the path (multiple ray segments)
    float3 color = TracePath(ray, seed);

    // Progressive Accumulation
    float numPaths = 1;
    if (FrameNumber > 1)
    {
        // Read the previous color and number of paths
        float3 previousColor = PTAccumulation[LaunchIndex.xy].xyz;
        numPaths = PTAccumulation[LaunchIndex.xy].w;

        // Add in the new color and number of paths
        color = (previousColor + color);
        numPaths++;

        // Store to the accumulation buffer
        PTAccumulation[LaunchIndex.xy] = float4(color, numPaths);
    }
    else
    {
        // Clear the accumulation buffer when moving
        PTAccumulation[LaunchIndex.xy] = float4(0, 0, 0, 0);
    }

    color /= (float)numPaths;   // Normalize

    // Apply exposure
    color *= Exposure;

    // Apply tonemapping
    color = ACESFilm(color);

    // Add noise to handle SDR color banding
    float3 noise = GetLowDiscrepancyBlueNoise(int2(LaunchIndex), FrameNumber, 1.0f / 256.0f, BlueNoiseRGB);
    color += noise;

    // Gamma correct
    color = LinearToSRGB(color);

    PTOutput[LaunchIndex] = float4(color, 1.f);
}
