/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RTXGI_COMMON_HLSL
#define RTXGI_COMMON_HLSL

const static float RTXGI_PI = 3.1415926535897932f;

//------------------------------------------------------------------------
// Math Helpers
//------------------------------------------------------------------------

/**
* Finds the largest component of the vector.
*/
float RTXGIMaxComponent(float3 a)
{
    return max(a.x, max(a.y, a.z));
}

/**
* Returns either -1 or 1 based on the sign of the input value.
* If the input is zero, 1 is returned.
*/
float RTXGISignNotZero(float v)
{
    return (v >= 0.f) ? 1.f : -1.f;
}

/**
* 2-component version of RTXGISignNotZero.
*/
float2 RTXGISignNotZero(float2 v)
{
    return float2(RTXGISignNotZero(v.x), RTXGISignNotZero(v.y));
}

#endif /* RTXGI_COMMON_HLSL */
