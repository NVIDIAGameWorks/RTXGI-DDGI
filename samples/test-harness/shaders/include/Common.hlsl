/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef COMMON_HLSL
#define COMMON_HLSL

static const float PI = 3.1415926535897932f;
static const float TWO_PI = 6.2831853071795864f;

static const float COMPOSITE_FLAG_IGNORE_PIXEL = 0.2f;
static const float COMPOSITE_FLAG_POSTPROCESS_PIXEL = 0.5f;
static const float COMPOSITE_FLAG_LIGHT_PIXEL = 0.8f;

#define RTXGI_DDGI_VISUALIZE_PROBE_IRRADIANCE 0
#define RTXGI_DDGI_VISUALIZE_PROBE_DISTANCE 1

float3 LessThan(float3 f, float value)
{
    return float3(
        (f.x < value) ? 1.f : 0.f,
        (f.y < value) ? 1.f : 0.f,
        (f.z < value) ? 1.f : 0.f);
}

float3 LinearToSRGB(float3 rgb)
{
    rgb = clamp(rgb, 0.f, 1.f);
    return lerp(
        pow(rgb * 1.055f, 1.f / 2.4f) - 0.055f,
        rgb * 12.92f,
        LessThan(rgb, 0.0031308f)
    );
}

float3 SRGBToLinear(float3 rgb)
{
    rgb = clamp(rgb, 0.f, 1.f);
    return lerp(
        pow((rgb + 0.055f) / 1.055f, 2.4f),
        rgb / 12.92f,
        LessThan(rgb, 0.04045f)
    );
}

// ACES tone mapping curve fit to go from HDR to LDR
//https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x*(a*x + b)) / (x*(c*x + d) + e));
}

#endif // COMMON_HLSL
