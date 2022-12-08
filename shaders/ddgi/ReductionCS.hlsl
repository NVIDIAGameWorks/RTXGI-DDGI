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

#include "include/validation/ReductionDefines.hlsl"

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
        #define PROBE_VARIABILITY_REG_DECL 
        #define PROBE_VARIABILITY_AVERAGE_REG_DECL
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
        #define PROBE_VARIABILITY_REG_DECL : register(PROBE_VARIABILITY_REGISTER, PROBE_VARIABILITY_SPACE)
        #define PROBE_VARIABILITY_AVERAGE_REG_DECL : register(PROBE_VARIABILITY_AVERAGE_REGISTER, PROBE_VARIABILITY_SPACE)
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

    // Probe data (world-space offsets and classification states)
    RTXGI_VK_BINDING(PROBE_DATA_REGISTER, PROBE_DATA_SPACE)
    RWTexture2DArray<float4> ProbeData PROBE_DATA_REG_DECL;

    // Probe variability
    RTXGI_VK_BINDING(PROBE_VARIABILITY_REGISTER, PROBE_VARIABILITY_SPACE)
    RWTexture2DArray<float4> ProbeVariability PROBE_VARIABILITY_REG_DECL;

    // Probe variability average
    RTXGI_VK_BINDING(PROBE_VARIABILITY_AVERAGE_REGISTER, PROBE_VARIABILITY_SPACE)
    RWTexture2DArray<float4> ProbeVariabilityAverage PROBE_VARIABILITY_AVERAGE_REG_DECL;

#endif // RTXGI_DDGI_BINDLESS_RESOURCES

// -------- SHARED MEMORY DECLARATIONS ------------------------------------------------------------

#define NUM_THREADS_X 4
#define NUM_THREADS_Y 8
#define NUM_THREADS_Z 4
#define NUM_THREADS NUM_THREADS_X*NUM_THREADS_Y*NUM_THREADS_Z
#define NUM_WAVES NUM_THREADS / RTXGI_DDGI_WAVE_LANE_COUNT

groupshared float ThreadGroupSum[NUM_WAVES];
groupshared uint MaxSumEntry;
groupshared uint NumTotalSamples;

// -------- HELPER FUNCTIONS ----------------------------------------------------------------------

// Sums values in the ThreadGroupSum shared memory array, from 0 to MaxSumEntry
// At the end of the function, ThreadGroupSum[0] should have the total of the whole array
void reduceSharedMemorySum(uint ThreadIndexInGroup, uint waveIndex, uint waveLaneCount)
{
    uint numSharedMemoryEntries = MaxSumEntry + 1;
    uint activeThreads = numSharedMemoryEntries;
    while (activeThreads > 1)
    {
        bool usefulThread = ThreadIndexInGroup < activeThreads;
        if (usefulThread)
        {
            float value = ThreadGroupSum[ThreadIndexInGroup];
            GroupMemoryBarrierWithGroupSync();

            float warpTotalValue = WaveActiveSum(value);

            if (WaveIsFirstLane())
            {
                ThreadGroupSum[waveIndex] = warpTotalValue;
            }
            GroupMemoryBarrierWithGroupSync();
        }
        // Divide by wave size, rounding up (ceil)
        activeThreads = (activeThreads + waveLaneCount - 1) / waveLaneCount;
    }
}

// -------- ENTRY POINT ---------------------------------------------------------------------------

[numthreads(NUM_THREADS_X, NUM_THREADS_Y, NUM_THREADS_Z)]
void DDGIReductionCS(uint3 GroupID : SV_GroupID, uint3 GroupThreadID : SV_GroupThreadID, uint ThreadIndexInGroup : SV_GroupIndex)
{
    if (ThreadIndexInGroup == 0)
    {
        MaxSumEntry = 0;
        NumTotalSamples = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    // Doing 4x2 samples per thread
    const uint3 ThreadSampleFootprint = uint3(4, 2, 1);

    uint3 groupCoordOffset = GroupID.xyz * uint3(NUM_THREADS_X, NUM_THREADS_Y, NUM_THREADS_Z) * ThreadSampleFootprint;
    uint3 threadCoordInGroup = GroupThreadID.xyz;
    uint3 threadCoordGlobal = groupCoordOffset + threadCoordInGroup * ThreadSampleFootprint;

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
        RWTexture2DArray<float4> ProbeVariability = ResourceDescriptorHeap[resourceIndices.probeVariabilityUAVIndex];
        RWTexture2DArray<float4> ProbeVariabilityAverage = ResourceDescriptorHeap[resourceIndices.probeVariabilityAverageUAVIndex];
        RWTexture2DArray<float4> ProbeData = ResourceDescriptorHeap[resourceIndices.probeDataUAVIndex];

    #elif RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS

        // Get the volume's resource indices
        DDGIVolumeResourceIndices resourceIndices = DDGIVolumeBindless[volumeIndex];

        // Get the volume's texture array UAVs
        RWTexture2DArray<float4> ProbeVariability = RWTex2DArray[resourceIndices.probeVariabilityUAVIndex];
        RWTexture2DArray<float4> ProbeVariabilityAverage = RWTex2DArray[resourceIndices.probeVariabilityAverageUAVIndex];
        RWTexture2DArray<float4> ProbeData = RWTex2DArray[resourceIndices.probeDataUAVIndex];

    #endif
#endif

    uint waveLaneCount = WaveGetLaneCount();
    uint wavesPerThreadGroup = NUM_THREADS / waveLaneCount;
    uint waveIndex = ThreadIndexInGroup / waveLaneCount;

    // Total size of the input variability texture
    uint3 probeVariabilitySize = GetReductionInputSize();

    float sampleSum = 0;
    uint numSamples = 0;
    for (uint i = 0; i < ThreadSampleFootprint.x; i++)
    {
        for (uint j = 0; j < ThreadSampleFootprint.y; j++)
        {
            uint3 sampleCoord = threadCoordGlobal + uint3(i, j, 0);
            // Iterating over non-border samples of the irradiance texture
            // Calling GetProbeIndex with NUM_INTERIOR_TEXELS (instead of NUM_TEXELS) to make
            // sample coordinates line up with probe indices and avoid sampling border texels
            int probeIndex = DDGIGetProbeIndex(sampleCoord, RTXGI_DDGI_PROBE_NUM_INTERIOR_TEXELS, volume);
            bool sampleInBounds = all(sampleCoord < probeVariabilitySize);
            if (sampleInBounds)
            {
                float value = ProbeVariability[sampleCoord].r;

                // Skip inactive probes
                if (volume.probeClassificationEnabled)
                {
                    uint3 probeDataCoords = DDGIGetProbeTexelCoords(probeIndex, volume);
                    int  probeState = ProbeData[probeDataCoords].w;
                    if (probeState == RTXGI_DDGI_PROBE_STATE_INACTIVE)
                    {
                        value = 0.f;
                        continue;
                    }
                }

                sampleSum += value;
                numSamples++;
            }
        }
    }

    // Sum up the warp
    float waveTotalValue = WaveActiveSum(sampleSum);
    // Sum up useful sample count
    uint usefulSampleCount = WaveActiveSum(numSamples);
    // Write sum and sample count for this wave
    if (WaveIsFirstLane())
    {
        ThreadGroupSum[waveIndex] = waveTotalValue;
        InterlockedMax(MaxSumEntry, waveIndex);
        InterlockedAdd(NumTotalSamples, usefulSampleCount);
    }
    GroupMemoryBarrierWithGroupSync();
    reduceSharedMemorySum(ThreadIndexInGroup, waveIndex, waveLaneCount);

    if (ThreadIndexInGroup == 0)
    {
        float TotalPossibleSamples = NUM_THREADS * ThreadSampleFootprint.x * ThreadSampleFootprint.y;
        // Average value for the samples we took
        ProbeVariabilityAverage[GroupID.xyz].r = NumTotalSamples > 0 ? ThreadGroupSum[0] / NumTotalSamples : 0;
        // Normalizing "weight" factor for this thread group, to allow partial thread groups to average properly with full groups
        ProbeVariabilityAverage[GroupID.xyz].g = NumTotalSamples / TotalPossibleSamples;
    }
}

// -------- SHARED MEMORY DECLARATIONS ------------------------------------------------------------

groupshared float ThreadGroupAverage[NUM_WAVES];
groupshared uint MaxAverageEntry;
groupshared float ThreadGroupWeight[NUM_WAVES];

// -------- HELPER FUNCTIONS ----------------------------------------------------------------------

// Weighted average ThreadGroupAverage shared memory array, from 0 to MaxSumEntry
// At the end of the function, ThreadGroupAverage[0] should have the average of the whole array
// ThreadGroupWeight[0] will have the total weight of this thread group to be used when averaging with other groups
void reduceSharedMemoryAverage(uint ThreadIndexInGroup, uint waveIndex, uint waveLaneCount)
{
    uint numSharedMemoryEntries = MaxAverageEntry + 1;
    uint activeThreads = numSharedMemoryEntries;
    while (activeThreads > 1)
    {
        bool usefulThread = ThreadIndexInGroup < activeThreads;
        if (usefulThread)
        {
            float value = ThreadGroupAverage[ThreadIndexInGroup];
            float weight = ThreadGroupWeight[ThreadIndexInGroup];
            GroupMemoryBarrierWithGroupSync();

            float waveTotalValue = WaveActiveSum(weight*value);
            float waveTotalWeight = WaveActiveSum(weight);
            float TotalPossibleWeight = WaveActiveCountBits(true);

            if (WaveIsFirstLane())
            {
                ThreadGroupAverage[waveIndex] = waveTotalValue / waveTotalWeight;
                ThreadGroupWeight[waveIndex] = waveTotalWeight / TotalPossibleWeight;
            }
            GroupMemoryBarrierWithGroupSync();
        }
        activeThreads = (activeThreads + waveLaneCount - 1) / waveLaneCount;
    }
}

// -------- ENTRY POINT ---------------------------------------------------------------------------

[numthreads(NUM_THREADS_X, NUM_THREADS_Y, NUM_THREADS_Z)]
void DDGIExtraReductionCS(uint3 GroupID : SV_GroupID, uint3 GroupThreadID : SV_GroupThreadID, uint ThreadIndexInGroup : SV_GroupIndex)
{
    if (ThreadIndexInGroup == 0)
    {
        MaxAverageEntry = 0;
    }
    GroupMemoryBarrierWithGroupSync();

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
        RWTexture2DArray<float4> ProbeVariability = ResourceDescriptorHeap[resourceIndices.probeVariabilityUAVIndex];
        RWTexture2DArray<float4> ProbeVariabilityAverage = ResourceDescriptorHeap[resourceIndices.probeVariabilityAverageUAVIndex];
        RWTexture2DArray<float4> ProbeData = ResourceDescriptorHeap[resourceIndices.probeDataUAVIndex];

    #elif RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS

        // Get the volume's resource indices
        DDGIVolumeResourceIndices resourceIndices = DDGIVolumeBindless[volumeIndex];

        // Get the volume's texture array UAVs
        RWTexture2DArray<float4> ProbeVariability = RWTex2DArray[resourceIndices.probeVariabilityUAVIndex];
        RWTexture2DArray<float4> ProbeVariabilityAverage = RWTex2DArray[resourceIndices.probeVariabilityAverageUAVIndex];
        RWTexture2DArray<float4> ProbeData = RWTex2DArray[resourceIndices.probeDataUAVIndex];

    #endif
#endif

    uint waveLaneCount = WaveGetLaneCount();
    uint wavesPerThreadGroup = NUM_THREADS / waveLaneCount;
    uint waveIndex = ThreadIndexInGroup / waveLaneCount;

    // Doing 4x2 samples per thread
    const uint3 ThreadSampleFootprint = uint3(4, 2, 1);

    uint3 groupCoordOffset = GroupID.xyz * uint3(NUM_THREADS_X, NUM_THREADS_Y, NUM_THREADS_Z) * ThreadSampleFootprint;
    uint3 threadCoordInGroup = GroupThreadID.xyz;
    uint3 threadCoordGlobal = groupCoordOffset + threadCoordInGroup * ThreadSampleFootprint;
    uint3 inputSize = GetReductionInputSize();

    bool footprintInBounds = all(threadCoordGlobal < inputSize);
    float threadFootprintValueSum = 0;
    float threadFootprintWeightSum = 0;

    if (footprintInBounds)
    {
        for (uint i = 0; i < ThreadSampleFootprint.x; i++)
        {
            for (uint j = 0; j < ThreadSampleFootprint.y; j++)
            {
                uint3 sampleCoord = threadCoordGlobal + uint3(i, j, 0);
                bool sampleInBounds = all(sampleCoord < inputSize);
                if (sampleInBounds)
                {
                    float value = ProbeVariabilityAverage[sampleCoord].r;
                    float weight = ProbeVariabilityAverage[sampleCoord].g;
                    threadFootprintValueSum += weight * value;
                    threadFootprintWeightSum += weight;
                }
            }
        }
    }
    float threadAverageValue = footprintInBounds ? threadFootprintValueSum / threadFootprintWeightSum : 0;
    // Per-thread weight will be 1.0 if thread sampled all 4x2 pixels, 0.125 if it only sampled one
    float ThreadTotalPossibleWeight = ThreadSampleFootprint.x * ThreadSampleFootprint.y;
    float threadWeight = threadFootprintWeightSum / ThreadTotalPossibleWeight;

    // Sum up the warp
    float waveTotalValue = WaveActiveSum(threadWeight * threadAverageValue);
    float waveTotalWeight = WaveActiveSum(threadWeight);
    float waveTotalPossibleWeight = waveLaneCount * ThreadTotalPossibleWeight;

    if (WaveIsFirstLane() && WaveActiveAnyTrue(footprintInBounds))
    {
        ThreadGroupAverage[waveIndex] = waveTotalValue / waveTotalWeight;
        ThreadGroupWeight[waveIndex] = waveTotalWeight / waveTotalPossibleWeight;
        InterlockedMax(MaxSumEntry, waveIndex);
    }

    GroupMemoryBarrierWithGroupSync();
    reduceSharedMemoryAverage(ThreadIndexInGroup, waveIndex, waveLaneCount);
    if (ThreadIndexInGroup == 0)
    {
        ProbeVariabilityAverage[GroupID.xyz].r = ThreadGroupAverage[0];
        ProbeVariabilityAverage[GroupID.xyz].g = ThreadGroupWeight[0];
    }
}
