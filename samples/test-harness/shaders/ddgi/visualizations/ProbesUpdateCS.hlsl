/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "../../include/Descriptors.hlsl"

#include "../../../../../rtxgi-sdk/shaders/ddgi/include/ProbeCommon.hlsl"
#include "../../../../../rtxgi-sdk/shaders/ddgi/include/DDGIRootConstants.hlsl"

[numthreads(32, 1, 1)]
void CS(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    // Get the DDGIVolume index from root/push constants
    uint volumeIndex = GetDDGIVolumeIndex();

    // Get the DDGIVolume structured buffers
    StructuredBuffer<DDGIVolumeDescGPUPacked> DDGIVolumes = GetDDGIVolumeConstants(GetDDGIVolumeConstantsIndex());
    StructuredBuffer<DDGIVolumeResourceIndices> DDGIVolumeBindless = GetDDGIVolumeResourceIndices(GetDDGIVolumeResourceIndicesIndex());

    // Load and unpack the DDGIVolume's constants
    DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumes[volumeIndex]);

    // Get the number of probes
    uint numProbes = (volume.probeCounts.x * volume.probeCounts.y * volume.probeCounts.z);

    // Early out: processed all probes, a probe doesn't exist for this thread
    if(DispatchThreadID.x >= numProbes) return;

    // Get the DDGIVolume's bindless resource indices
    DDGIVolumeResourceIndices resourceIndices = DDGIVolumeBindless[volumeIndex];

    // Get the probe data texture array
    Texture2DArray<float4> ProbeData = GetTex2DArray(resourceIndices.probeDataSRVIndex);

    // Get the probe's grid coordinates
    float3 probeCoords = DDGIGetProbeCoords(DispatchThreadID.x, volume);

    // Get the probe's world position from the probe index
    float3 probeWorldPosition = DDGIGetProbeWorldPosition(probeCoords, volume, ProbeData);

    // Get the probe radius
    float probeRadius = GetGlobalConst(ddgivis, probeRadius);

    // Get the instance offset (where one volume's probes end and another begin)
    uint instanceOffset = GetGlobalConst(ddgivis, instanceOffset);

    // Get the TLAS Instances structured buffer
    RWStructuredBuffer<TLASInstance> RWInstances = GetDDGIProbeVisTLASInstances();

    // Set the probe's transform
    RWInstances[(instanceOffset + DispatchThreadID.x)].transform = float3x4(
        probeRadius, 0.f, 0.f, probeWorldPosition.x,
        0.f, probeRadius, 0.f, probeWorldPosition.y,
        0.f, 0.f, probeRadius, probeWorldPosition.z);

}
