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
#include "include/Lighting.hlsl"
#include "include/Random.hlsl"
#include "include/RayTracing.hlsl"

#include "../../../rtxgi-sdk/shaders/Common.hlsl"

// ---[ Helper Functions ]---

float3 TracePath(RayDesc ray, uint seed)
{
    float3 throughput = float3(1.f, 1.f, 1.f);
    float3 color = float3(0.f, 0.f, 0.f);

    // Get the lights and scene acceleration structure
    StructuredBuffer<Light> Lights = GetLights();
    RaytracingAccelerationStructure SceneTLAS = GetAccelerationStructure(SCENE_TLAS_INDEX);

    for (int bounceIndex = 0; bounceIndex < GetGlobalConst(pt, numBounces); bounceIndex++)
    {
        // Trace the ray
        PackedPayload packedPayload = (PackedPayload)0;

    #if GFX_NVAPI
        if (GetPTShaderExecutionReordering())
        {
            NvHitObject hit;
            NvTraceRayHitObject(
                SceneTLAS,
                RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
                0xFF,
                0,
                0,
                0,
                ray,
                packedPayload,
                hit);
            NvReorderThread(hit, 0, 0);
            NvInvokeHitObject(SceneTLAS, hit, packedPayload);
        }
        else
        {
            TraceRay(
                SceneTLAS,
                RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
                0xFF,
                0,
                0,
                0,
                ray,
                packedPayload);
        }
    #else
        TraceRay(
            SceneTLAS,
            RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
            0xFF,
            0,
            0,
            0,
            ray,
            packedPayload);
    #endif

        // Unpack the payload
        Payload payload = UnpackPayload(packedPayload);

        // Miss, exit loop
        if (payload.hitT < 0.f)
        {
            color += GetGlobalConst(app, skyRadiance) * throughput;
            break;
        }

        // Direct Lighting
        float3 diffuse = DirectDiffuseLighting(payload, GetGlobalConst(pt, rayNormalBias), GetGlobalConst(pt, rayViewBias), SceneTLAS, Lights);

        // Attenuate the color
        color += diffuse * throughput;

        // Increment the seed
        seed += bounceIndex;

        // Set the ray origin for the next bounce
        ray.Origin = payload.worldPosition;
        ray.Origin += (payload.normal * GetGlobalConst(pt, rayNormalBias) - (normalize(ray.Direction) * GetGlobalConst(pt, rayViewBias)));

        // Select random directions on the hemisphere with a cos(theta) distribution and then compute throughput
        ray.Direction = GetRandomCosineDirectionOnHemisphere(payload.normal, seed);

        // Select random directions on the hemisphere with a uniform distribution and then compute throughput
        // [BRDF * cos(theta)] / PDF, where PDF = 1 / Area of Integration = 1 / 2PI
        //ray.Direction = GetRandomDirectionOnHemisphere(payload.normal, seed);
        //throughput *= (payload.albedo / PI) * dot(payload.normal, ray.Direction) * (2.f * PI);

        // Perfectly diffuse reflectors don't exist in the real world.
        // Limit the BRDF albedo to a maximum value to account for the energy loss at each bounce.
        float maxAlbedo = 0.9f;
        throughput *= min(payload.albedo, float3(maxAlbedo, maxAlbedo, maxAlbedo));

        // End the path if the throughput is close to zero
        if (RTXGIMaxComponent(throughput) <= 0.005f) break;
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
    seed *= GetGlobalConst(app, frameNumber);

    // Get the (bindless) resources
    RWTexture2D<float4> PTOutput = GetRWTex2D(PT_OUTPUT_INDEX);
    RWTexture2D<float4> PTAccumulation = GetRWTex2D(PT_ACCUMULATION_INDEX);
    Texture2D<float4> BlueNoise = GetTex2D(BLUE_NOISE_INDEX);

    // Trace the paths for this pixel
    float3 color = float3(0.f, 0.f, 0.f);
    float2 offsets = float2(0.5f, 0.5f);
    for (int sampleIndex = 0; sampleIndex < GetPTSamplesPerPixel(); sampleIndex++)
    {
        // Setup the ray
        RayDesc ray = (RayDesc)0;
        ray.Origin = GetCamera().position;
        ray.TMin = 0.f;
        ray.TMax = 1e27f;

        // Random numbers are only generated when AA is enabled
        if (GetPTAntialiasing())
        {
            // Generate offsets in [0, 1]
            offsets.x = GetRandomNumber(seed);
            offsets.y = GetRandomNumber(seed);
        }

        // Compute the primary ray direction
        float  halfHeight = GetCamera().tanHalfFovY;
        float  halfWidth = (GetCamera().aspect * halfHeight);
        float3 lowerLeftCorner = GetCamera().position - (halfWidth * GetCamera().right) - (halfHeight * GetCamera().up) + GetCamera().forward;
        float3 horizontal = (2.f * halfWidth) * GetCamera().right;
        float3 vertical = (2.f * halfHeight) * GetCamera().up;

        float s = ((float)LaunchIndex.x + offsets.x) / (float)LaunchDimensions.x;
        float t = 1.f - (((float)LaunchIndex.y + offsets.y) / (float)LaunchDimensions.y);

        ray.Direction = (lowerLeftCorner + s * horizontal + t * vertical) - ray.Origin;

        // Trace!
        color += TracePath(ray, seed);
    }

    // Progressive Accumulation
    float numPaths = (float)GetPTSamplesPerPixel();
    if (GetGlobalConst(app, frameNumber) > 1)
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

    // Get the post processing useFlags
    uint ppUseFlags = GetGlobalConst(post, useFlags);

    // Early out, no post processing
    if (ppUseFlags == POSTPROCESS_FLAG_USE_NONE)
    {
        PTOutput[LaunchIndex] = float4(color, 1.f);
        return;
    }

    // Exposure
    if (ppUseFlags & POSTPROCESS_FLAG_USE_EXPOSURE)
    {
        color *= GetGlobalConst(post, exposure);
    }

    // Tonemapping
    if (ppUseFlags & POSTPROCESS_FLAG_USE_TONEMAPPING)
    {
        color = ACESFilm(color);
    }

    // Dither to reduce SDR color banding
    if (ppUseFlags & POSTPROCESS_FLAG_USE_DITHER)
    {
        color += GetLowDiscrepancyBlueNoise(int2(LaunchIndex), GetGlobalConst(app, frameNumber), 1.f / 256.f, BlueNoise);
    }

    // Gamma correction
    if (ppUseFlags & POSTPROCESS_FLAG_USE_GAMMA)
    {
        color = LinearToSRGB(color);
    }

    // Store result
    PTOutput[LaunchIndex] = float4(color, 1.f);
}
