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

    // Load the baseColor
    float4 baseColor = RTGBufferA.Load(int3(input.position.xy, 0));
    result = baseColor.rgb;

    // Primary ray hit, need to light it
    if (baseColor.a > 0.f)
    {
        float4 worldPosHitT = RTGBufferB.Load(int3(input.position.xy, 0));
        float3 normal = RTGBufferC.Load(int3(input.position.xy, 0)).xyz;
        float3 diffuse = RTGBufferD.Load(int3(input.position.xy, 0)).rgb;
        
        // Load ambient occlusion and multiply with diffuse lighting
        ambientOcclusion = UseRTAO ? RTAOFiltered.Load(int3(input.position.xy, 0)) : 1.f;       
        diffuse *= ambientOcclusion;

        // Indirect Lighting
        float3 irradiance = 0.f;
#if RTXGI_DDGI_COMPUTE_IRRADIANCE
        float3 cameraDirection = normalize(worldPosHitT.xyz - cameraOrigin);
        float3 surfaceBias = DDGIGetSurfaceBias(normal, cameraDirection, DDGIVolume);

        DDGIVolumeResources resources;
        resources.probeIrradianceSRV = DDGIProbeIrradianceSRV;
        resources.probeDistanceSRV = DDGIProbeDistanceSRV;
        resources.trilinearSampler = TrilinearSampler;
#if RTXGI_DDGI_PROBE_RELOCATION
        resources.probeOffsets = DDGIProbeOffsets;
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        resources.probeStates = DDGIProbeStates;
#endif

        // Get irradiance from the DDGIVolume
        irradiance = DDGIGetVolumeIrradiance(
            worldPosHitT.xyz,
            surfaceBias,
            normal,
            DDGIVolume,
            resources);

        // Attenuate irradiance by ambient occlusion
        irradiance *= ambientOcclusion;
#endif

        // Compute color
        if (UseDDGI == 1)
        {
            result = saturate(diffuse + (baseColor.rgb / PI) * irradiance);
        }
        else
        {
            result = diffuse;
        }
    }

    if (ViewAO)
    {
        // Visualize the ambient occlusion data
        return float4(LinearToSRGB(ambientOcclusion.xxx), 1.f);
    }

    // Apply exposure
    result *= Exposure;

    // Apply tonemapping
    result = ACESFilm(result);

    // Add noise to handle SDR color banding
    float3 noise = GetLowDiscrepancyBlueNoise(int2(input.position.xy), FrameNumber, 1.f / 256.f, BlueNoiseRGB);
    result += noise;

    // Gamma correct
    result = LinearToSRGB(result);

    return float4(result, 1.f);
}
