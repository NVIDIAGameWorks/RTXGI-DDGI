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
#include "../../../include/rtxgi/ddgi/DDGIRootConstants.h"
#include "../../../include/rtxgi/ddgi/DDGIVolumeDescGPU.h"

//------------------------------------------------------------------------
// Defines
//------------------------------------------------------------------------

// Bindless resource implementation type
#define RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS 0
#define RTXGI_BINDLESS_TYPE_DESCRIPTOR_HEAP 1

// Texture formats (matches EDDGIVolumeTextureFormat)
#define RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_U32 0
#define RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F16 1
#define RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F16x2 2
#define RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F16x4 3
#define RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32 4
#define RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x2 5
#define RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x4 6

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
