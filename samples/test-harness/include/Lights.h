/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef LIGHTS_H
#define LIGHTS_H

#ifdef LIGHTS_CPU
#include <rtxgi/Types.h>
using namespace rtxgi;
#endif

struct DirectionalLightDescGPU
{
    float3 direction;
    float  power;
    float3 color;
    float  directionalPad1;
};

struct PointLightDescGPU
{
    float3 position;
    float  power;
    float3 color;
    float  maxDistance;
};

struct SpotLightDescGPU
{
    float3 position;
    float  power;
    float3 direction;
    float  umbraAngle;
    float3 color;
    float  penumbraAngle;
    float  maxDistance;
    float3 spotPad;
};

#endif /* LIGHTS_H */
