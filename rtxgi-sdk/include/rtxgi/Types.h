/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

namespace rtxgi
{

    struct uint2
    {
        unsigned int x;
        unsigned int y;
    };

    struct uint3
    {
        unsigned int x;
        unsigned int y;
        unsigned int z;
    };

    struct uint4
    {
        unsigned int x;
        unsigned int y;
        unsigned int z;
        unsigned int w;
    };

    struct int2
    {
        int x;
        int y;
    };

    struct int3
    {
        int x;
        int y;
        int z;
    };

    struct int4
    {
        int x;
        int y;
        int z;
        int w;
    };

    struct float2
    {
        float x;
        float y;
    };

    struct float3
    {
        float x;
        float y;
        float z;
    };

    struct float4
    {
        float x;
        float y;
        float z;
        float w;
    };

    struct float4x4
    {
        float4 r0;
        float4 r1;
        float4 r2;
        float4 r3;
    };

    struct AABB
    {
        float3 min;
        float3 max;
    };

}
