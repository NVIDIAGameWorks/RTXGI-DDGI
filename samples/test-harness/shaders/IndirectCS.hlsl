/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

// -------- CONFIGURATION DEFINES -----------------------------------------------------------------

// RTXGI_DDGI_NUM_VOLUMES must be passed in as a define at shader compilation time.
// This define specifies the number of DDGIVolumes in the scene.
// Ex: RTXGI_DDGI_NUM_VOLUMES 6
#ifndef RTXGI_DDGI_NUM_VOLUMES
    #error Required define RTXGI_DDGI_NUM_VOLUMES is not defined for IndirectCS.hlsl!
#endif

// THGP_DIM_X must be passed in as a define at shader compilation time.
// This define specifies the number of threads in the thread group in the X dimension.
// Ex: THGP_DIM_X 8
#ifndef THGP_DIM_X
    #error Required define THGP_DIM_X is not defined for IndirectCS.hlsl!
#endif

// THGP_DIM_Y must be passed in as a define at shader compilation time.
// This define specifies the number of threads in the thread group in the X dimension.
// Ex: THGP_DIM_Y 4
#ifndef THGP_DIM_Y
    #error Required define THGP_DIM_Y is not defined for IndirectCS.hlsl!
#endif

// -------------------------------------------------------------------------------------------

#include "include/Common.hlsl"
#include "include/Descriptors.hlsl"

#include "../../../rtxgi-sdk/shaders/ddgi/include/DDGIRootConstants.hlsl"
#include "../../../rtxgi-sdk/shaders/ddgi/Irradiance.hlsl"

// ---[ Compute Shader ]---

[numthreads(THGP_DIM_X, THGP_DIM_Y, 1)]
void CS(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    float3 color = float3(0.f, 0.f, 0.f);

    // Get the (bindless) resources
    RWTexture2D<float4> GBufferA = GetRWTex2D(GBUFFERA_INDEX);
    RWTexture2D<float4> GBufferB = GetRWTex2D(GBUFFERB_INDEX);
    RWTexture2D<float4> GBufferC = GetRWTex2D(GBUFFERC_INDEX);
    RWTexture2D<float4> DDGIOutput = GetRWTex2D(DDGI_OUTPUT_INDEX);

    // Load the albedo and primary ray hit distance
    float4 albedo = GBufferA.Load(DispatchThreadID.xy);

    // Primary ray hit, need to light it
    if (albedo.a > 0.f)
    {
        // Convert albedo back to linear
        albedo.rgb = SRGBToLinear(albedo.rgb);

        // Load the world position, hit distance, and normal
        float4 worldPosHitT = GBufferB.Load(DispatchThreadID.xy);
        float3 normal = GBufferC.Load(DispatchThreadID.xy).xyz;

        // Compute indirect lighting
        float3 irradiance = 0.f;

        // Get the structured buffers
        StructuredBuffer<DDGIVolumeDescGPUPacked> DDGIVolumes = GetDDGIVolumeConstants(GetDDGIVolumeConstantsIndex());
        StructuredBuffer<DDGIVolumeResourceIndices> DDGIVolumeBindless = GetDDGIVolumeResourceIndices(GetDDGIVolumeResourceIndicesIndex());

        // TODO: sort volumes by density, screen-space area, and/or other prioritization heuristics
        for(int volumeIndex = 0; volumeIndex < RTXGI_DDGI_NUM_VOLUMES; volumeIndex++)
        {
            // Get the DDGIVolume's resource indices
            DDGIVolumeResourceIndices resourceIndices = DDGIVolumeBindless[volumeIndex];

            // Get the volume's constants
            DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumes[volumeIndex]);

            float3 cameraDirection = normalize(worldPosHitT.xyz - GetCamera().position);
            float3 surfaceBias = DDGIGetSurfaceBias(normal, cameraDirection, volume);

            // Get the volume's resources
            DDGIVolumeResources resources;
            resources.probeIrradiance = GetTex2DArray(resourceIndices.probeIrradianceSRVIndex);
            resources.probeDistance = GetTex2DArray(resourceIndices.probeDistanceSRVIndex);
            resources.probeData = GetTex2DArray(resourceIndices.probeDataSRVIndex);
            resources.bilinearSampler = GetBilinearWrapSampler();

            // Get the blend weight for this volume's contribution to the surface
            float blendWeight = DDGIGetVolumeBlendWeight(worldPosHitT.xyz, volume);
            if(blendWeight > 0)
            {
                // Get irradiance for the world-space position in the volume
                irradiance += DDGIGetVolumeIrradiance(
                    worldPosHitT.xyz,
                    surfaceBias,
                    normal,
                    volume,
                    resources);

                irradiance *= blendWeight;
            }
        }

        // Compute final color
        color = (albedo.rgb / PI) * irradiance;
    }

    DDGIOutput[DispatchThreadID.xy] = float4(color, 1.f);
}
