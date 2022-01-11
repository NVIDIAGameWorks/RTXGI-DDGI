/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RANDOM_HLSL
#define RANDOM_HLSL

#include "../../../../rtxgi-sdk/shaders/Common.hlsl"

// ---[ Random Number Generation ]---

/*
 * From Nathan Reed's blog at:
 * http://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11/
*/

uint WangHash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

uint Xorshift(uint seed)
{
    // Xorshift algorithm from George Marsaglia's paper
    seed ^= (seed << 13);
    seed ^= (seed >> 17);
    seed ^= (seed << 5);
    return seed;
}

float GetRandomNumber(inout uint seed)
{
    seed = WangHash(seed);
    return float(Xorshift(seed)) * (1.f / 4294967296.f);
}

/**
* Generate three components of low discrepancy blue noise.
*/
float3 GetLowDiscrepancyBlueNoise(int2 screenPosition, uint frameNumber, float noiseScale, Texture2D<float4> blueNoise)
{
    static const float goldenRatioConjugate = 0.61803398875f;

    // Load random value from a blue noise texture
    float3 rnd = blueNoise.Load(int3(screenPosition % 256, 0)).rgb;

    // Generate a low discrepancy sequence
    rnd = frac(rnd + goldenRatioConjugate * ((frameNumber - 1) % 16));
    
    // Scale the noise magnitude to [0, noiseScale]
    return (rnd * noiseScale);
}

/**
* Generate three components of white noise.
*/
float3 GetWhiteNoise(uint2 screenPosition, uint screenWidth, uint frameNumber, float noiseScale)
{
    // Generate a unique seed on screen position and time
    uint seed = ((screenPosition.y * screenWidth) + screenPosition.x) * frameNumber;

    // Generate uniformly distributed random values in the range [0, 1]
    float3 rnd;
    rnd.x = GetRandomNumber(seed);
    rnd.y = GetRandomNumber(seed);
    rnd.z = GetRandomNumber(seed);

    // Shift the random value into the range [-1, 1]
    rnd = mad(rnd, 2.f, -1.f);

    // Scale the noise magnitude to [-noiseScale, noiseScale]
    return (rnd * noiseScale);
}

/**
* Compute a uniformly distributed random direction on the hemisphere about the given (normal) direction.
*/
float3 GetRandomDirectionOnHemisphere(float3 direction, inout uint seed)
{
    float3 p;
    do
    {
        p.x = GetRandomNumber(seed) * 2.f - 1.f;
        p.y = GetRandomNumber(seed) * 2.f - 1.f;
        p.z = GetRandomNumber(seed) * 2.f - 1.f;

        // Only accept unit length directions to stay inside
        // the unit sphere and be uniformly distributed
    } while (length(p) > 1.f);

    // Direction is on the opposite hemisphere, flip and use it
    if (dot(direction, p) < 0.f) p *= -1.f;
    return normalize(p);
}

/**
* Compute a cosine distributed random direction on the hemisphere about the given (normal) direction.
*/
float3 GetRandomCosineDirectionOnHemisphere(float3 direction, inout uint seed)
{
    // Choose random points on the unit sphere offset along the surface normal
    // to produce a cosine distribution of random directions.
    float a = GetRandomNumber(seed) * RTXGI_2PI;
    float z = GetRandomNumber(seed) * 2.f - 1.f;
    float r = sqrt(1.f - z * z);

    float3 p = float3(r * cos(a), r * sin(a), z) + direction;
    return normalize(p);
}

#endif // RANDOM_HLSL
