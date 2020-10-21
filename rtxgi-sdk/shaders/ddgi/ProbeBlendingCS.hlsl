/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "ProbeCommon.hlsl"

// Note: PROBE_UAV_INDEX must be passed in as a define at shader compilation time
// See Harness.cpp::CompileShaders() in the Test Harness
// Index 0 cooresponds with the Probe Irradiance UAV
// Index 1 cooresponds with the Probe Distance UAV
// #define PROBE_UAV_INDEX [0|1]

// Note: PROBE_NUM_TEXELS must be passed in as a define at shader compilation time
// See Harness.cpp::CompileShaders() in the Test Harness application
//#define PROBE_NUM_TEXELS [6|14]

ConstantBuffer<DDGIVolumeDescGPU> DDGIVolume    : register(b1, space1);

// Probe ray traced radiance and hit distance
RWTexture2D<float4> DDGIProbeRTRadianceUAV      : register(u0, space1);

// Probe irradiance or filtered distance
RWTexture2D<float4> DDGIProbeUAV[2]             : register(u1, space1);

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
// Probe states
RWTexture2D<uint>   DDGIProbeStates             : register(u4, space1);
#endif

#if RTXGI_DDGI_BLENDING_USE_SHARED_MEMORY
// Note: When using shared memory, RAYS_PER_PROBE must be passed in as a define at shader compilation time 
// See Harness.cpp::CompileShaders() in the Test Harness application
// #define RAYS_PER_PROBE 144

// Shared Memory (example for default settings):
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
groupshared float3 RTRadiance[RAYS_PER_PROBE];
#endif
groupshared float  RTDistance[RAYS_PER_PROBE];
groupshared float3 RayDirection[RAYS_PER_PROBE];
#endif

[numthreads(PROBE_NUM_TEXELS, PROBE_NUM_TEXELS, 1)]
void DDGIProbeBlendingCS(uint3 DispatchThreadID : SV_DispatchThreadID, uint GroupIndex : SV_GroupIndex)
{
    float4 result = float4(0.f, 0.f, 0.f, 0.f);

    // Find the index of the probe that this thread maps to (for reading the RT radiance buffer)
    int probeIndex = DDGIGetProbeIndex(DispatchThreadID.xy, DDGIVolume.probeGridCounts, PROBE_NUM_TEXELS);
    if (probeIndex < 0)
    {
        return; // Probe doesn't exist
    }
#if RTXGI_DDGI_PROBE_SCROLL
    int storageProbeIndex = DDGIGetProbeIndexOffset(probeIndex, DDGIVolume.probeGridCounts, DDGIVolume.probeScrollOffsets);
    // Transform the probe index into probe texel coordinates
    // Offset 1 texel on X and Y to account for the 1 texel probe border
    uint2 intraProbeTexelOffset = DispatchThreadID.xy % uint2(PROBE_NUM_TEXELS, PROBE_NUM_TEXELS);
    uint2 probeTexCoords = DDGIGetThreadBaseCoords(storageProbeIndex, DDGIVolume.probeGridCounts, PROBE_NUM_TEXELS) + intraProbeTexelOffset;
    probeTexCoords.xy = probeTexCoords.xy + uint2(1, 1) + (probeTexCoords.xy / PROBE_NUM_TEXELS) * 2;
#else
    int storageProbeIndex = probeIndex;
    // Transform the thread dispatch index into probe texel coordinates
    // Offset 1 texel on X and Y to account for the 1 texel probe border
    uint2 probeTexCoords = DispatchThreadID.xy + uint2(1, 1);
    probeTexCoords.xy += (DispatchThreadID.xy / PROBE_NUM_TEXELS) * 2;
#endif

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
    int2 texelPosition = DDGIGetProbeTexelPosition(storageProbeIndex, DDGIVolume.probeGridCounts);
    int  probeState = DDGIProbeStates[texelPosition];
    if (probeState == PROBE_STATE_INACTIVE)
    {
        return; // If the probe is inactive, do not blend (it didn't shoot rays to get new radiance values)
    }
#endif /* RTXGI_DDGI_PROBE_STATE_CLASSIFIER */

#if RTXGI_DDGI_BLEND_RADIANCE && RTXGI_DDGI_DEBUG_PROBE_INDEXING && RTXGI_DDGI_DEBUG_FORMAT_IRRADIANCE
    // Visualize the probe index
    DDGIProbeUAV[0][probeTexCoords] = float4(probeIndex, 0, 0, 1);
    return;
#endif

    float2 probeOctantUV = float2(0.f, 0.f);

#if RTXGI_DDGI_BLEND_RADIANCE && RTXGI_DDGI_DEBUG_OCTAHEDRAL_INDEXING
    probeOctantUV = DDGIGetNormalizedOctahedralCoordinates(int2(DispatchThreadID.xy), PROBE_NUM_TEXELS);
    if (all(abs(probeOctantUV) <= 1.f))
    {
        float3 probeDirection = DDGIGetOctahedralDirection(probeOctantUV);
        probeDirection = (abs(probeDirection) >= 0.001f) * sign(probeDirection);    // Robustness for when the octant size is not a power of 2.
        result = float4((probeDirection * 0.5f) + 0.5f, 1.f);
    }
    DDGIProbeUAV[0][probeTexCoords] = result;
    return;
#endif

    // Get the probe ray direction associated with this thread
    probeOctantUV = DDGIGetNormalizedOctahedralCoordinates(int2(DispatchThreadID.xy), PROBE_NUM_TEXELS);
    float3 probeRayDirection = DDGIGetOctahedralDirection(probeOctantUV);

#if RTXGI_DDGI_BLENDING_USE_SHARED_MEMORY
    // Cooperatively load the ray traced radiance and hit distance values into shared memory
    // Cooperatively compute the probe ray directions
    int totalIterations = int(ceil(float(RAYS_PER_PROBE) / float(PROBE_NUM_TEXELS * PROBE_NUM_TEXELS)));
    for (int iteration = 0; iteration < totalIterations; iteration++)
    {
        int rayIndex = (GroupIndex * totalIterations) + iteration;
        if (rayIndex >= RAYS_PER_PROBE) break;

#if RTXGI_DDGI_BLEND_RADIANCE
#if RTXGI_DDGI_DEBUG_FORMAT_RADIANCE
        RTRadiance[rayIndex] = DDGIProbeRTRadianceUAV[int2(rayIndex, probeIndex)].rgb;
#else
        RTRadiance[rayIndex] = RTXGIUintToFloat3(asuint(DDGIProbeRTRadianceUAV[int2(rayIndex, probeIndex)].r));
#endif
#endif

#if RTXGI_DDGI_DEBUG_FORMAT_RADIANCE
        RTDistance[rayIndex] = DDGIProbeRTRadianceUAV[int2(rayIndex, probeIndex)].a;
#else
        RTDistance[rayIndex] = DDGIProbeRTRadianceUAV[int2(rayIndex, probeIndex)].g;
#endif

        RayDirection[rayIndex] = DDGIGetProbeRayDirection(rayIndex, DDGIVolume.numRaysPerProbe, DDGIVolume.probeRayRotationTransform);
    }

    // Wait for all threads in the group to finish shared memory operations
    GroupMemoryBarrierWithGroupSync();
#endif /* RTXGI_DDGI_BLENDING_USE_SHARED_MEMORY */

#if RTXGI_DDGI_BLEND_RADIANCE
    // Backface hits are ignored when blending radiance
    // Allow a maximum of 10% of the rays to hit backfaces. If that limit is exceeded, don't blend anything into this probe.
    uint backfaces = 0;
    uint maxBackfaces = DDGIVolume.numRaysPerProbe * 0.1f;
#endif

    int rayIndex = 0;
#if RTXGI_DDGI_PROBE_RELOCATION || RTXGI_DDGI_PROBE_STATE_CLASSIFIER
    rayIndex = RTXGI_DDGI_NUM_FIXED_RAYS;
#endif

    // Blend radiance or distance values from each ray to compute irradiance or fitered distance
    for ( /*rayIndex*/; rayIndex < DDGIVolume.numRaysPerProbe; rayIndex++)
    {
        // Get the direction for this probe ray
#if RTXGI_DDGI_BLENDING_USE_SHARED_MEMORY
        float3 rayDirection = RayDirection[rayIndex];
#else
        float3 rayDirection = DDGIGetProbeRayDirection(rayIndex, DDGIVolume.numRaysPerProbe, DDGIVolume.probeRayRotationTransform);
#endif

        // Find the weight of the contribution for this ray
        // Weight is based on the cosine of the angle between the ray direction and the direction of the probe octant's texel
        float weight = max(0.f, dot(probeRayDirection, rayDirection));

        // The indices of the probe ray in the radiance buffer
        int2 probeRayIndex = int2(rayIndex, probeIndex);

#if RTXGI_DDGI_BLEND_RADIANCE
        // Load the ray traced radiance and hit distance
#if RTXGI_DDGI_BLENDING_USE_SHARED_MEMORY
        float3 probeRayRadiance = RTRadiance[rayIndex];
        float  probeRayDistance = RTDistance[rayIndex];
#else
#if RTXGI_DDGI_DEBUG_FORMAT_RADIANCE
        float3 probeRayRadiance = DDGIProbeRTRadianceUAV[probeRayIndex].rgb;
        float  probeRayDistance = DDGIProbeRTRadianceUAV[probeRayIndex].a;
#else
        float3 probeRayRadiance = RTXGIUintToFloat3(asuint(DDGIProbeRTRadianceUAV[int2(rayIndex, probeIndex)].r));
        float  probeRayDistance = DDGIProbeRTRadianceUAV[probeRayIndex].g;
#endif
#endif

        // Backface hit, don't blend this sample
        if (probeRayDistance < 0.f)
        {
            backfaces++;
            if (backfaces >= maxBackfaces) return;
            continue;
        }

        // Blend the ray's radiance
        result += float4(probeRayRadiance * weight, weight);

#else /* !RTXGI_DDGI_BLEND_RADIANCE */

        // Initialize the probe hit distance to three quarters of the distance of the grid cell diagonal
        float probeMaxRayDistance = length(DDGIVolume.probeGridSpacing) * 0.75f;

        // Increase or decrease the filtered distance value's "sharpness"
        weight = pow(weight, DDGIVolume.probeDistanceExponent);

        // Load the ray traced distance
#if RTXGI_DDGI_BLENDING_USE_SHARED_MEMORY
        float probeRayDistance = min(abs(RTDistance[rayIndex]), probeMaxRayDistance);
#else
        // HitT is negative on backface hits for the probe relocation, so take the absolute value
#if RTXGI_DDGI_DEBUG_FORMAT_RADIANCE
        float probeRayDistance = min(abs(DDGIProbeRTRadianceUAV[probeRayIndex].a), probeMaxRayDistance);
#else
        float probeRayDistance = min(abs(DDGIProbeRTRadianceUAV[probeRayIndex].g), probeMaxRayDistance);
#endif
#endif

        // Filter the ray distance
        result += float4(probeRayDistance * weight, (probeRayDistance * probeRayDistance) * weight, 0.f, weight);
#endif
    }

    // Normalize the blended irradiance (or filtered distance), if the combined weight is not close to zero
    const float epsilon = 1e-9f * float(DDGIVolume.numRaysPerProbe);
    result.rgb *= 1.f / max(result.a, epsilon);

#if RTXGI_DDGI_BLEND_RADIANCE
    // Tone-mapping gamma adjustment
    result.rgb = pow(result.rgb, DDGIVolume.probeInverseIrradianceEncodingGamma);
#endif

    float  hysteresis = DDGIVolume.probeHysteresis;
    float3 previous = DDGIProbeUAV[PROBE_UAV_INDEX][probeTexCoords].rgb;

#if RTXGI_DDGI_BLEND_RADIANCE
    if (RTXGIMaxComponent(previous.rgb - result.rgb) > DDGIVolume.probeChangeThreshold)
    {
        // Lower the hysteresis when a large lighting change is detected
        hysteresis = max(0.f, hysteresis - 0.15f);
    }
    
    float3 delta = (result.rgb - previous.rgb);
    if (length(delta) > DDGIVolume.probeBrightnessThreshold)
    {
        // Clamp the maximum change in irradiance when a large brightness change is detected
        result.rgb = previous.rgb + (delta * 0.25f);
    }
#endif

    // Interpolate the new blended irradiance (or filtered distance) with the existing 
    // irradiance (or filtered distance) in the probe. A high hysteresis value emphasizes
    // the existing probe irradiance (or filtered distance).
    result = float4(lerp(result.rgb, previous.rgb, hysteresis), 1.f);

    DDGIProbeUAV[PROBE_UAV_INDEX][probeTexCoords] = result;
}