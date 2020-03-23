/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "include/RTCommon.hlsl"
#include "include/RTGlobalRS.hlsl"
#include "include/LightingCommon.hlsl"

// ---[ Ray Generation Shader ]---

[shader("raygeneration")]
void RayGen()
{
    uint2 LaunchIndex = DispatchRaysIndex().xy;
    uint2 LaunchDimensions = DispatchRaysDimensions().xy;

    RayDesc ray;
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

    // Primary Ray Trace
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

    RTGBufferA[LaunchIndex] = float4(payload.baseColor, payload.hitT > -1.f ? 1.f : 0.f);
    RTGBufferB[LaunchIndex] = float4(payload.worldPosition, payload.hitT);

    if (payload.hitT > -1.f)
    {
        // Compute direct diffuse lighting
        float3 diffuse = DirectDiffuseLighting(
            payload.baseColor,
            payload.worldPosition,
            payload.normal,
            NormalBias,
            ViewBias,
            SceneBVH);
        
        RTGBufferC[LaunchIndex] = float4(payload.normal, 1.f);
        RTGBufferD[LaunchIndex] = float4(saturate(diffuse), 1.f);
    }
    else
    {
        RTGBufferC[LaunchIndex] = float4(0.0f, 0.0f, 0.0f, 1.f);
        RTGBufferD[LaunchIndex] = float4(0.0f, 0.0f, 0.0f, 1.f);
    }
}
