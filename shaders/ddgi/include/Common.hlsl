/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RTXGI_DDGI_COMMON_HLSL
#define RTXGI_DDGI_COMMON_HLSL

#include "../../Common.hlsl"
#include "../../Platform.hlsl"
#include "../../../include/rtxgi/Defines.h"
#include "../../../include/rtxgi/ddgi/DDGIConstants.h"
#include "../../../include/rtxgi/ddgi/DDGIVolumeDescGPU.h"

//------------------------------------------------------------------------
// Defines
//------------------------------------------------------------------------

// Probe ray data texture formats
#define RTXGI_DDGI_FORMAT_PROBE_RAY_DATA_R32G32_FLOAT 0
#define RTXGI_DDGI_FORMAT_PROBE_RAY_DATA_R32G32B32A32_FLOAT 1

// Probe irradiance texture formats
#define RTXGI_DDGI_FORMAT_PROBE_IRRADIANCE_R10G10B10A2_FLOAT 0
#define RTXGI_DDGI_FORMAT_PROBE_IRRADIANCE_R16G16B16A16_FLOAT 1
#define RTXGI_DDGI_FORMAT_PROBE_IRRADIANCE_R32G32B32A32_FLOAT 2

// The number of fixed rays that are used by probe relocation and classification.
// These rays directions are always the same to produce temporally stable results.
#define RTXGI_DDGI_NUM_FIXED_RAYS 32

// Probe classification states
#define RTXGI_DDGI_PROBE_STATE_ACTIVE 0     // probe shoots rays and may be sampled by a front facing surface or another probe (recursive irradiance)
#define RTXGI_DDGI_PROBE_STATE_INACTIVE 1   // probe doesn't need to shoot rays, it isn't near a front facing surface

// Volume movement types
#define RTXGI_DDGI_VOLUME_MOVEMENT_TYPE_DEFAULT 0
#define RTXGI_DDGI_VOLUME_MOVEMENT_TYPE_SCROLLING 1

//------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------

bool IsVolumeMovementScrolling(DDGIVolumeDescGPU volume)
{
    return (volume.movementType == RTXGI_DDGI_VOLUME_MOVEMENT_TYPE_SCROLLING);
}

#endif // RTXGI_DDGI_COMMON_HLSL
