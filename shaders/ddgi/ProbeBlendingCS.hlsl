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
#error Required define RTXGI_DDGI_RESOURCE_MANAGEMENT is not defined for ProbeBlendingCS.hlsl!
#endif

#if RTXGI_DDGI_RESOURCE_MANAGEMENT && !RTXGI_DDGI_SHADER_REFLECTION
#if SPIRV
    #define VOLUME_CONSTS_REGISTER 0
    #define VOLUME_CONSTS_SPACE 0
    #define RAY_DATA_REGISTER 1
    #define RAY_DATA_SPACE 0
    #if RTXGI_DDGI_BLEND_RADIANCE
    #define OUTPUT_REGISTER 2
    #else
    #define OUTPUT_REGISTER 3
    #endif
    #define OUTPUT_SPACE 0
    #define PROBE_DATA_REGISTER 4
    #define PROBE_DATA_SPACE 0
#else
    #define CONSTS_REGISTER b0
    #define CONSTS_SPACE space1
    #define VOLUME_CONSTS_REGISTER t0
    #define VOLUME_CONSTS_SPACE space1
    #define RAY_DATA_REGISTER u0
    #define RAY_DATA_SPACE space1
    #if RTXGI_DDGI_BLEND_RADIANCE
    #define OUTPUT_REGISTER u1
    #else
    #define OUTPUT_REGISTER u2
    #endif
    #define OUTPUT_SPACE space1
    #define PROBE_DATA_REGISTER u3
    #define PROBE_DATA_SPACE space1
#endif // SPIRV
#endif // RTXGI_DDGI_RESOURCE_MANAGEMENT && !RTXGI_DDGI_SHADER_REFLECTION

// -------- SHADER REFLECTION DEFINES -------------------------------------------------------------

// RTXGI_DDGI_SHADER_REFLECTION must be passed in as a define at shader compilation time.
// This define specifies if the shader resources will be determined using shader reflection.
// Ex: RTXGI_DDGI_SHADER_REFLECTION [0|1]

#ifndef RTXGI_DDGI_SHADER_REFLECTION
#error Required define RTXGI_DDGI_SHADER_REFLECTION is not defined for ProbeBlendingCS.hlsl!
#else

#if !RTXGI_DDGI_SHADER_REFLECTION

// REGISTERs AND SPACEs (SHADER REFLECTION DISABLED)

#if !SPIRV
// CONSTS_REGISTER and CONSTS_SPACE must be passed in as defines at shader compilation time *when not using reflection*.
// These defines specify the shader register and space used for root / push DDGI constants.
// Ex: CONSTS_REGISTER b0
// Ex: CONSTS_SPACE space1

#ifndef CONSTS_REGISTER
#error Required define CONSTS_REGISTER is not defined for ProbeBlendingCS.hlsl!
#endif

#ifndef CONSTS_SPACE
#error Required define CONSTS_SPACE is not defined for ProbeBlendingCS.hlsl!
#endif

#endif // !SPIRV

// VOLUME_CONSTS_REGISTER and VOLUME_CONSTS_SPACE must be passed in as defines at shader compilation time *when not using reflection*.
// These defines specify the shader register and space used for the DDGIVolume constants structured buffer.
// Ex: VOLUME_CONSTS_REGISTER t5
// Ex: VOLUME_CONSTS_SPACE space0

#ifndef VOLUME_CONSTS_REGISTER
#error Required define VOLUME_CONSTS_REGISTER is not defined for ProbeBlendingCS.hlsl!
#endif

#ifndef VOLUME_CONSTS_SPACE
#error Required define VOLUME_CONSTS_SPACE is not defined for ProbeBlendingCS.hlsl!
#endif

#endif // !RTXGI_DDGI_SHADER_REFLECTION

#endif // #ifndef RTXGI_DDGI_SHADER_REFLECTION

// -------- RESOURCE BINDING DEFINES --------------------------------------------------------------

// RTXGI_DDGI_BINDLESS_RESOURCES must be passed in as a define at shader compilation time.
// This define specifies whether resources will be accessed bindlessly or not.
// Ex: RTXGI_DDGI_BINDLESS_RESOURCES [0|1]

#ifndef RTXGI_DDGI_BINDLESS_RESOURCES
#error Required define RTXGI_DDGI_BINDLESS_RESOURCES is not defined for ProbeBlendingCS.hlsl!
#else

#if !RTXGI_DDGI_SHADER_REFLECTION

#if RTXGI_DDGI_BINDLESS_RESOURCES

// BINDLESS RESOURCE DEFINES (SHADER REFLECTION DISABLED)

// RWTEX2D_REGISTER and RWTEX2D_SPACE must be passed in as defines at shader compilation time *when not using reflection*.
// These defines specify the shader register and space used for the DDGIVolume constants structured buffer.
// Ex: RWTEX2D_REGISTER t5
// Ex: RWTEX2D_SPACE space0

#ifndef RWTEX2D_REGISTER
#error Required bindless mode define RWTEX2D_REGISTER is not defined for ProbeBlendingCS.hlsl!
#endif

#ifndef RWTEX2D_SPACE
#error Required bindless mode define RWTEX2D_SPACE is not defined for ProbeBlendingCS.hlsl!
#endif

#else

// BOUND RESOURCE DEFINES (SHADER REFLECTION DISABLED)

// RAY_DATA_REGISTER and RAY_DATA_SPACE must be passed in as defines at shader compilation time *when not using reflection*.
// These defines specify the shader register and space used for the DDGIVolume ray data texture.
// Ex: RAY_DATA_REGISTER u0
// Ex: RAY_DATA_SPACE space1

#ifndef RAY_DATA_REGISTER
#error Required define RAY_DATA_REGISTER is not defined for ProbeBlendingCS.hlsl!
#endif

#ifndef RAY_DATA_SPACE
#error Required define RAY_DATA_SPACE is not defined for ProbeBlendingCS.hlsl!
#endif

// OUTPUT_REGISTER and OUTPUT_SPACE must be passed in as defines at shader compilation time *when not using reflection*.
// These defines specify the shader register and space used for the DDGIVolume irradiance or distance texture.
// Ex: OUTPUT_REGISTER u1
// Ex: OUTPUT_SPACE space1

#ifndef OUTPUT_REGISTER
#error Required define OUTPUT_REGISTER is not defined for ProbeBlendingCS.hlsl!
#endif

#ifndef OUTPUT_SPACE
#error Required define OUTPUT_SPACE is not defined for ProbeBlendingCS.hlsl!
#endif

// PROBE_DATA_REGISTER and PROBE_DATA_SPACE must be passed in as defines at shader compilation time *when not using reflection*
// and when probe classification is enabled.
// These defines specify the shader register and space used for the DDGIVolume probe data texture.
// Ex: PROBE_DATA_REGISTER u2
// Ex: PROBE_DATA_SPACE space1

#ifndef PROBE_DATA_REGISTER
#error Required define PROBE_DATA_REGISTER is not defined for ProbeBlendingCS.hlsl!
#endif

#ifndef PROBE_DATA_SPACE
#error Required define PROBE_DATA_SPACE is not defined for ProbeBlendingCS.hlsl!
#endif

#endif // RTXGI_DDGI_BINDLESS_RESOURCES

#endif // !RTXGI_DDGI_SHADER_REFLECTION

#endif // #ifndef RTXGI_DDGI_BINDLESS_RESOURCES

// -------- CONFIGURATION DEFINES -----------------------------------------------------------------

// RTXGI_DDGI_BLEND_SHARED_MEMORY must be passed in as a define at shader compilation time.
// This define specifies if probe blending will use shared memory to improve performance.
// Shared memory substantially increases performance, so using it is strongly recommended.
// Ex: RTXGI_DDGI_BLEND_SHARED_MEMORY [0|1]
#ifndef RTXGI_DDGI_BLEND_SHARED_MEMORY
#error Required define RTXGI_DDGI_BLEND_SHARED_MEMORY is not defined for ProbeBlendingCS.hlsl!
#else

#if RTXGI_DDGI_BLEND_SHARED_MEMORY

// RTXGI_DDGI_BLEND_RAYS_PER_PROBE must be passed in as a define at shader compilation time *when using shared memory*.
// This define specifies the number of rays that are traced per probe and determines how data
// is cooperatively loaded, computed, and stored in shared memory.
// Ex: RTXGI_DDGI_BLEND_RAYS_PER_PROBE 144 => 144 rays are traced per probe
#ifndef RTXGI_DDGI_BLEND_RAYS_PER_PROBE
#error Required define RTXGI_DDGI_BLEND_RAYS_PER_PROBE is not defined for ProbeBlendingCS.hlsl!
#endif

#endif // RTXGI_DDGI_BLEND_SHARED_MEMORY

#endif

// RTXGI_DDGI_BLEND_SCROLL_SHARED_MEMORY must be passed in as a define at shader compilation time.
// This define specifies if probe blending will use shared memory to improve performance.
// Ex: RTXGI_DDGI_BLEND_SCROLL_SHARED_MEMORY [0|1]
#ifndef RTXGI_DDGI_BLEND_SCROLL_SHARED_MEMORY
#error Required define RTXGI_DDGI_BLEND_SCROLL_SHARED_MEMORY is not defined for ProbeBlendingCS.hlsl!
#endif // RTXGI_DDGI_BLEND_SCROLL_SHARED_MEMORY

// RTXGI_DDGI_BLEND_RADIANCE must be passed in as a define at shader compilation time.
// This define specifies whether the shader blends radiance or distance values.
// Ex: RTXGI_DDGI_BLEND_RADIANCE [0|1]
#ifndef RTXGI_DDGI_BLEND_RADIANCE
#error Required define RTXGI_DDGI_BLEND_RADIANCE is not defined for ProbeBlendingCS.hlsl!
#endif

// RTXGI_DDGI_PROBE_NUM_TEXELS must be passed in as a define at shader compilation time.
// This define specifies the number of texels of a single dimension of a probe.
// Ex: RTXGI_DDGI_PROBE_NUM_TEXELS 6  => irradiance data is 6x6 texels for a single probe
// Ex: RTXGI_DDGI_PROBE_NUM_TEXELS 14 => distance data is 14x14 texels for a single probe
#ifndef RTXGI_DDGI_PROBE_NUM_TEXELS
#error Required define RTXGI_DDGI_PROBE_NUM_TEXELS is not defined for ProbeBlendingCS.hlsl!
#endif

// -------- OPTIONAL DEFINES -----------------------------------------------------------------

// Define RTXGI_DDGI_DEBUG_PROBE_INDEXING before compiling SDK HLSL shaders to toggle
// a visualization mode that outputs probe indices as probe color. Useful when debugging.
// 0: Disabled (default).
// 1: Enabled.
#ifndef RTXGI_DDGI_DEBUG_PROBE_INDEXING
#pragma message "Optional define RTXGI_DDGI_DEBUG_PROBE_INDEXING is not defined, defaulting to 0." 
#define RTXGI_DDGI_DEBUG_PROBE_INDEXING 0
#endif

// Define RTXGI_DDGI_DEBUG_OCTAHEDRAL_INDEXING before compiling SDK HLSL shaders to toggle
// a visualization mode that outputs directions (as colors) in probe irradiance texels. Useful when debugging.
// 0: Disabled (default).
// 1: Enabled.
#ifndef RTXGI_DDGI_DEBUG_OCTAHEDRAL_INDEXING
#pragma message "Optional define RTXGI_DDGI_DEBUG_OCTAHEDRAL_INDEXING is not defined, defaulting to 0." 
#define RTXGI_DDGI_DEBUG_OCTAHEDRAL_INDEXING 0
#endif

// -------------------------------------------------------------------------------------------

#include "include/ProbeCommon.hlsl"

#if RTXGI_DDGI_SHADER_REFLECTION || SPIRV
#define CONSTS_REG_DECL 
#define VOLUME_CONSTS_REG_DECL 
#if RTXGI_DDGI_BINDLESS_RESOURCES
    #define RWTEX2D_REG_DECL 
#else
    #define RAY_DATA_REG_DECL 
    #define OUTPUT_REG_DECL 
    #define PROBE_DATA_REG_DECL 
#endif

#else

#define CONSTS_REG_DECL : register(CONSTS_REGISTER, CONSTS_SPACE)
#define VOLUME_CONSTS_REG_DECL : register(VOLUME_CONSTS_REGISTER, VOLUME_CONSTS_SPACE)
#if RTXGI_DDGI_BINDLESS_RESOURCES
    #define RWTEX2D_REG_DECL : register(RWTEX2D_REGISTER, RWTEX2D_SPACE)
#else
    #define RAY_DATA_REG_DECL : register(RAY_DATA_REGISTER, RAY_DATA_SPACE)
    #define OUTPUT_REG_DECL : register(OUTPUT_REGISTER, OUTPUT_SPACE)
    #define PROBE_DATA_REG_DECL : register(PROBE_DATA_REGISTER, PROBE_DATA_SPACE)
#endif // RTXGI_DDGI_BINDLESS_RESOURCES

#endif // RTXGI_DDGI_SHADER_REFLECTION || SPIRV

// Root / Push Constants
#if !SPIRV

// D3D12 can have multiple root constants across different root parameter slots, so this constant
// buffer can be used for both bindless and bound resource access methods
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

// DDGIVolume probe irradiance or filtered distance
RTXGI_VK_BINDING(OUTPUT_REGISTER, OUTPUT_SPACE)
RWTexture2D<float4> Output OUTPUT_REG_DECL;

// Probe data (world-space offsets and classification states)
RTXGI_VK_BINDING(PROBE_DATA_REGISTER, PROBE_DATA_SPACE)
RWTexture2D<float4> ProbeData PROBE_DATA_REG_DECL;

#endif // RTXGI_DDGI_BINDLESS_RESOURCES

#if RTXGI_DDGI_BLEND_SHARED_MEMORY
// Shared Memory (example with default settings):
// Radiance (float3) x 144 rays/probe = 432 floats (~1.7 KB)
// Distance (float) x 144 rays/probe = 144 floats (~0.56 KB)
// Ray Directions (float3 x 144 rays/probe) = 432 floats (~1.7 KB)
//
// Max shared memory usage = ~3.96 KB (~1.7 KB radiance + ~0.56 KB distance + ~1.7 KB directions)

// Example usage:
// Irradiance thread groups as 6 x 6 = 36 threads
//     Group threads load 144 ray radiance & distance values / 36 threads = 4 ray radiance & distance values / thread
//     Group threads compute 144 ray directions / 36 threads = 4 directions / thread
// Distance thread groups are 14 x 14 = 196 threads
//     Group threads load 144 ray distances / 196 threads = ~0.73 ray distance values / thread
//     Group threads compute 144 ray directions / 196 threads = ~0.73 ray directions / thread

#if RTXGI_DDGI_BLEND_RADIANCE
groupshared float3 RayRadiance[RTXGI_DDGI_BLEND_RAYS_PER_PROBE];
#endif
groupshared float  RayDistance[RTXGI_DDGI_BLEND_RAYS_PER_PROBE];
groupshared float3 RayDirection[RTXGI_DDGI_BLEND_RAYS_PER_PROBE];
#endif // RTXGI_DDGI_BLEND_SHARED_MEMORY

#if RTXGI_DDGI_BLEND_SCROLL_SHARED_MEMORY
groupshared bool scrollClear;
#endif // RTXGI_DDGI_BLEND_SCROLL_SHARED_MEMORY

[numthreads(RTXGI_DDGI_PROBE_NUM_TEXELS, RTXGI_DDGI_PROBE_NUM_TEXELS, 1)]
void DDGIProbeBlendingCS(uint3 DispatchThreadID : SV_DispatchThreadID, uint3 GroupThreadIndex : SV_GroupThreadID, uint GroupIndex : SV_GroupIndex)
{
    float4 result = float4(0.f, 0.f, 0.f, 0.f);

    // Get the volume's index
    uint volumeIndex = GetVolumeIndex();

    // Get the volume's constants
    DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumes[volumeIndex]);

    // Get the number of probes
    uint numProbes = (volume.probeCounts.x * volume.probeCounts.y * volume.probeCounts.z);

    // Find the probe index for this thread
    int probeIndex = DDGIGetProbeIndex(DispatchThreadID.xy, RTXGI_DDGI_PROBE_NUM_TEXELS, volume);

    // Early out: the probe doesn't exist
    if (probeIndex >= numProbes || probeIndex < 0) return;

    // Get the texture coordinates in the irradiance (or distance) texture atlas
    uint2 atlasTexCoords = uint2(1, 1) + DispatchThreadID.xy + (DispatchThreadID.xy / RTXGI_DDGI_PROBE_NUM_TEXELS) * 2;

#if RTXGI_DDGI_BINDLESS_RESOURCES
    // Get the offset of each resource in the UAV array
    uint uavOffset = GetUAVOffset();

    // Get the volume's texture UAVs
    RWTexture2D<float4> RayData = RWTex2D[uavOffset + (volumeIndex * 4)];
    #if RTXGI_DDGI_BLEND_RADIANCE
        RWTexture2D<float4> Output = RWTex2D[uavOffset + (volumeIndex * 4) + 1];
    #else
        RWTexture2D<float4> Output = RWTex2D[uavOffset + (volumeIndex * 4) + 2];
    #endif
    RWTexture2D<float4> ProbeData = RWTex2D[uavOffset + (volumeIndex * 4) + 3];
#endif

    // Determine if a scrolled probe should be cleared
    // Using shared memory may or may not be worth it depending on the target HW
#if RTXGI_DDGI_BLEND_SCROLL_SHARED_MEMORY
    // Initialize groupshared variable to not clear the probe texels (no scroll has occured)
    scrollClear = false;

    // Check if probe scrolling should clear this probe's texels
    uint groupThreadIdx = (GroupThreadIndex.x + GroupThreadIndex.y);
    if(groupThreadIdx == 0 && IsVolumeMovementScrolling(volume))
    {
        // Get the probe's grid coordinates
        int3 probeCoords = DDGIGetProbeCoords(probeIndex, volume);

        // Clear probes that have been scrolled
        scrollClear |= DDGIClearScrolledPlane(Output, atlasTexCoords, probeCoords, 0, volume);
        scrollClear |= DDGIClearScrolledPlane(Output, atlasTexCoords, probeCoords, 1, volume);
        scrollClear |= DDGIClearScrolledPlane(Output, atlasTexCoords, probeCoords, 2, volume);
    }

    // Wait for all threads in the group to finish their shared memory operations
    GroupMemoryBarrierWithGroupSync();

    // Early out: this probe has been scrolled and cleared
    if(scrollClear) return;
#else
    if (IsVolumeMovementScrolling(volume))
    {
        // Get the probe's grid coordinates
        int3 probeCoords = DDGIGetProbeCoords(probeIndex, volume);

        // Clear probes that have been scrolled
        bool scrollClear = false;
        scrollClear |= DDGIClearScrolledPlane(Output, atlasTexCoords, probeCoords, 0, volume);
        scrollClear |= DDGIClearScrolledPlane(Output, atlasTexCoords, probeCoords, 1, volume);
        scrollClear |= DDGIClearScrolledPlane(Output, atlasTexCoords, probeCoords, 2, volume);
        if(scrollClear) return;
    }
#endif // RTXGI_DDGI_BLEND_SCROLL_SHARED_MEMORY

    // Early Out: don't blend rays for probes that are inactive
    int probeState = DDGILoadProbeState(probeIndex, ProbeData, volume);
    if (probeState == RTXGI_DDGI_PROBE_STATE_INACTIVE) return;

    // Visualize the probe index
#if RTXGI_DDGI_BLEND_RADIANCE && RTXGI_DDGI_DEBUG_PROBE_INDEXING
    if(volume.probeIrradianceFormat == RTXGI_DDGI_FORMAT_PROBE_IRRADIANCE_R32G32B32A32_FLOAT)
    {
        Output[atlasTexCoords] = float4(probeIndex, 0, 0, 1);
    }
    return;
#endif

    float2 probeOctantUV = float2(0.f, 0.f);

    // Visualize the probe's octahedral indexing
#if RTXGI_DDGI_BLEND_RADIANCE && RTXGI_DDGI_DEBUG_OCTAHEDRAL_INDEXING
    if(volume.probeIrradianceFormat == RTXGI_DDGI_FORMAT_PROBE_IRRADIANCE_R32G32B32A32_FLOAT)
    {
        probeOctantUV = DDGIGetNormalizedOctahedralCoordinates(int2(DispatchThreadID.xy), RTXGI_DDGI_PROBE_NUM_TEXELS);
        if (all(abs(probeOctantUV) <= 1.f))
        {
            float3 probeDirection = DDGIGetOctahedralDirection(probeOctantUV);
            probeDirection = (abs(probeDirection) >= 0.001f) * sign(probeDirection);    // Robustness for when the octant size is not a power of 2.
            result = float4((probeDirection * 0.5f) + 0.5f, 1.f);
        }
        Output[atlasTexCoords] = result;
        return;
    }
#endif

    // Get the probe ray direction associated with this thread
    probeOctantUV = DDGIGetNormalizedOctahedralCoordinates(int2(DispatchThreadID.xy), RTXGI_DDGI_PROBE_NUM_TEXELS);
    float3 probeRayDirection = DDGIGetOctahedralDirection(probeOctantUV);

#if RTXGI_DDGI_BLEND_SHARED_MEMORY
    // Cooperatively load the ray radiance and hit distance values into shared memory
    // Cooperatively compute probe ray directions
    int totalIterations = int(ceil(float(RTXGI_DDGI_BLEND_RAYS_PER_PROBE) / float(RTXGI_DDGI_PROBE_NUM_TEXELS * RTXGI_DDGI_PROBE_NUM_TEXELS)));
    for (int iteration = 0; iteration < totalIterations; iteration++)
    {
        int rayIndex = (GroupIndex * totalIterations) + iteration;
        if (rayIndex >= RTXGI_DDGI_BLEND_RAYS_PER_PROBE) break;

    #if RTXGI_DDGI_BLEND_RADIANCE
        // Load the ray radiance and store it in shared memory
        RayRadiance[rayIndex] = DDGILoadProbeRayRadiance(RayData, uint2(rayIndex, probeIndex), volume);
    #endif // RTXGI_DDGI_BLEND_RADIANCE

        // Load the ray hit distance and store it in shared memory
        RayDistance[rayIndex] = DDGILoadProbeRayDistance(RayData, uint2(rayIndex, probeIndex), volume);

        // Get a random normalized probe ray direction and store it in shared memory
        RayDirection[rayIndex] = DDGIGetProbeRayDirection(rayIndex, volume);
    }

    // Wait for all threads in the group to finish their shared memory operations
    GroupMemoryBarrierWithGroupSync();

#endif // RTXGI_DDGI_BLEND_SHARED_MEMORY

    int rayIndex = 0;

    // If relocation or classification are enabled, don't blend the fixed rays since they will bias the result
    if(volume.probeRelocationEnabled || volume.probeClassificationEnabled)
    {
        rayIndex = RTXGI_DDGI_NUM_FIXED_RAYS;
    }

#if RTXGI_DDGI_BLEND_RADIANCE
    // Backface hits are ignored when blending radiance
    // If more than the backface threshold of the rays hit backfaces, the probe is probably inside geometry
    // In this case, don't blend anything into the probe
    uint backfaces = 0;
    uint maxBackfaces = uint((volume.probeNumRays - rayIndex) * volume.probeRandomRayBackfaceThreshold);
#endif

    // Blend each ray's radiance or distance values to compute irradiance or fitered distance
    for ( /*rayIndex*/; rayIndex < volume.probeNumRays; rayIndex++)
    {
        // Get the direction for this probe ray
    #if RTXGI_DDGI_BLEND_SHARED_MEMORY
        float3 rayDirection = RayDirection[rayIndex];
    #else
        float3 rayDirection = DDGIGetProbeRayDirection(rayIndex, volume);
    #endif

        // Find the weight of the contribution for this ray
        // Weight is based on the cosine of the angle between the ray direction and the direction of the probe octant's texel
        float weight = max(0.f, dot(probeRayDirection, rayDirection));

        // The indices of the probe ray in the radiance buffer
        uint2 probeRayIndex = uint2(rayIndex, probeIndex);

    #if RTXGI_DDGI_BLEND_RADIANCE
        // Load the ray traced radiance and hit distance
        float3 probeRayRadiance = 0.f;
        float  probeRayDistance = 0.f;

        #if RTXGI_DDGI_BLEND_SHARED_MEMORY
            probeRayRadiance = RayRadiance[rayIndex];
            probeRayDistance = RayDistance[rayIndex];
        #else
            probeRayRadiance = DDGILoadProbeRayRadiance(RayData, probeRayIndex, volume);
            probeRayDistance = DDGILoadProbeRayDistance(RayData, probeRayIndex, volume);
        #endif // RTXGI_DDGI_BLEND_SHARED_MEMORY

        // Backface hit, don't blend this sample
        if (probeRayDistance < 0.f)
        {
            backfaces++;

            // Early out: only blend ray radiance into the probe if the backface threshold hasn't been exceeded
            if (backfaces >= maxBackfaces) return;

            continue;
        }

        // Blend the ray's radiance
        result += float4(probeRayRadiance * weight, weight);

    #else // RTXGI_DDGI_BLEND_RADIANCE == 0

        // Initialize the max probe hit distance to 50% larger the maximum distance between probe grid cells
        float probeMaxRayDistance = length(volume.probeSpacing) * 1.5f;

        // Increase or decrease the filtered distance value's "sharpness"
        weight = pow(weight, volume.probeDistanceExponent);

        // Load the ray traced distance
        // Hit distance is negative on backface hits (for probe relocation), so take the absolute value of the loaded data
        float probeRayDistance = 0.f;
    #if RTXGI_DDGI_BLEND_SHARED_MEMORY
        probeRayDistance = min(abs(RayDistance[rayIndex]), probeMaxRayDistance);
    #else
        probeRayDistance = min(abs(DDGILoadProbeRayDistance(RayData, probeRayIndex, volume)), probeMaxRayDistance);
    #endif // RTXGI_DDGI_BLEND_SHARED_MEMORY

        // Filter the ray hit distance
        result += float4(probeRayDistance * weight, (probeRayDistance * probeRayDistance) * weight, 0.f, weight);

    #endif // RTXGI_DDGI_BLEND_RADIANCE
    }

    float epsilon = float(volume.probeNumRays);
    if (volume.probeRelocationEnabled || volume.probeClassificationEnabled)
    {
        // If relocation or classification are enabled, fixed rays aren't blended since they will bias the result
        epsilon -= RTXGI_DDGI_NUM_FIXED_RAYS;
    }
    epsilon *= 1e-9f;

    // Normalize the blended irradiance (or filtered distance), if the combined weight is not close to zero.
    // To match the Monte Carlo Estimator of Irradiance, we should divide by N (the number of radiance samples).
    // Instead, we are dividing by sum(cos(theta)) (i.e. the sum of cosine weights) to reduce variance. To account
    // for this, we must multiply in a factor of 1/2. See the Math Guide in the documentation for more information.
    // For distance, note that we are *not* dividing by the sum of the cosine weights, but to avoid branching here
    // we are still dividing by 2. This means distance values sampled from texture need to be multiplied by 2 (see
    // Irradiance.hlsl line 138).
    result.rgb *= 1.f / (2.f * max(result.a, epsilon));

    // Get the previous irradiance in the probe
    float3 previous = Output[atlasTexCoords].rgb;

    // Get the history weight (hysteresis) to use for the probe texel's previous value
    // If the probe was previously cleared to completely black, set the hysteresis to zero
    float  hysteresis = volume.probeHysteresis;
    if (dot(previous, previous) == 0) hysteresis = 0.f;

#if RTXGI_DDGI_BLEND_RADIANCE
    // Tone-mapping gamma adjustment
    result.rgb = pow(result.rgb, (1.f / volume.probeIrradianceEncodingGamma));

    float3 delta = (result.rgb - previous.rgb);

    if (RTXGIMaxComponent(previous.rgb - result.rgb) > volume.probeIrradianceThreshold)
    {
        // Lower the hysteresis when a large lighting change is detected
        hysteresis = max(0.f, hysteresis - 0.75f);
    }

    if (length(delta) > volume.probeBrightnessThreshold)
    {
        // Clamp the maximum change in irradiance when a large brightness change is detected
        result.rgb = previous.rgb + (delta * 0.25f);
    }

    // Interpolate the new blended irradiance with the existing irradiance in the probe.
    // A high hysteresis value emphasizes the existing probe irradiance.
    //
    // When using lower bit depth formats for irradiance, the difference between lerped values
    // may be smaller than what the texture format can represent. This can stop progress towards
    // the target value when going from high to low values. When darkening, step at least the minimum
    // value the texture format can represent to ensure the target value is reached. The threshold value
    // for 10-bit/channel formats is always used (even for 32-bit/channel formats) to speed up light to
    // dark convergence.
    static const float c_threshold = 1.f / 1024.f;
    float3 lerpDelta = (1.f - hysteresis) * delta;
    if (RTXGIMaxComponent(result.rgb) < RTXGIMaxComponent(previous.rgb))
    {
        lerpDelta = min(max(c_threshold, abs(lerpDelta)), abs(delta)) * sign(lerpDelta);
    }
    result = float4(previous.rgb + lerpDelta, 1.f);
#else

    // Interpolate the new filtered distance with the existing filtered distance in the probe.
    // A high hysteresis value emphasizes the existing probe filtered distance.
    result = float4(lerp(result.rg, previous.rg, hysteresis), 0.f, 1.f);
#endif

    Output[atlasTexCoords] = result;
}
