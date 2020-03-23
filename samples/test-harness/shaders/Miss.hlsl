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

// ---[ Miss Shader ]---

[shader("miss")]
void Miss(inout PayloadData payload)
{
    // Basic Sky
    //float3 RayDirection = WorldRayDirection();
    //float t = 0.5f * (normalize(RayDirection).y + 1.f);
    //payload.baseColor = (1.f - t) * float3(0.1f, 0.3f, 1.f) + t * float3(0.5f, 0.7f, 1.f);

    payload.baseColor = float3(0.f, 0.f, 0.f);
    payload.hitT = -1.f;
}
