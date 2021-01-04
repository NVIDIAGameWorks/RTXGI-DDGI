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
#include "include/RTCommon.hlsl"
#include "include/RTGlobalRS.hlsl"
#include "include/LightingCommon.hlsl"
#include "include/Random.hlsl"

// ---[ Helper Functions ]---

float3 TracePath(RayDesc ray, uint seed)
{
    float3 throughput = float3(1.f, 1.f, 1.f);
    float3 color = float3(0.f, 0.f, 0.f);

    for (int i = 0; i < NumBounces; i++)
    {
        // Trace the ray
        PackedPayload packedPayload = (PackedPayload)0;
        TraceRay(
            SceneBVH,
            RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
            0xFF,
            0,
            1,
            0,
            ray,
            packedPayload);

        // Unpack the payload
        Payload payload = UnpackPayload(packedPayload);

        // Miss, exit loop
        if (payload.hitT < 0.f)
        {
            color += SkyIntensity.xxx * throughput;
            break;
        }

        // Light the surface
        float3 diffuse = DirectDiffuseLighting(payload, NormalBias, ViewBias, SceneBVH);

        // Attenuate the color
        color += diffuse * throughput;

        // Increment the seed
        seed += i;

        // Set the ray origin for the next bounce
        ray.Origin = payload.worldPosition;
        ray.Origin += (payload.normal * NormalBias);

        // Select random directions on the hemisphere with a cos(theta) distribution and compute throughput
        ray.Direction = GetRandomCosineDirectionOnHemisphere(payload.normal, seed);
        throughput *= payload.albedo;

        // Select random directions on the hemisphere with a uniform distribution and compute throughput
        // [BRDF * cos(theta)] / PDF, where PDF = 1 / Area of Integration = 1 / 2PI
        //ray.Direction = GetRandomDirectionOnHemisphere(payload.normal, seed);
        //throughput *= (payload.albedo / PI) * dot(payload.normal, ray.Direction) * (2.f * PI);
    }

    return color;
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

    // Trace the path (multiple ray segments)
    float3 color = float3(0.f, 0.f, 0.f);
    float2 offsets = float2(0.5f, 0.5f);
    for (int sampleIndex = 0; sampleIndex < cameraNumPaths; sampleIndex++)
    {
        // Setup the ray
        RayDesc ray = (RayDesc)0;
        ray.Origin = cameraPosition;
        ray.TMin = 0.f;
        ray.TMax = 1e27f;

        // Random numbers are only generated when AA is enabled (more than 1 path per pixel)
        if (cameraNumPaths > 1)
        {
            // Generate offsets in [0, 1]
            offsets.x = GetRandomNumber(seed);
            offsets.y = GetRandomNumber(seed);
        }

        // Compute the primary ray direction
        float  halfHeight = cameraTanHalfFovY;
        float  halfWidth = (cameraAspect * halfHeight);
        float3 lowerLeftCorner = cameraPosition - (halfWidth * cameraRight) - (halfHeight * cameraUp) + cameraForward;
        float3 horizontal = (2.f * halfWidth) * cameraRight;
        float3 vertical = (2.f * halfHeight) * cameraUp;

        float s = ((float)LaunchIndex.x + offsets.x) / (float)LaunchDimensions.x;
        float t = 1.f - (((float)LaunchIndex.y + offsets.y) / (float)LaunchDimensions.y);

        ray.Direction = (lowerLeftCorner + s * horizontal + t * vertical) - ray.Origin;

        // Trace!
        color += TracePath(ray, seed);
    }

    // Progressive Accumulation
    float numPaths = (float)cameraNumPaths;
    if (FrameNumber > 1)
    {
        // Read the previous color and number of paths
        float3 previousColor = PTAccumulation[LaunchIndex.xy].xyz;
        float  numPreviousPaths = PTAccumulation[LaunchIndex.xy].w;

        // Add in the new color and number of paths
        color = (previousColor + color);
        numPaths = (numPreviousPaths + numPaths);

        // Store to the accumulation buffer
        PTAccumulation[LaunchIndex.xy] = float4(color, numPaths);
    }
    else
    {
        // Clear the accumulation buffer when moving
        PTAccumulation[LaunchIndex.xy] = float4(0.f, 0.f, 0.f, 0.f);
    }

    // Normalize
    color /= numPaths;

    // Apply exposure
    if (UseExposure)
    {
        color *= Exposure;
    }

    // Apply tonemapping
    if (UseTonemapping)
    {
        color = ACESFilm(color);
    }

    // Add noise to handle SDR color banding
    if (UseDithering)
    {
        color += GetLowDiscrepancyBlueNoise(int2(LaunchIndex), FrameNumber, 1.f / 256.f, BlueNoiseRGB);
    }

    // Gamma correct
    color = LinearToSRGB(color);

    // Store result
    PTOutput[LaunchIndex] = float4(color, 1.f);
}
