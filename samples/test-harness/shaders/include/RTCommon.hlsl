/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RT_COMMON_HLSL
#define RT_COMMON_HLSL

struct PayloadData
{
    float3    baseColor;
    float3    worldPosition;
    float3    normal;
    float     hitT;
    uint      hitKind;
    uint      instanceIndex;
};

#endif /* RT_COMMON_HLSL */
