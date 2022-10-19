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

#include "include/validation/ProbeRelocationDefines.hlsl"

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

    // DDGIVolume Probe data (world-space offsets and classification states)
    RTXGI_VK_BINDING(PROBE_DATA_REGISTER, PROBE_DATA_SPACE)
    RWTexture2DArray<float4> ProbeData PROBE_DATA_REG_DECL;

#endif // RTXGI_DDGI_BINDLESS_RESOURCES

[numthreads(32, 1, 1)]
void DDGIProbeRelocationCS(uint3 DispatchThreadID : SV_DispatchThreadID)
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

        // Get the volume's ray data texture and probe data texture array UAVs
        RWTexture2DArray<float4> RayData = RWTex2DArray[resourceIndices.rayDataUAVIndex];
        RWTexture2DArray<float4> ProbeData = RWTex2DArray[resourceIndices.probeDataUAVIndex];
    #endif
#endif

    // Get the probe's texel coordinates in the Probe Data texture array
    uint3 outputCoords = DDGIGetProbeTexelCoords(probeIndex, volume);

    // Read the current world position offset
    float3 offset = DDGILoadProbeDataOffset(ProbeData, outputCoords, volume);

    // Initialize variables
    int   closestBackfaceIndex = -1;
    int   closestFrontfaceIndex = -1;
    int   farthestFrontfaceIndex = -1;
    float closestBackfaceDistance = 1e27f;
    float closestFrontfaceDistance = 1e27f;
    float farthestFrontfaceDistance = 0.f;
    float backfaceCount = 0.f;

    // Get the number of rays to inspect
    int numRays = min(volume.probeNumRays, RTXGI_DDGI_NUM_FIXED_RAYS);

    // Iterate over the rays cast for this probe to find the number of backfaces and closest/farthest distances to the probe
    for (int rayIndex = 0; rayIndex < numRays; rayIndex++)
    {
        // Get the coordinates for the probe ray in the RayData texture array
        int3 rayDataTexCoords = DDGIGetRayDataTexelCoords(rayIndex, probeIndex, volume);

        // Load the hit distance for the ray
        float hitDistance = DDGILoadProbeRayDistance(RayData, rayDataTexCoords, volume);

        if (hitDistance < 0.f)
        {
            // Found a backface
            backfaceCount++;

            // Negate the hit distance on a backface hit and scale back to the full distance
            hitDistance = hitDistance * -5.f;
            if (hitDistance < closestBackfaceDistance)
            {
                // Store the closest backface distance and ray index
                closestBackfaceDistance = hitDistance;
                closestBackfaceIndex = rayIndex;
            }
        }
        else
        {
            // Found a frontface
            if (hitDistance < closestFrontfaceDistance)
            {
                // Store the closest frontface distance and ray index
                closestFrontfaceDistance = hitDistance;
                closestFrontfaceIndex = rayIndex;
            }
            else if (hitDistance > farthestFrontfaceDistance)
            {
                // Store the farthest frontface distance and ray index
                farthestFrontfaceDistance = hitDistance;
                farthestFrontfaceIndex = rayIndex;
            }
        }
    }

    float3 fullOffset = float3(1e27f, 1e27f, 1e27f);

    if (closestBackfaceIndex != -1 && ((float)backfaceCount / numRays) > volume.probeFixedRayBackfaceThreshold)
    {
        // If at least one backface triangle is hit AND backfaces are hit by enough probe rays,
        // assume the probe is inside geometry and move it outside of the geometry.
        float3 closestBackfaceDirection = DDGIGetProbeRayDirection(closestBackfaceIndex, volume);
        fullOffset = offset + (closestBackfaceDirection * (closestBackfaceDistance + volume.probeMinFrontfaceDistance * 0.5f));
    }
    else if (closestFrontfaceDistance < volume.probeMinFrontfaceDistance)
    {
        // Don't move the probe if moving towards the farthest frontface will also bring us closer to the nearest frontface
        float3 closestFrontfaceDirection = DDGIGetProbeRayDirection(closestFrontfaceIndex, volume);
        float3 farthestFrontfaceDirection = DDGIGetProbeRayDirection(farthestFrontfaceIndex, volume);

        if (dot(closestFrontfaceDirection, farthestFrontfaceDirection) <= 0.f)
        {
            // Ensures the probe never moves through the farthest frontface
            farthestFrontfaceDistance *= min(farthestFrontfaceDistance, 1.f);
            fullOffset = offset + farthestFrontfaceDirection;
        }
    }
    else if (closestFrontfaceDistance > volume.probeMinFrontfaceDistance)
    {
        // Probe isn't near anything, try to move it back towards zero offset
        float moveBackMargin = min(closestFrontfaceDistance - volume.probeMinFrontfaceDistance, length(offset));
        float3 moveBackDirection = normalize(-offset);
        fullOffset = offset + (moveBackMargin * moveBackDirection);
    }

    // Absolute maximum distance that probe could be moved should satisfy ellipsoid equation:
    // x^2 / probeGridSpacing.x^2 + y^2 / probeGridSpacing.y^2 + z^2 / probeGridSpacing.y^2 < (0.5)^2
    // Clamp to less than maximum distance to avoid degenerate cases
    float3 normalizedOffset = fullOffset / volume.probeSpacing;
    if (dot(normalizedOffset, normalizedOffset) < 0.2025f) // 0.45 * 0.45 == 0.2025
    {
        offset = fullOffset;
    }

    // Write the probe offsets
    DDGIStoreProbeDataOffset(ProbeData, outputCoords, offset, volume);
}

[numthreads(32, 1, 1)]
void DDGIProbeRelocationResetCS(uint3 DispatchThreadID : SV_DispatchThreadID)
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

        // Get the volume's probe data texture array UAV from the descriptor heap (SM6.6+ only)
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

    // Write the probe offset
    ProbeData[outputCoords].xyz = float3(0.f, 0.f, 0.f);
}
