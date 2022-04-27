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
#error Required define RTXGI_DDGI_RESOURCE_MANAGEMENT is not defined for ProbeClassificationCS.hlsl!
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
#error Required define RTXGI_DDGI_SHADER_REFLECTION is not defined for ProbeClassificationCS.hlsl!
#else

#if !RTXGI_DDGI_SHADER_REFLECTION

// REGISTERs AND SPACEs (SHADER REFLECTION DISABLED)

#if !SPIRV

// CONSTS_REGISTER and CONSTS_SPACE must be passed in as defines at shader compilation time *when not using reflection*.
// These defines specify the shader register and space used for root / push DDGI constants.
// Ex: CONSTS_REGISTER b0
// Ex: CONSTS_SPACE space1

#ifndef CONSTS_REGISTER
#error Required define CONSTS_REGISTER is not defined for ProbeClassificationCS.hlsl!
#endif

#ifndef CONSTS_SPACE
#error Required define CONSTS_SPACE is not defined for ProbeClassificationCS.hlsl!
#endif

#endif

// VOLUME_CONSTS_REGISTER and VOLUME_CONSTS_SPACE must be passed in as defines at shader compilation time *when not using reflection*.
// These defines specify the shader register and space used for the DDGIVolume constants structured buffer.
// Ex: VOLUME_CONSTS_REGISTER t5
// Ex: VOLUME_CONSTS_SPACE space0

#ifndef VOLUME_CONSTS_REGISTER
#error Required define VOLUME_CONSTS_REGISTER is not defined for ProbeClassificationCS.hlsl!
#endif

#ifndef VOLUME_CONSTS_SPACE
#error Required define VOLUME_CONSTS_SPACE is not defined for ProbeClassificationCS.hlsl!
#endif

#endif // !RTXGI_DDGI_SHADER_REFLECTION

#endif // #ifndef RTXGI_DDGI_SHADER_REFLECTION

// -------- RESOURCE BINDING DEFINES --------------------------------------------------------------

// RTXGI_DDGI_BINDLESS_RESOURCES must be passed in as a define at shader compilation time.
// This define specifies whether resources will be accessed bindlessly or not.
// Ex: RTXGI_DDGI_BINDLESS_RESOURCES [0|1]

#ifndef RTXGI_DDGI_BINDLESS_RESOURCES
#error Required define RTXGI_DDGI_BINDLESS_RESOURCES is not defined for ProbeClassificationCS.hlsl!
#else

#if !RTXGI_DDGI_SHADER_REFLECTION

#if RTXGI_DDGI_BINDLESS_RESOURCES

// BINDLESS RESOURCE DEFINES (SHADER REFLECTION DISABLED)

// RWTEX2D_REGISTER and RWTEX2D_SPACE must be passed in as defines at shader compilation time *when not using reflection*.
// These defines specify the shader register and space used for the DDGIVolume constants structured buffer.
// Ex: RWTEX2D_REGISTER t5
// Ex: RWTEX2D_SPACE space0

#ifndef RWTEX2D_REGISTER
#error Required bindless mode define RWTEX2D_REGISTER is not defined for ProbeClassificationCS.hlsl!
#endif

#ifndef RWTEX2D_SPACE
#error Required bindless mode define RWTEX2D_SPACE is not defined for ProbeClassificationCS.hlsl!
#endif

#else

// BOUND RESOURCE DEFINES (SHADER REFLECTION DISABLED)

// RAY_DATA_REGISTER and RAY_DATA_SPACE must be passed in as defines at shader compilation time *when not using reflection*.
// These defines specify the shader register and space used for the DDGIVolume ray data texture.
// Ex: RAY_DATA_REGISTER u0
// Ex: RAY_DATA_SPACE space1

#ifndef RAY_DATA_REGISTER
#error Required define RAY_DATA_REGISTER is not defined for ProbeClassificationCS.hlsl!
#endif

#ifndef RAY_DATA_SPACE
#error Required define RAY_DATA_SPACE is not defined for ProbeClassificationCS.hlsl!
#endif

// PROBE_DATA_REGISTER and PROBE_DATA_SPACE must be passed in as defines at shader compilation time *when not using reflection*
// and when probe classification is enabled.
// These defines specify the shader register and space used for the DDGIVolume probe data texture.
// Ex: PROBE_DATA_REGISTER u2
// Ex: PROBE_DATA_SPACE space1

#ifndef PROBE_DATA_REGISTER
#error Required define PROBE_DATA_REGISTER is not defined for ProbeClassificationCS.hlsl!
#endif

#ifndef PROBE_DATA_SPACE
#error Required define PROBE_DATA_SPACE is not defined for ProbeClassificationCS.hlsl!
#endif

#endif // RTXGI_DDGI_BINDLESS_RESOURCES

#endif // !RTXGI_DDGI_SHADER_REFLECTION

#endif // #ifndef RTXGI_DDGI_BINDLESS_RESOURCES

// -------------------------------------------------------------------------------------------

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
void DDGIProbeClassificationCS(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    // Get the volume's index
    uint volumeIndex = GetVolumeIndex();

    // Compute the probe index for this thread
    uint probeIndex = DispatchThreadID.x;

    // Get the volume's constants
    DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumes[volumeIndex]);

    // Early out if this thread maps past the number of probes in the volume
    int numProbes = (volume.probeCounts.x * volume.probeCounts.y * volume.probeCounts.z);
    if (probeIndex >= numProbes) return;

#if RTXGI_DDGI_BINDLESS_RESOURCES
    // Get the offset of each resource in the UAV array
    uint uavOffset = GetUAVOffset();

    // Get the volume's texture UAVs
    RWTexture2D<float4> RayData = RWTex2D[uavOffset + (volumeIndex * 4)];
    RWTexture2D<float4> ProbeData = RWTex2D[uavOffset + (volumeIndex * 4) + 3];
#endif

    // Get the number of ray samples to inspect
    int numRays = min(volume.probeNumRays, RTXGI_DDGI_NUM_FIXED_RAYS);

    int rayIndex;
    int   backfaceCount = 0;
    float hitDistances[RTXGI_DDGI_NUM_FIXED_RAYS];

    // Load the hit distances and count the number of backface hits
    for (rayIndex = 0; rayIndex < RTXGI_DDGI_NUM_FIXED_RAYS; rayIndex++)
    {
        // Construct the texture coordinates for the ray
        int2 rayTexCoords = int2(rayIndex, probeIndex);

        // Load the hit distance for the ray
        hitDistances[rayIndex] = DDGILoadProbeRayDistance(RayData, rayTexCoords, volume);

        // Increment the count if a backface is hit
        backfaceCount += (hitDistances[rayIndex] < 0.f);
    }

    // Get the probe's texel coordinates in the Probe Data texture
    uint2 probeDataCoords = DDGIGetProbeTexelCoords(probeIndex, volume);

    // Early out: number of backface hits has been exceeded. The probe is probably inside geometry.
    if(((float)backfaceCount / (float)RTXGI_DDGI_NUM_FIXED_RAYS) > volume.probeFixedRayBackfaceThreshold)
    {
        ProbeData[probeDataCoords].w = RTXGI_DDGI_PROBE_STATE_INACTIVE;
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
            ProbeData[probeDataCoords].w = RTXGI_DDGI_PROBE_STATE_ACTIVE;
            return;
        }
    }

    ProbeData[probeDataCoords].w = RTXGI_DDGI_PROBE_STATE_INACTIVE;
}


[numthreads(32, 1, 1)]
void DDGIProbeClassificationResetCS(uint3 DispatchThreadID : SV_DispatchThreadID)
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

    ProbeData[probeDataCoords].w = RTXGI_DDGI_PROBE_STATE_ACTIVE;
}
