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

// -------- MANAGED RESOURCES DEFINES -------------------------------------------------------------

#ifndef RTXGI_DDGI_RESOURCE_MANAGEMENT
#error Required define RTXGI_DDGI_RESOURCE_MANAGEMENT is not defined for ProbeRelocationCS.hlsl!
#endif

#if RTXGI_DDGI_RESOURCE_MANAGEMENT && !RTXGI_DDGI_SHADER_REFLECTION
#if SPIRV
    #define VOLUME_CONSTS_REGISTER 0
    #define VOLUME_CONSTS_SPACE 0
    #define RAY_DATA_REGISTER 1
    #define RAY_DATA_SPACE 0
    #define PROBE_DATA_REGISTER 4
    #define PROBE_DATA_SPACE 0
#else
    #define CONSTS_REGISTER b0
    #define CONSTS_SPACE space1
    #define VOLUME_CONSTS_REGISTER t0
    #define VOLUME_CONSTS_SPACE space1
    #define RAY_DATA_REGISTER u0
    #define RAY_DATA_SPACE space1
    #define PROBE_DATA_REGISTER u3
    #define PROBE_DATA_SPACE space1
#endif // SPIRV
#endif // RTXGI_DDGI_RESOURCE_MANAGEMENT && !RTXGI_DDGI_SHADER_REFLECTION

// -------- SHADER REFLECTION DEFINES -------------------------------------------------------------

// RTXGI_DDGI_SHADER_REFLECTION must be passed in as a define at shader compilation time.
// This define specifies if the shader resources will be determined using shader reflection.
// Ex: RTXGI_DDGI_SHADER_REFLECTION [0|1]

#ifndef RTXGI_DDGI_SHADER_REFLECTION
#error Required define RTXGI_DDGI_SHADER_REFLECTION is not defined for ProbeRelocationCS.hlsl!
#else

#if !RTXGI_DDGI_SHADER_REFLECTION

// REGISTERs AND SPACEs (SHADER REFLECTION DISABLED)

#if !SPIRV

// CONSTS_REGISTER and CONSTS_SPACE must be passed in as defines at shader compilation time *when not using reflection*.
// These defines specify the shader register and space used for root / push DDGI constants.
// Ex: CONSTS_REGISTER b0
// Ex: CONSTS_SPACE space1

#ifndef CONSTS_REGISTER
#error Required define CONSTS_REGISTER is not defined for ProbeRelocationCS.hlsl!
#endif

#ifndef CONSTS_SPACE
#error Required define CONSTS_SPACE is not defined for ProbeRelocationCS.hlsl!
#endif

#endif

// VOLUME_CONSTS_REGISTER and VOLUME_CONSTS_SPACE must be passed in as defines at shader compilation time *when not using reflection*.
// These defines specify the shader register and space used for the DDGIVolume constants structured buffer.
// Ex: VOLUME_CONSTS_REGISTER t5
// Ex: VOLUME_CONSTS_SPACE space0

#ifndef VOLUME_CONSTS_REGISTER
#error Required define VOLUME_CONSTS_REGISTER is not defined for ProbeRelocationCS.hlsl!
#endif

#ifndef VOLUME_CONSTS_SPACE
#error Required define VOLUME_CONSTS_SPACE is not defined for ProbeRelocationCS.hlsl!
#endif

#endif // !RTXGI_DDGI_SHADER_REFLECTION

#endif // #ifndef RTXGI_DDGI_SHADER_REFLECTION

// -------- RESOURCE BINDING DEFINES --------------------------------------------------------------

// RTXGI_DDGI_BINDLESS_RESOURCES must be passed in as a define at shader compilation time.
// This define specifies whether resources will be accessed bindlessly or not.
// Ex: RTXGI_DDGI_BINDLESS_RESOURCES [0|1]

#ifndef RTXGI_DDGI_BINDLESS_RESOURCES
#error Required define RTXGI_DDGI_BINDLESS_RESOURCES is not defined for ProbeRelocationCS.hlsl!
#else

#if !RTXGI_DDGI_SHADER_REFLECTION

#if RTXGI_DDGI_BINDLESS_RESOURCES

// BINDLESS RESOURCE DEFINES (SHADER REFLECTION DISABLED)

// RWTEX2D_REGISTER and RWTEX2D_SPACE must be passed in as defines at shader compilation time *when not using reflection*.
// These defines specify the shader register and space used for the DDGIVolume constants structured buffer.
// Ex: RWTEX2D_REGISTER t5
// Ex: RWTEX2D_SPACE space0

#ifndef RWTEX2D_REGISTER
#error Required bindless mode define RWTEX2D_REGISTER is not defined for ProbeRelocationCS.hlsl!
#endif

#ifndef RWTEX2D_SPACE
#error Required bindless mode define RWTEX2D_SPACE is not defined for ProbeRelocationCS.hlsl!
#endif

#else

// BOUND RESOURCE DEFINES (SHADER REFLECTION DISABLED)

// RAY_DATA_REGISTER and RAY_DATA_SPACE must be passed in as defines at shader compilation time *when not using reflection*.
// These defines specify the shader register and space used for the DDGIVolume ray data texture.
// Ex: RAY_DATA_REGISTER u0
// Ex: RAY_DATA_SPACE space1

#ifndef RAY_DATA_REGISTER
#error Required define RAY_DATA_REGISTER is not defined for ProbeRelocationCS.hlsl!
#endif

#ifndef RAY_DATA_SPACE
#error Required define RAY_DATA_SPACE is not defined for ProbeRelocationCS.hlsl!
#endif

// PROBE_DATA_REGISTER and PROBE_DATA_SPACE must be passed in as defines at shader compilation time *when not using reflection*
// and when probe classification is enabled.
// These defines specify the shader register and space used for the DDGIVolume probe data texture.
// Ex: PROBE_DATA_REGISTER u2
// Ex: PROBE_DATA_SPACE space1

#ifndef PROBE_DATA_REGISTER
#error Required define PROBE_DATA_REGISTER is not defined for ProbeRelocationCS.hlsl!
#endif

#ifndef PROBE_DATA_SPACE
#error Required define PROBE_DATA_SPACE is not defined for ProbeRelocationCS.hlsl!
#endif

#endif // RTXGI_DDGI_BINDLESS_RESOURCES

#endif // !RTXGI_DDGI_SHADER_REFLECTION

#endif // #ifndef RTXGI_DDGI_BINDLESS_RESOURCES

// ------------------------------------------------------------------------------------------------

#include "include/ProbeCommon.hlsl"

#if RTXGI_DDGI_SHADER_REFLECTION || SPIRV
#define CONSTS_REG_DECL 
#define VOLUME_CONSTS_REG_DECL 
#if RTXGI_DDGI_BINDLESS_RESOURCES
    #define RWTEX2D_REG_DECL 
#else
    #define RAY_DATA_REG_DECL 
    #define PROBE_DATA_REG_DECL 
#endif 

#else

#define CONSTS_REG_DECL : register(CONSTS_REGISTER, CONSTS_SPACE)
#define VOLUME_CONSTS_REG_DECL : register(VOLUME_CONSTS_REGISTER, VOLUME_CONSTS_SPACE)
#if RTXGI_DDGI_BINDLESS_RESOURCES
    #define RWTEX2D_REG_DECL : register(RWTEX2D_REGISTER, RWTEX2D_SPACE)
#else
    #define RAY_DATA_REG_DECL : register(RAY_DATA_REGISTER, RAY_DATA_SPACE)
    #define PROBE_DATA_REG_DECL : register(PROBE_DATA_REGISTER, PROBE_DATA_SPACE)
#endif

#endif // RTXGI_DDGI_SHADER_REFLECTION || SPIRV

// Root / Push Constants
#if !SPIRV

// D3D12 can have multiple root constants across different root parameter slots, so this constant
// buffer can be declared here for both bindless and bound resource access methods
ConstantBuffer<DDGIConstants> DDGI CONSTS_REG_DECL;
uint GetVolumeIndex() { return DDGI.volumeIndex; }
#if RTXGI_DDGI_BINDLESS_RESOURCES
uint GetUAVOffset() { return DDGI.uavOffset; }
#endif

#else

#if RTXGI_DDGI_BINDLESS_RESOURCES
// Vulkan only allows a single block of memory for push constants, so when using bindless access
// the shader must understand the layout of the push constant data - which comes from the host application
struct PushConsts
{
    // Insert padding to match the layout of your push constants.
    // This matches the Test Harness' "GlobalConstants" struct with 
    // 36 float values before the DDGI constants.
    float4x4 padding0;
    float4x4 padding1;
    float4   padding3;
    uint     ddgi_volumeIndex;
    uint     ddgi_uavOffset;
};
RTXGI_VK_PUSH_CONST PushConsts Global;
uint GetVolumeIndex() { return Global.ddgi_volumeIndex; }
uint GetUAVOffset() { return Global.ddgi_uavOffset; }
#else
RTXGI_VK_PUSH_CONST ConstantBuffer<DDGIConstants> Global;
uint GetVolumeIndex() { return Global.volumeIndex; }
#endif // RTXGI_DDGI_BINDLESS_RESOURCES

#endif

// DDGIVolume constants structured buffer
RTXGI_VK_BINDING(VOLUME_CONSTS_REGISTER, VOLUME_CONSTS_SPACE)
StructuredBuffer<DDGIVolumeDescGPUPacked> DDGIVolumes VOLUME_CONSTS_REG_DECL;

#if RTXGI_DDGI_BINDLESS_RESOURCES

// DDGIVolume ray data, probe irradiance / distance, probe data
RTXGI_VK_BINDING(RWTEX2D_REGISTER, RWTEX2D_SPACE)
RWTexture2D<float4> RWTex2D[] RWTEX2D_REG_DECL;

#else

// DDGIVolume ray data (radiance and hit distances)
RTXGI_VK_BINDING(RAY_DATA_REGISTER, RAY_DATA_SPACE)
RWTexture2D<float4> RayData RAY_DATA_REG_DECL;

// Probe data (world-space offsets and classification states)
RTXGI_VK_BINDING(PROBE_DATA_REGISTER, PROBE_DATA_SPACE)
RWTexture2D<float4> ProbeData PROBE_DATA_REG_DECL;

#endif

[numthreads(32, 1, 1)]
void DDGIProbeRelocationCS(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    // Get the volume's index
    uint volumeIndex = GetVolumeIndex();

    // Compute the probe index for this thread
    int probeIndex = DispatchThreadID.x;

    // Get the volume's constants
    DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumes[volumeIndex]);

    // Early out if the thread maps past the number of probes in the volume
    int numProbes = volume.probeCounts.x * volume.probeCounts.y * volume.probeCounts.z;
    if (probeIndex >= numProbes) return;

#if RTXGI_DDGI_BINDLESS_RESOURCES
    // Get the offset of each resource in the UAV array
    uint uavOffset = GetUAVOffset();

    // Get the volume's texture UAVs
    RWTexture2D<float4> RayData = RWTex2D[uavOffset + (volumeIndex * 4)];
    RWTexture2D<float4> ProbeData = RWTex2D[uavOffset + (volumeIndex * 4) + 3];
#endif

    // Get the probe's texel coordinates in the Probe Data texture
    uint2 coords = DDGIGetProbeTexelCoords(probeIndex, volume);

    // Read the current world position offset
    float3 offset = DDGILoadProbeDataOffset(ProbeData, coords, volume);

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
        // Construct the texture coordinates for the ray
        int2 rayTexCoords = int2(rayIndex, probeIndex);

        // Load the hit distance for the ray
        float hitDistance = DDGILoadProbeRayDistance(RayData, rayTexCoords, volume);

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
    DDGIStoreProbeDataOffset(ProbeData, coords, offset, volume);
}

[numthreads(32, 1, 1)]
void DDGIProbeRelocationResetCS(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    // Get the volume's index
    uint volumeIndex = GetVolumeIndex();

    // Get the volume's constants
    DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumes[volumeIndex]);

#if RTXGI_DDGI_BINDLESS_RESOURCES
    // Get the offset of each resource in the UAV array
    uint uavOffset = GetUAVOffset();

    // Get the volume's texture UAVs
    RWTexture2D<float4> ProbeData = RWTex2D[uavOffset + (volumeIndex * 4) + 3];
#endif

    // Get the probe's texel coordinates in the Probe Data texture
    uint2 probeDataCoords = DDGIGetProbeTexelCoords(DispatchThreadID.x, volume);

    // Write the probe offset
    ProbeData[probeDataCoords].xyz = float3(0.f, 0.f, 0.f);
}
