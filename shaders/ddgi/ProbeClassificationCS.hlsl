/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

// For example usage, see DDGI_[D3D12|VK].cpp::CompileDDGIVolumeShaders() function.

// -------- CONFIG FILE ---------------------------------------------------------------------------

#if RTXGI_DDGI_USE_SHADER_CONFIG_FILE
#include <DDGIShaderConfig.h>
#endif

// -------- DEFINE VALIDATION ---------------------------------------------------------------------

#include "include/validation/ProbeClassificationDefines.hlsl"

// -------- REGISTER DECLARATIONS -----------------------------------------------------------------

#if RTXGI_DDGI_SHADER_REFLECTION || defined(__spirv__)

    // Don't declare registers when using reflection or cross-compiling to SPIRV
    #define VOLUME_CONSTS_REG_DECL 
    #if RTXGI_DDGI_BINDLESS_RESOURCES
        #define VOLUME_RESOURCES_REG_DECL 
        #define RWTEX2DARRAY_REG_DECL 
    #else
        #define RAY_DATA_REG_DECL 
        #define PROBE_DATA_REG_DECL 
    #endif

#else

    // Declare registers and spaces when using D3D *without* reflection
    #define VOLUME_CONSTS_REG_DECL : register(VOLUME_CONSTS_REGISTER, VOLUME_CONSTS_SPACE)
    #if RTXGI_DDGI_BINDLESS_RESOURCES
        #if RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS
            #define VOLUME_RESOURCES_REG_DECL : register(VOLUME_RESOURCES_REGISTER, VOLUME_RESOURCES_SPACE)
            #define RWTEX2DARRAY_REG_DECL : register(RWTEX2DARRAY_REGISTER, RWTEX2DARRAY_SPACE)
        #endif
    #else
        #define RAY_DATA_REG_DECL : register(RAY_DATA_REGISTER, RAY_DATA_SPACE)
        #define PROBE_DATA_REG_DECL : register(PROBE_DATA_REGISTER, PROBE_DATA_SPACE)
    #endif

#endif // RTXGI_DDGI_SHADER_REFLECTION || SPIRV

// -------- ROOT / PUSH CONSTANT DECLARATIONS -----------------------------------------------------

#include "include/ProbeCommon.hlsl"
#include "include/DDGIRootConstants.hlsl"

// -------- RESOURCE DECLARATIONS -----------------------------------------------------------------

#if RTXGI_DDGI_BINDLESS_RESOURCES
    #if RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS

        // DDGIVolume constants structured buffer
        RTXGI_VK_BINDING(VOLUME_CONSTS_REGISTER, VOLUME_CONSTS_SPACE)
        StructuredBuffer<DDGIVolumeDescGPUPacked> DDGIVolumes VOLUME_CONSTS_REG_DECL;

        // DDGIVolume resource indices structured buffer
        RTXGI_VK_BINDING(VOLUME_RESOURCES_REGISTER, VOLUME_RESOURCES_SPACE)
        StructuredBuffer<DDGIVolumeResourceIndices> DDGIVolumeBindless VOLUME_RESOURCES_REG_DECL;

        // DDGIVolume ray data, probe irradiance, probe distance, and probe data
        RTXGI_VK_BINDING(RWTEX2DARRAY_REGISTER, RWTEX2DARRAY_SPACE)
        RWTexture2DArray<float4> RWTex2DArray[] RWTEX2DARRAY_REG_DECL;

    #endif
#else

    // DDGIVolume constants structured buffer
    RTXGI_VK_BINDING(VOLUME_CONSTS_REGISTER, VOLUME_CONSTS_SPACE)
    StructuredBuffer<DDGIVolumeDescGPUPacked> DDGIVolumes VOLUME_CONSTS_REG_DECL;

    // DDGIVolume ray data (radiance and hit distances)
    RTXGI_VK_BINDING(RAY_DATA_REGISTER, RAY_DATA_SPACE)
    RWTexture2DArray<float4> RayData RAY_DATA_REG_DECL;

    // Probe data (world-space offsets and classification states)
    RTXGI_VK_BINDING(PROBE_DATA_REGISTER, PROBE_DATA_SPACE)
    RWTexture2DArray<float4> ProbeData PROBE_DATA_REG_DECL;

#endif // RTXGI_DDGI_BINDLESS_RESOURCES

[numthreads(32, 1, 1)]
void DDGIProbeClassificationCS(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    // Get the volume's index
    uint volumeIndex = GetDDGIVolumeIndex();

    // Compute the probe index for this thread
    uint probeIndex = DispatchThreadID.x;

#if RTXGI_DDGI_BINDLESS_RESOURCES
    #if RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_DESCRIPTOR_HEAP
        // Get the DDGIVolume constants structured buffer from the descriptor heap (SM6.6+ only)
        StructuredBuffer<DDGIVolumeDescGPUPacked> DDGIVolumes = ResourceDescriptorHeap[GetDDGIVolumeConstantsIndex()];
    #endif
#endif

    // Get the volume's constants
    DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumes[volumeIndex]);

    // Early out: if this thread maps past the number of probes in the volume
    int numProbes = (volume.probeCounts.x * volume.probeCounts.y * volume.probeCounts.z);
    if (probeIndex >= numProbes) return;

#if RTXGI_DDGI_BINDLESS_RESOURCES
    #if RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_DESCRIPTOR_HEAP
        // Get the volume's resource indices from the descriptor heap (SM6.6+ only)
        StructuredBuffer<DDGIVolumeResourceIndices> DDGIVolumeBindless = ResourceDescriptorHeap[GetDDGIVolumeResourceIndicesIndex()];
        DDGIVolumeResourceIndices resourceIndices = DDGIVolumeBindless[volumeIndex];

        // Get the volume's ray data and probe data UAVs from the descriptor heap (SM6.6+ only)
        RWTexture2DArray<float4> RayData = ResourceDescriptorHeap[resourceIndices.rayDataUAVIndex];
        RWTexture2DArray<float4> ProbeData = ResourceDescriptorHeap[resourceIndices.probeDataUAVIndex];
    #elif RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS
        // Get the volume's resource indices
        DDGIVolumeResourceIndices resourceIndices = DDGIVolumeBindless[volumeIndex];

        // Get the volume's ray data and probe data UAVs
        RWTexture2DArray<float4> RayData = RWTex2DArray[resourceIndices.rayDataUAVIndex];
        RWTexture2DArray<float4> ProbeData = RWTex2DArray[resourceIndices.probeDataUAVIndex];
    #endif
#endif

    // Get the number of ray samples to inspect
    int numRays = min(volume.probeNumRays, RTXGI_DDGI_NUM_FIXED_RAYS);

    int rayIndex;
    int backfaceCount = 0;
    float hitDistances[RTXGI_DDGI_NUM_FIXED_RAYS];

    // Load the hit distances and count the number of backface hits
    for (rayIndex = 0; rayIndex < RTXGI_DDGI_NUM_FIXED_RAYS; rayIndex++)
    {
        // Get the coordinates for the probe ray in the RayData texture array
        int3 rayDataTexCoords = DDGIGetRayDataTexelCoords(rayIndex, probeIndex, volume);

        // Load the hit distance for the ray
        hitDistances[rayIndex] = DDGILoadProbeRayDistance(RayData, rayDataTexCoords, volume);

        // Increment the count if a backface is hit
        backfaceCount += (hitDistances[rayIndex] < 0.f);
    }

    // Get the probe's texel coordinates in the Probe Data texture array
    uint3 outputCoords = DDGIGetProbeTexelCoords(probeIndex, volume);

    // Early out: number of backface hits has been exceeded. The probe is probably inside geometry.
    if(((float)backfaceCount / (float)RTXGI_DDGI_NUM_FIXED_RAYS) > volume.probeFixedRayBackfaceThreshold)
    {
        ProbeData[outputCoords].w = RTXGI_DDGI_PROBE_STATE_INACTIVE;
        return;
    }

    // Get the world space position of the probe
    int3 probeCoords = DDGIGetProbeCoords(probeIndex, volume);
    float3 probeWorldPosition = DDGIGetProbeWorldPosition(probeCoords, volume, ProbeData);

    // Determine if there is nearby geometry in the probe's voxel.
    // Iterate over the probe rays and compare ray hit distances with
    // the distance(s) to the probe's voxel planes.
    for (rayIndex = 0; rayIndex < RTXGI_DDGI_NUM_FIXED_RAYS; rayIndex++)
    {
        // Skip backface hits
        if(hitDistances[rayIndex] < 0) continue;

        // Get the direction of the "fixed" ray
        float3 direction = DDGIGetProbeRayDirection(rayIndex, volume);

        // Get the plane normals
        float3 xNormal = float3(direction.x / max(abs(direction.x), 0.000001f), 0.f, 0.f);
        float3 yNormal = float3(0.f, direction.y / max(abs(direction.y), 0.000001f), 0.f);
        float3 zNormal = float3(0.f, 0.f, direction.z / max(abs(direction.z), 0.000001f));

        // Get the relevant planes to intersect
        float3 p0x = probeWorldPosition + (volume.probeSpacing.x * xNormal);
        float3 p0y = probeWorldPosition + (volume.probeSpacing.y * yNormal);
        float3 p0z = probeWorldPosition + (volume.probeSpacing.z * zNormal);

        // Get the ray's intersection distance with each plane
        float3 distances = 
        {
            dot((p0x - probeWorldPosition), xNormal) / max(dot(direction, xNormal), 0.000001f),
            dot((p0y - probeWorldPosition), yNormal) / max(dot(direction, yNormal), 0.000001f),
            dot((p0z - probeWorldPosition), zNormal) / max(dot(direction, zNormal), 0.000001f)
        };

        // If the ray is parallel to the plane, it will never intersect
        // Set the distance to a very large number for those planes
        if (distances.x == 0.f) distances.x = 1e27f;
        if (distances.y == 0.f) distances.y = 1e27f;
        if (distances.z == 0.f) distances.z = 1e27f;

        // Get the distance to the closest plane intersection
        float maxDistance = min(distances.x, min(distances.y, distances.z));

        // If the hit distance is less than the closest plane intersection, the probe should be active
        if(hitDistances[rayIndex] <= maxDistance)
        {
            ProbeData[outputCoords].w = RTXGI_DDGI_PROBE_STATE_ACTIVE;
            return;
        }
    }

    ProbeData[outputCoords].w = RTXGI_DDGI_PROBE_STATE_INACTIVE;
}


[numthreads(32, 1, 1)]
void DDGIProbeClassificationResetCS(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    // Get the volume's index
    uint volumeIndex = GetDDGIVolumeIndex();

#if RTXGI_DDGI_BINDLESS_RESOURCES
    #if RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_DESCRIPTOR_HEAP
        // Get the DDGIVolume constants structured buffer from the descriptor heap (SM6.6+ only)
        StructuredBuffer<DDGIVolumeDescGPUPacked> DDGIVolumes = ResourceDescriptorHeap[GetDDGIVolumeConstantsIndex()];
    #endif
#endif

    // Get the volume's constants
    DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumes[volumeIndex]);

#if RTXGI_DDGI_BINDLESS_RESOURCES
    #if RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_DESCRIPTOR_HEAP
        // Get the volume's resource indices from the descriptor heap (SM6.6+ only)
        StructuredBuffer<DDGIVolumeResourceIndices> DDGIVolumeBindless = ResourceDescriptorHeap[GetDDGIVolumeResourceIndicesIndex()];
        DDGIVolumeResourceIndices resourceIndices = DDGIVolumeBindless[volumeIndex];

        // Get the volume's probe data texture array UAV
        RWTexture2DArray<float4> ProbeData = ResourceDescriptorHeap[resourceIndices.probeDataUAVIndex];
    #elif RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS

        // Get the volume's resource indices
        DDGIVolumeResourceIndices resourceIndices = DDGIVolumeBindless[volumeIndex];

        // Get the volume's probe data texture array UAV
        RWTexture2DArray<float4> ProbeData = RWTex2DArray[resourceIndices.probeDataUAVIndex];
    #endif
#endif

    // Get the probe's texel coordinates in the Probe Data texture
    uint3 outputCoords = DDGIGetProbeTexelCoords(DispatchThreadID.x, volume);

    // Set all probes to active
    ProbeData[outputCoords].w = RTXGI_DDGI_PROBE_STATE_ACTIVE;
}
