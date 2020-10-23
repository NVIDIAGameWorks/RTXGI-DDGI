/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "../../../rtxgi-sdk/shaders/ddgi/Irradiance.hlsl"

#include "include/Common.hlsl"
#include "include/RasterRS.hlsl"
#include "include/Random.hlsl"

// ---[ Structures ]---

struct PSInput
{
    float4 position    : SV_POSITION;
    float2 tex0        : TEXCOORD;
};

// ---[ Vertex Shader ]---

PSInput VS(uint VertID : SV_VertexID)
{
    float div2 = (VertID / 2);
    float mod2 = (VertID % 2);

    PSInput result;
    result.position.x = (mod2 * 4.f) - 1.f;
    result.position.y = (div2 * 4.f) - 1.f;
    result.position.zw = float2(0.f, 1.f);

    result.tex0.x = mod2 * 2.f;
    result.tex0.y = 1.f - (div2 * 2.f);

    return result;
}

// ---[ Pixel Shader ]---

float4 PS(PSInput input) : SV_TARGET
{
    float3 result = 0.f;
    float  ambientOcclusion = 1.f;

    // Load the albedo
    float4 albedo = GBufferA.Load(input.position.xy);
    result = albedo.rgb;

    // Primary ray hit, need to light it
    if (albedo.a > 0.f)
    {
        float4 worldPosHitT = GBufferB.Load(input.position.xy);
        float3 normal = GBufferC.Load(input.position.xy).xyz;
        float3 diffuse = GBufferD.Load(input.position.xy).rgb;

        // Load ambient occlusion and multiply with diffuse lighting
        ambientOcclusion = UseRTAO ? RTAOFiltered.Load(input.position.xy) : 1.f;
        diffuse *= ambientOcclusion;

        // Indirect Lighting
        float3 irradiance = 0.f;
        float  blendTotal = 1e-9f;   // Start blendTotal with a very small value to avoid divide by zero
#if RTXGI_DDGI_COMPUTE_IRRADIANCE
        // Iterate over the DDGIVolumes
        for (uint i = 0; (i < volumeSelect) && (blendTotal < 1.f); i++)
        {
            DDGIVolumeDescGPU DDGIVolume = DDGIVolumes.volumes[i];
            float3 cameraDirection = normalize(worldPosHitT.xyz - cameraPosition);
            float3 surfaceBias = DDGIGetSurfaceBias(normal, cameraDirection, DDGIVolume);

            DDGIVolumeResources resources;
            resources.probeIrradianceSRV = GetDDGIProbeIrradianceSRV(i);
            resources.probeDistanceSRV = GetDDGIProbeDistanceSRV(i);
            resources.bilinearSampler = BilinearSampler;
#if RTXGI_DDGI_PROBE_RELOCATION
            resources.probeOffsets = GetDDGIProbeOffsetsUAV(i);
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
            resources.probeStates = GetDDGIProbeStatesUAV(i);
#endif
            float weight = DDGIGetVolumeBlendWeight(worldPosHitT.xyz, DDGIVolume);
            // Get irradiance from the DDGIVolume
            irradiance += weight * DDGIGetVolumeIrradiance(
                worldPosHitT.xyz,
                surfaceBias,
                normal,
                DDGIVolume,
                resources);
            blendTotal += weight;
        }

        // Normalize by blendTotal
        irradiance /= blendTotal;

        // Attenuate irradiance by ambient occlusion
        irradiance *= ambientOcclusion;
#endif

        // Compute final color
        result = diffuse;
        if (UseDDGI == 1)
        {
            result += (albedo.rgb / PI) * irradiance;
        }
    }

    if (ViewAO)
    {
        // Visualize the ambient occlusion data
        return float4(LinearToSRGB(ambientOcclusion.xxx), 1.f);
    }

    // Apply exposure
    if (UseExposure)
    {
        result *= Exposure;
    }

    // Apply tonemapping
    if (UseTonemapping)
    {
        result = ACESFilm(result);
    }

    // Add noise to handle SDR color banding
    if (UseDithering)
    {
        result += GetLowDiscrepancyBlueNoise(int2(input.position.xy), FrameNumber, 1.f / 256.f, BlueNoiseRGB);
    }

    // Gamma correct
    result = LinearToSRGB(result);

    return float4(result, 1.f);
}
