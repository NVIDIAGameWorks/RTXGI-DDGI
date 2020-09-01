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
#include "include/RTLocalRS.hlsl"

 // ---[ Any Hit Shader ]---

[shader("anyhit")]
void AHS(inout PackedPayload payload, BuiltInTriangleIntersectionAttributes attrib)
{
    float alpha = opacity;

    if (alphaMode == 2)
    {
        // Load and interpolate the triangle's texture coordinates
        float3 barycentrics = float3((1.f - attrib.barycentrics.x - attrib.barycentrics.y), attrib.barycentrics.x, attrib.barycentrics.y);        
        float2 uv0 = GetInterpolatedUV0(PrimitiveIndex(), barycentrics);

        if (baseColorTexIdx > -1)
        {
            alpha = Textures[baseColorTexIdx].SampleLevel(BilinearSampler, uv0, 0).a;
        }
    }

    if (alpha < alphaCutoff) IgnoreHit();
}

