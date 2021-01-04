/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RS_COMMON_HLSL
#define RS_COMMON_HLSL

struct DDGIVolumeDescGPUGroup
{
    DDGIVolumeDescGPU volumes[4];
};

cbuffer MultiVolumeCB : register(b0, space1)
{
    uint volumeSelect;
}

// ---- DDGIVolume Entries -----------
ConstantBuffer<DDGIVolumeDescGPUGroup>      DDGIVolumes              : register(b1, space1);

// Bindless descriptor arrays
Texture2D<float4> SRVArray[] : register(t0, space1);
RWTexture2D<float4> UAVArrayFloat[] : register(u0, space1);
RWTexture2D<uint> UAVArrayUint[] : register(u0, space2);

// -----------------------------------

// See DescriptorHeapConstants in Common.h and make sure it matches DESCRIPTORS_PER_VOLUME here
#define DESCRIPTORS_PER_VOLUME 7
#define RADIANCE_UAV_OFFSET 0
#define IRRADIANCE_UAV_OFFSET 1
#define DISTANCES_UAV_OFFSET 2
#define OFFSETS_UAV_OFFSET 3
#define STATES_UAV_OFFSET 4
#define IRRADIANCE_SRV_OFFSET 5
#define DISTANCES_SRV_OFFSET 6

RWTexture2D<float4> GetDDGIProbeRTRadianceUAV(uint volumeIndex)
{
    return UAVArrayFloat[volumeIndex * DESCRIPTORS_PER_VOLUME + RADIANCE_UAV_OFFSET];
}

RWTexture2D<float4> GetDDGIProbeOffsetsUAV(uint volumeIndex)
{
    return UAVArrayFloat[volumeIndex * DESCRIPTORS_PER_VOLUME + OFFSETS_UAV_OFFSET];
}

RWTexture2D<uint> GetDDGIProbeStatesUAV(uint volumeIndex)
{
    return UAVArrayUint[volumeIndex * DESCRIPTORS_PER_VOLUME + STATES_UAV_OFFSET];
}

Texture2D<float4> GetDDGIProbeIrradianceSRV(uint volumeIndex)
{
    return SRVArray[volumeIndex * DESCRIPTORS_PER_VOLUME + IRRADIANCE_SRV_OFFSET];
}

Texture2D<float4> GetDDGIProbeDistanceSRV(uint volumeIndex)
{
    return SRVArray[volumeIndex * DESCRIPTORS_PER_VOLUME + DISTANCES_SRV_OFFSET];
}

#endif /* RS_COMMON_HLSL */
