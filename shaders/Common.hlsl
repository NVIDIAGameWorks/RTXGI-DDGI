/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RTXGI_COMMON_HLSL
#define RTXGI_COMMON_HLSL

static const float RTXGI_PI = 3.1415926535897932f;
static const float RTXGI_2PI = 6.2831853071795864f;

//------------------------------------------------------------------------
// Math Helpers
//------------------------------------------------------------------------

/**
 * Returns the largest component of the vector.
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

//------------------------------------------------------------------------
// Sampling Helpers
//------------------------------------------------------------------------

/**
 * Computes a low discrepancy spherically distributed direction on the unit sphere,
 * for the given index in a set of samples. Each direction is unique in
 * the set, but the set of directions is always the same.
 */
float3 RTXGISphericalFibonacci(float sampleIndex, float numSamples)
{
    const float b = (sqrt(5.f) * 0.5f + 0.5f) - 1.f;
    float phi = RTXGI_2PI * frac(sampleIndex * b);
    float cosTheta = 1.f - (2.f * sampleIndex + 1.f) * (1.f / numSamples);
    float sinTheta = sqrt(saturate(1.f - (cosTheta * cosTheta)));

    return float3((cos(phi) * sinTheta), (sin(phi) * sinTheta), cosTheta);
}

//------------------------------------------------------------------------
// Format Conversion Helpers
//------------------------------------------------------------------------

/**
 * Return the given float value as an unsigned integer within the given numerical scale.
 */
uint RTXGIFloatToUint(float v, float scale)
{
    return (uint)floor(v * scale + 0.5f);
}

/**
 * Pack a float3 into a 32-bit unsigned integer.
 * All channels use 10 bits and 2 bits are unused.
 * Compliment of RTXGIUintToFloat3().
 */
uint RTXGIFloat3ToUint(float3 input)
{
    return (RTXGIFloatToUint(input.r, 1023.f)) | (RTXGIFloatToUint(input.g, 1023.f) << 10) | (RTXGIFloatToUint(input.b, 1023.f) << 20);
}

/**
 * Unpack a packed 32-bit unsigned integer to a float3.
 * Compliment of RTXGIFloat3ToUint().
 */
float3 RTXGIUintToFloat3(uint input)
{
    float3 output;
    output.x = (float)(input & 0x000003FF) / 1023.f;
    output.y = (float)((input >> 10) & 0x000003FF) / 1023.f;
    output.z = (float)((input >> 20) & 0x000003FF) / 1023.f;
    return output;
}

//------------------------------------------------------------------------
// Quaternion Helpers
//------------------------------------------------------------------------

/**
 * Rotate vector v with quaternion q.
 */
float3 RTXGIQuaternionRotate(float3 v, float4 q)
{
    float3 b = q.xyz;
    float b2 = dot(b, b);
    return (v * (q.w * q.w - b2) + b * (dot(v, b) * 2.f) + cross(b, v) * (q.w * 2.f));
}

/**
 * Quaternion conjugate.
 * For unit quaternions, conjugate equals inverse.
 * Use this to create a quaternion that rotates in the opposite direction.
 */
float4 RTXGIQuaternionConjugate(float4 q)
{
    return float4(-q.xyz, q.w);
}

//------------------------------------------------------------------------
// Luminance Helper
//------------------------------------------------------------------------

/**
 * Convert Linear RGB value to Luminance
 */
float RTXGILinearRGBToLuminance(float3 rgb)
{
    const float3 LuminanceWeights = float3(0.2126, 0.7152, 0.0722);
    return dot(rgb, LuminanceWeights);
}

#endif // RTXGI_COMMON_HLSL
