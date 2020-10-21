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

/**
* Return the given float value as an unsigned integer within the given numerical scale.
*/
uint RTXGIFloatToUint(float v, float scale)
{
    return (uint)floor(v * scale + 0.5f);
}

/**
* Pack a float3 into a 32-bit unsigned integer.
* Red and green channels use 11 bits, the blue channel uses 10 bits.
* Compliment of RTXGIUintToFloat3().
*/
uint RTXGIFloat3ToUint(float3 input)
{
    return (RTXGIFloatToUint(input.r, 2047.f)) | (RTXGIFloatToUint(input.g, 2047) << 11) | (RTXGIFloatToUint(input.b, 1023) << 22);
}

/**
* Unpack a packed 32-bit unsigned integer to a float3.
* Compliment of RTXGIFloat3ToUint().
*/
float3 RTXGIUintToFloat3(uint packed)
{
    float3 unpacked;
    unpacked.x = (float)(packed & 0x000007FF) / 2047.f;
    unpacked.y = (float)((packed >> 11) & 0x000007FF) / 2047.f;
    unpacked.z = (float)((packed >> 22) & 0x000003FF) / 1023.f;
    return unpacked;
}

#endif /* RTXGI_COMMON_HLSL */
