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

#include "include/validation/ProbeBlendingDefines.hlsl"

// -------- REGISTER DECLARATIONS -----------------------------------------------------------------

#if RTXGI_DDGI_SHADER_REFLECTION || defined(__spirv__)

    // Don't declare registers when using reflection or cross-compiling to SPIRV
    #define VOLUME_CONSTS_REG_DECL 
    #if RTXGI_DDGI_BINDLESS_RESOURCES
        #define VOLUME_RESOURCES_REG_DECL 
        #define RWTEX2DARRAY_REG_DECL 
    #else
        #define RAY_DATA_REG_DECL 
        #define OUTPUT_REG_DECL 
        #define PROBE_DATA_REG_DECL
        #if RTXGI_DDGI_BLEND_RADIANCE
        #define PROBE_VARIABILITY_REG_DECL
        #endif
    #endif

#else

    // Declare registers and spaces when using D3D without reflection
    #define VOLUME_CONSTS_REG_DECL : register(VOLUME_CONSTS_REGISTER, VOLUME_CONSTS_SPACE)
    #if RTXGI_DDGI_BINDLESS_RESOURCES
        #if RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS
            #define VOLUME_RESOURCES_REG_DECL : register(VOLUME_RESOURCES_REGISTER, VOLUME_RESOURCES_SPACE)
            #define RWTEX2DARRAY_REG_DECL : register(RWTEX2DARRAY_REGISTER, RWTEX2DARRAY_SPACE)
        #endif
    #else
        #define RAY_DATA_REG_DECL : register(RAY_DATA_REGISTER, RAY_DATA_SPACE)
        #define OUTPUT_REG_DECL : register(OUTPUT_REGISTER, OUTPUT_SPACE)
        #define PROBE_DATA_REG_DECL : register(PROBE_DATA_REGISTER, PROBE_DATA_SPACE)
        #if RTXGI_DDGI_BLEND_RADIANCE
        #define PROBE_VARIABILITY_REG_DECL : register(PROBE_VARIABILITY_REGISTER, PROBE_VARIABILITY_SPACE)
        #endif
    #endif // RTXGI_DDGI_BINDLESS_RESOURCES

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

    // DDGIVolume probe irradiance or distance arrays
    RTXGI_VK_BINDING(OUTPUT_REGISTER, OUTPUT_SPACE)
    RWTexture2DArray<float4> Output OUTPUT_REG_DECL;

    // Probe data (world-space offsets and classification states)
    RTXGI_VK_BINDING(PROBE_DATA_REGISTER, PROBE_DATA_SPACE)
    RWTexture2DArray<float4> ProbeData PROBE_DATA_REG_DECL;

#if RTXGI_DDGI_BLEND_RADIANCE
    // Probe variability
    RTXGI_VK_BINDING(PROBE_VARIABILITY_REGISTER, PROBE_VARIABILITY_SPACE)
    RWTexture2DArray<float4> ProbeVariability PROBE_VARIABILITY_REG_DECL;
#endif

#endif // RTXGI_DDGI_BINDLESS_RESOURCES

// -------- SHARED MEMORY DECLARATIONS ------------------------------------------------------------

#if RTXGI_DDGI_BLEND_SHARED_MEMORY
// Shared Memory (example with default settings):
// Radiance (float3) x 256 rays/probe = 768 floats (3 KB)
// Distance (float) x 256 rays/probe = 256 floats (~1 KB)
// Ray Directions (float3 x 256 rays/probe) = 768 floats (~3 KB)
//
// Shared Memory Use = 7 KB (3 KB radiance + 1 KB distance + ~3 KB directions)

// Example usage:
// Irradiance thread groups as 8 x 8 = 64 threads
//     Group threads load 256 ray radiance & distance values / 64 threads = 4 ray radiance & distance values / thread
//     Group threads compute 256 ray directions / 64 threads = 4 directions / thread
// Distance thread groups are 16 x 16 = 256 threads
//     Group threads load 256 ray distances / 256 threads = ~1 ray distance value / thread
//     Group threads compute 256 ray directions / 256 threads = ~1 ray direction / thread

#if RTXGI_DDGI_BLEND_RADIANCE
    groupshared float3 RayRadiance[RTXGI_DDGI_BLEND_RAYS_PER_PROBE];
#endif
    groupshared float  RayDistance[RTXGI_DDGI_BLEND_RAYS_PER_PROBE];
    groupshared float3 RayDirection[RTXGI_DDGI_BLEND_RAYS_PER_PROBE];
#endif // RTXGI_DDGI_BLEND_SHARED_MEMORY

#if RTXGI_DDGI_BLEND_SCROLL_SHARED_MEMORY
    groupshared bool scrollClear;
#endif // RTXGI_DDGI_BLEND_SCROLL_SHARED_MEMORY

// -------- VISUALIZATION FUNCTIONS ---------------------------------------------------------------

#if RTXGI_DDGI_BLEND_RADIANCE && RTXGI_DDGI_DEBUG_PROBE_INDEXING
    void DebugProbeIndexing(int probeIndex, int3 outputCoords, RWTexture2DArray<float4> Output, DDGIVolumeDescGPU volume)
    {
        if(volume.probeIrradianceFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x4)
        {
            Output[outputCoords] = float4(probeIndex, 0, 0, 1);
        }
    }
#endif // RTXGI_DDGI_BLEND_RADIANCE && RTXGI_DDGI_DEBUG_PROBE_INDEXING

#if RTXGI_DDGI_BLEND_RADIANCE && RTXGI_DDGI_DEBUG_OCTAHEDRAL_INDEXING
    void DebugOctahedralIndexing(int2 threadCoords, int3 outputCoords, RWTexture2DArray<float4> Output, DDGIVolumeDescGPU volume)
    {
        if(volume.probeIrradianceFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x4)
        {
            float4 result = 0;
            float2 probeOctantUV = DDGIGetNormalizedOctahedralCoordinates(threadCoords, RTXGI_DDGI_PROBE_NUM_INTERIOR_TEXELS);
            if (all(abs(probeOctantUV) <= 1.f))
            {
                float3 probeDirection = DDGIGetOctahedralDirection(probeOctantUV);
                probeDirection = (abs(probeDirection) >= 0.001f) * sign(probeDirection);    // Robustness for when the octant size is not a power of 2.
                result = float4((probeDirection * 0.5f) + 0.5f, 1.f);
            }
            Output[outputCoords] = result;
            return;
        }
    }
#endif // RTXGI_DDGI_BLEND_RADIANCE && RTXGI_DDGI_DEBUG_OCTAHEDRAL_INDEXING

// -------- HELPER FUNCTIONS ----------------------------------------------------------------------

#if RTXGI_DDGI_BLEND_SHARED_MEMORY
    // Cooperatively load the ray radiance and hit distance values into shared memory
    // Cooperatively compute probe ray directions
    void LoadSharedMemory(int probeIndex, uint GroupIndex, RWTexture2DArray<float4> RayData, DDGIVolumeDescGPU volume)
    {
        int totalIterations = int(ceil(float(RTXGI_DDGI_BLEND_RAYS_PER_PROBE) / float(RTXGI_DDGI_PROBE_NUM_TEXELS * RTXGI_DDGI_PROBE_NUM_TEXELS)));
        for (int iteration = 0; iteration < totalIterations; iteration++)
        {
            int rayIndex = (GroupIndex * totalIterations) + iteration;
            if (rayIndex >= RTXGI_DDGI_BLEND_RAYS_PER_PROBE) break;

            // Get the coordinates for the probe ray in the RayData texture array
            uint3 rayDataTexCoords = DDGIGetRayDataTexelCoords(rayIndex, probeIndex, volume);

        #if RTXGI_DDGI_BLEND_RADIANCE
            // Load the ray radiance and store it in shared memory
            RayRadiance[rayIndex] = DDGILoadProbeRayRadiance(RayData, rayDataTexCoords, volume);
        #endif

            // Load the ray hit distance and store it in shared memory
            RayDistance[rayIndex] = DDGILoadProbeRayDistance(RayData, rayDataTexCoords, volume);

            // Get a random normalized probe ray direction and store it in shared memory
            RayDirection[rayIndex] = DDGIGetProbeRayDirection(rayIndex, volume);
        }

        // Wait for all threads in the group to finish their shared memory operations
        GroupMemoryBarrierWithGroupSync();
    }
#endif // RTXGI_DDGI_BLEND_SHARED_MEMORY

#if RTXGI_DDGI_BLEND_SCROLL_SHARED_MEMORY
    // The first thread in a thread group determines if the probe has been scrolled
    // If scrolled, the texels of the probe should be cleared and blending can be skipped
    void LoadScrollSharedMemory(int probeIndex, uint3 outputCoords, uint3 GroupThreadID, RWTexture2DArray<float4> Output, DDGIVolumeDescGPU volume)
    {
        // Initialize the groupshared variable to not clear the probe texels (no scroll has occured)
        scrollClear = false;

        // Check if probe scrolling should clear this probe's texels
        uint groupThreadIdx = (GroupThreadID.x + GroupThreadID.y);
        if(groupThreadIdx == 0 && IsVolumeMovementScrolling(volume))
        {
            // Get the probe's grid coordinates
            int3 probeCoords = DDGIGetProbeCoords(probeIndex, volume);

            // Clear texels for probes that have been scrolled
            scrollClear |= DDGIClearScrolledPlane(probeCoords, 0, volume);
            scrollClear |= DDGIClearScrolledPlane(probeCoords, 1, volume);
            scrollClear |= DDGIClearScrolledPlane(probeCoords, 2, volume);
        }

        // Wait for all threads in the group to finish their shared memory operations
        GroupMemoryBarrierWithGroupSync();
    }
#endif // RTXGI_DDGI_BLEND_SCROLL_SHARED_MEMORY

// When the thread maps to a border texel, update it with the latest blended information for later use in bilinear filtering
void UpdateBorderTexel(uint3 DispatchThreadID, uint3 GroupThreadID, uint3 GroupID, RWTexture2DArray<float4> Output, DDGIVolumeDescGPU volume)
{
    bool isCornerTexel = (GroupThreadID.x == 0 || GroupThreadID.x == (RTXGI_DDGI_PROBE_NUM_TEXELS - 1)) && (GroupThreadID.y == 0 || GroupThreadID.y == (RTXGI_DDGI_PROBE_NUM_TEXELS - 1));
    bool isRowTexel = (GroupThreadID.x > 0 && GroupThreadID.x < (RTXGI_DDGI_PROBE_NUM_TEXELS - 1));

    uint3 copyCoordinates = uint3(GroupID.x * RTXGI_DDGI_PROBE_NUM_TEXELS, GroupID.y * RTXGI_DDGI_PROBE_NUM_TEXELS, DispatchThreadID.z);

    if(isCornerTexel)
    {
        copyCoordinates.x += GroupThreadID.x > 0 ? 1 : RTXGI_DDGI_PROBE_NUM_INTERIOR_TEXELS;
        copyCoordinates.y += GroupThreadID.y > 0 ? 1 : RTXGI_DDGI_PROBE_NUM_INTERIOR_TEXELS;
    }
    else if(isRowTexel)
    {
        copyCoordinates.x += (RTXGI_DDGI_PROBE_NUM_TEXELS - 1) - GroupThreadID.x;
        copyCoordinates.y += GroupThreadID.y + ((GroupThreadID.y > 0) ? -1 : 1);
    }
    else // Column Texel
    {
        copyCoordinates.x += GroupThreadID.x + ((GroupThreadID.x > 0) ? -1 : 1);
        copyCoordinates.y += (RTXGI_DDGI_PROBE_NUM_TEXELS - 1) - GroupThreadID.y;
    }

    // Visualize border copy indexing and exit early
#if RTXGI_DDGI_BLEND_RADIANCE && RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING
    if(volume.probeIrradianceFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_F32x4)
    {
        Output[DispatchThreadID] = float4(DispatchThreadID.xy, copyCoordinates.xy);
    }
    return;
#endif

    Output[DispatchThreadID] = Output[copyCoordinates];
}


// -------- ENTRY POINT ---------------------------------------------------------------------------

[numthreads(RTXGI_DDGI_PROBE_NUM_TEXELS, RTXGI_DDGI_PROBE_NUM_TEXELS, 1)]
void DDGIProbeBlendingCS(
    uint3 DispatchThreadID : SV_DispatchThreadID,
    uint3 GroupThreadID    : SV_GroupThreadID,
    uint3 GroupID          : SV_GroupID,
    uint  GroupIndex       : SV_GroupIndex)
{
    // Determine if this thread maps to a probe border texel
    bool isBorderTexel = (GroupThreadID.x == 0 || GroupThreadID.x == (RTXGI_DDGI_PROBE_NUM_INTERIOR_TEXELS + 1)); // Border Columns
    isBorderTexel |= (GroupThreadID.y == 0 || GroupThreadID.y == (RTXGI_DDGI_PROBE_NUM_INTERIOR_TEXELS + 1));     // Border Rows

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

    // Get the volume's resources
#if RTXGI_DDGI_BINDLESS_RESOURCES
    #if RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_DESCRIPTOR_HEAP

        // Get the volume's resource indices from the descriptor heap (SM6.6+ only)
        StructuredBuffer<DDGIVolumeResourceIndices> DDGIVolumeBindless = ResourceDescriptorHeap[GetDDGIVolumeResourceIndicesIndex()];
        DDGIVolumeResourceIndices resourceIndices = DDGIVolumeBindless[volumeIndex];

        // Get the volume's texture array UAVs from the descriptor heap (SM6.6+ only)
        RWTexture2DArray<float4> RayData = ResourceDescriptorHeap[resourceIndices.rayDataUAVIndex];
        #if RTXGI_DDGI_BLEND_RADIANCE
            RWTexture2DArray<float4> Output = ResourceDescriptorHeap[resourceIndices.probeIrradianceUAVIndex];
            RWTexture2DArray<float4> ProbeVariability = ResourceDescriptorHeap[resourceIndices.probeVariabilityUAVIndex];
        #else
            RWTexture2DArray<float4> Output = ResourceDescriptorHeap[resourceIndices.probeDistanceUAVIndex];
        #endif
        RWTexture2DArray<float4> ProbeData = ResourceDescriptorHeap[resourceIndices.probeDataUAVIndex];

    #elif RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS

        // Get the volume's resource indices
        DDGIVolumeResourceIndices resourceIndices = DDGIVolumeBindless[volumeIndex];

        // Get the volume's texture array UAVs
        RWTexture2DArray<float4> RayData = RWTex2DArray[resourceIndices.rayDataUAVIndex];
        #if RTXGI_DDGI_BLEND_RADIANCE
            RWTexture2DArray<float4> Output = RWTex2DArray[resourceIndices.probeIrradianceUAVIndex];
            RWTexture2DArray<float4> ProbeVariability = RWTex2DArray[resourceIndices.probeVariabilityUAVIndex];
        #else
            RWTexture2DArray<float4> Output = RWTex2DArray[resourceIndices.probeDistanceUAVIndex];
        #endif
        RWTexture2DArray<float4> ProbeData = RWTex2DArray[resourceIndices.probeDataUAVIndex];

    #endif
#endif

    // Find the probe index for this thread
    int probeIndex = DDGIGetProbeIndex(DispatchThreadID, RTXGI_DDGI_PROBE_NUM_TEXELS, volume);

    // Visualize the probe's index
#if RTXGI_DDGI_BLEND_RADIANCE && RTXGI_DDGI_DEBUG_PROBE_INDEXING
    DebugProbeIndexing(probeIndex, DispatchThreadID, Output, volume);
    return;
#endif

    // Get the number of probes
    uint numProbes = (volume.probeCounts.x * volume.probeCounts.y * volume.probeCounts.z);

    // Early out: no probe maps to this thread
    if (probeIndex >= numProbes || probeIndex < 0) return;

#if RTXGI_DDGI_BLEND_SHARED_MEMORY
    // Cooperatively load the ray radiance and hit distance values into shared memory and cooperatively compute probe ray directions
    LoadSharedMemory(probeIndex, GroupIndex, RayData, volume);
#endif // RTXGI_DDGI_BLEND_SHARED_MEMORY

    if(!isBorderTexel)
    {
        // Remap thread coordinates to not include the border texels
        int3 threadCoords = int3(GroupID.x * RTXGI_DDGI_PROBE_NUM_INTERIOR_TEXELS, GroupID.y * RTXGI_DDGI_PROBE_NUM_INTERIOR_TEXELS, DispatchThreadID.z) + GroupThreadID - int3(1, 1, 0);

        // Visualize the probe's octahedral indexing
    #if RTXGI_DDGI_BLEND_RADIANCE && RTXGI_DDGI_DEBUG_OCTAHEDRAL_INDEXING
        DebugOctahedralIndexing(int2(threadCoords.xy), DispatchThreadID, Output, volume);
        return;
    #endif

        // Determine if a scrolled probe should be cleared
        // Using shared memory may or may not be worth it depending on the target HW
    #if RTXGI_DDGI_BLEND_SCROLL_SHARED_MEMORY
        LoadScrollSharedMemory(probeIndex, DispatchThreadID, GroupThreadID, Output, volume);
        if(scrollClear)
        {
            Output[DispatchThreadID] = float4(0.f, 0.f, 0.f, 1.f);
            return; // Early out: this probe has been scrolled and cleared, don't blend
        }
    #else
        if (IsVolumeMovementScrolling(volume))
        {
            // Get the probe's grid coordinates
            int3 probeCoords = DDGIGetProbeCoords(probeIndex, volume);

            // Clear texels for probes that have been scrolled
            bool scrollClear = false;
            scrollClear |= DDGIClearScrolledPlane(probeCoords, 0, volume);
            scrollClear |= DDGIClearScrolledPlane(probeCoords, 1, volume);
            scrollClear |= DDGIClearScrolledPlane(probeCoords, 2, volume);
            if(scrollClear)
            {
                Output[DispatchThreadID] = float4(0.f, 0.f, 0.f, 1.f);
                return; // Early out: this probe has been scrolled and cleared, don't blend
            }
        }
    #endif // RTXGI_DDGI_BLEND_SCROLL_SHARED_MEMORY

        // Early out: don't blend rays for probes that are inactive
        int probeState = DDGILoadProbeState(probeIndex, ProbeData, volume);
        if (probeState == RTXGI_DDGI_PROBE_STATE_INACTIVE)
        {
        #if RTXGI_DDGI_BLEND_RADIANCE
            ProbeVariability[DispatchThreadID].r = 0.f;
        #endif
            return;
        }

        // Get the probe ray direction associated with this thread
        float2 probeOctantUV = DDGIGetNormalizedOctahedralCoordinates(int2(threadCoords.xy), RTXGI_DDGI_PROBE_NUM_INTERIOR_TEXELS);
        float3 probeRayDirection = DDGIGetOctahedralDirection(probeOctantUV);

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
        float4 result = float4(0.f, 0.f, 0.f, 0.f);
        for ( ; rayIndex < volume.probeNumRays; rayIndex++)
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

            // Get the coordinates for the probe ray in the RayData texture array
            uint3 rayDataTexCoords = DDGIGetRayDataTexelCoords(rayIndex, probeIndex, volume);

        #if RTXGI_DDGI_BLEND_RADIANCE
            // Load the ray traced radiance and hit distance
            float3 probeRayRadiance = 0.f;
            float  probeRayDistance = 0.f;

            #if RTXGI_DDGI_BLEND_SHARED_MEMORY
                probeRayRadiance = RayRadiance[rayIndex];
                probeRayDistance = RayDistance[rayIndex];
            #else
                probeRayRadiance = DDGILoadProbeRayRadiance(RayData, rayDataTexCoords, volume);
                probeRayDistance = DDGILoadProbeRayDistance(RayData, rayDataTexCoords, volume);
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
            probeRayDistance = min(abs(DDGILoadProbeRayDistance(RayData, rayDataTexCoords, volume)), probeMaxRayDistance);
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
        result.a = 1.f;

        // Get the previous irradiance in the probe
        float3 previous = Output[DispatchThreadID].rgb;

        // Get the history weight (hysteresis) to use for the probe texel's previous value
        // If the probe was previously cleared to completely black, set the hysteresis to zero
        float  hysteresis = volume.probeHysteresis;
        if (dot(previous, previous) == 0) hysteresis = 0.f;

    #if RTXGI_DDGI_BLEND_RADIANCE
        // Tone-mapping gamma adjustment
        result.rgb = pow(result.rgb, (1.f / volume.probeIrradianceEncodingGamma));

        float3 delta = (result.rgb - previous.rgb);

        float3 previousIrradianceMean = previous.rgb;
        float3 currentIrradianceSample = result.rgb;

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

        if (volume.probeVariabilityEnabled)
        {
            float3 newIrradianceMean = result.rgb;
            float3 newIrradianceSigma2 = (currentIrradianceSample - previousIrradianceMean) * (currentIrradianceSample - newIrradianceMean);
            float newLuminanceSigma2 = RTXGILinearRGBToLuminance(newIrradianceSigma2);
            float newLuminanceMean = RTXGILinearRGBToLuminance(newIrradianceMean);
            float coefficientOfVariation = (newLuminanceMean <= c_threshold) ? 0.f : sqrt(newLuminanceSigma2) / newLuminanceMean;
            ProbeVariability[threadCoords].r = coefficientOfVariation;
        }
    #else

        // Interpolate the new filtered distance with the existing filtered distance in the probe.
        // A high hysteresis value emphasizes the existing probe filtered distance.
        result = float4(lerp(result.rg, previous.rg, hysteresis), 0.f, 1.f);
    #endif

        Output[DispatchThreadID] = result;
        return;
    }

    // Wait for all threads in the group to finish all memory operations
    AllMemoryBarrierWithGroupSync();

    // Update the texel with the latest blended data
    UpdateBorderTexel(DispatchThreadID, GroupThreadID, GroupID, Output, volume);
}
