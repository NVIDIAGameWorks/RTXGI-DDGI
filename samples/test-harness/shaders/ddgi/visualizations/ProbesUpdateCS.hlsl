/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "Descriptors.hlsl"

#include "../../../../../rtxgi-sdk/shaders/ddgi/include/ProbeCommon.hlsl"

[numthreads(1, 1, 1)]
void CS(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    // Get the DDGIVolume's constants
    uint volumeIndex = GetGlobalConst(ddgivis, volumeIndex);
    DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumes[volumeIndex]);

    // Get the probe data texture
    Texture2D<float4> ProbeData = GetDDGIVolumeProbeDataSRV(volumeIndex);

    // Get the probe's grid coordinates
    float3 probeCoords = DDGIGetProbeCoords(DispatchThreadID.x, volume);

    // Get the probe's world position from the probe index
    float3 probeWorldPosition = DDGIGetProbeWorldPosition(probeCoords, volume, ProbeData);

    // Get the probe radius
    float probeRadius = GetGlobalConst(ddgivis, probeRadius);

    // Get the instance offset (where one volume's probes end and another begin)
    uint instanceOffset = GetGlobalConst(ddgivis, instanceOffset);

    // Set the probe's transform
    Instances[instanceOffset + DispatchThreadID.x].transform = float3x4(
        probeRadius, 0.f, 0.f, probeWorldPosition.x,
        0.f, probeRadius, 0.f, probeWorldPosition.y,
        0.f, 0.f, probeRadius, probeWorldPosition.z);

}
