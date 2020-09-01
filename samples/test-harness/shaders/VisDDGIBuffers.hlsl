/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "../../../rtxgi-sdk/shaders/Common.hlsl"
#include "../../../rtxgi-sdk/include/rtxgi/Defines.h"
#include "../../../rtxgi-sdk/include/rtxgi/ddgi/DDGIVolumeDefines.h"

#include "include/Common.hlsl"
#include "include/RasterRS.hlsl"

// ---[ Structures ]---

struct PSInput
{
    float4 position : SV_POSITION;
    float2 tex0     : TEXCOORD;
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
    DDGIVolumeDescGPU DDGIVolume = DDGIVolumes.volumes[volumeSelect];
    float numIrradianceProbeTexels = (DDGIVolume.probeNumIrradianceTexels + 2);
    float numDistanceProbeTexels = (DDGIVolume.probeNumDistanceTexels + 2);

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    float2 numProbes = float2(DDGIVolume.probeGridCounts.x * DDGIVolume.probeGridCounts.y, DDGIVolume.probeGridCounts.z);
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
    float2 numProbes = float2(DDGIVolume.probeGridCounts.y * DDGIVolume.probeGridCounts.z, DDGIVolume.probeGridCounts.x);
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
    float2 numProbes = float2(DDGIVolume.probeGridCounts.x * DDGIVolume.probeGridCounts.y, DDGIVolume.probeGridCounts.z);
#endif

    float2 coords;
    Texture2D<float4> DDGIProbeIrradianceSRV = GetDDGIProbeIrradianceSRV(volumeSelect);
    Texture2D<float4> DDGIProbeDistanceSRV = GetDDGIProbeDistanceSRV(volumeSelect);
    RWTexture2D<float4> DDGIProbeRTRadiance = GetDDGIProbeRTRadianceUAV(volumeSelect);

    // Irradiance
    float2 irradianceRect = numProbes.xy * numIrradianceProbeTexels * VizIrradianceScale;
    if(input.position.x <= irradianceRect.x && input.position.y <= irradianceRect.y)
    {
        coords = (input.position.xy / irradianceRect.xy);
        float3 result = DDGIProbeIrradianceSRV.SampleLevel(PointSampler, coords, 0).rgb;

        // Decode the tone curve
        float3 exponent = DDGIVolume.probeIrradianceEncodingGamma * 0.5f;
        result = pow(result, exponent);

        // Gamma correct
        result = LinearToSRGB(result);

        return float4(result, 1.f);
    }

    // Filtered Distance
    float2 distanceRect = numProbes.xy * numDistanceProbeTexels * VizDistanceScale;
    if (input.position.x <= distanceRect.x && (input.position.y > irradianceRect.y) &&
        input.position.y <= (irradianceRect.y + distanceRect.y))
    {
        coords = float2(input.position.x, (input.position.y - irradianceRect.y)) / distanceRect.xy;
        return float4(DDGIProbeDistanceSRV.SampleLevel(PointSampler, coords, 0).rg / VizDistanceDivisor, 0.f, 1.f);
    }

    // RT Radiance
    float2 radianceRect = float2(DDGIVolume.numRaysPerProbe, DDGIVolume.probeGridCounts.x * DDGIVolume.probeGridCounts.y * DDGIVolume.probeGridCounts.z) * VizRadianceScale;
    if (input.position.x <= radianceRect.x && (input.position.y > irradianceRect.y + distanceRect.y) &&
        input.position.y <= (irradianceRect.y + distanceRect.y + radianceRect.y))
    {
        coords = float2(input.position.x, (input.position.y - irradianceRect.y - distanceRect.y)) / VizRadianceScale;

#if RTXGI_DDGI_DEBUG_FORMAT_RADIANCE
        return float4(DDGIProbeRTRadiance.Load(coords).rgb, 1.f);
#else
        return float4(RTXGIUintToFloat3(asuint(DDGIProbeRTRadiance.Load(coords).r)), 1.f);
#endif
    }

#if RTXGI_DDGI_PROBE_RELOCATION
    // Offsets
    float2 offsetRect = numProbes.xy * VizOffsetScale;
    if (input.position.x <= offsetRect.x && (input.position.y > irradianceRect.y + distanceRect.y + radianceRect.y) &&
        input.position.y <= (irradianceRect.y + distanceRect.y + radianceRect.y + offsetRect.y))
    {
        RWTexture2D<float4> DDGIProbeOffsets = GetDDGIProbeOffsetsUAV(volumeSelect);
        coords = float2(input.position.x, (input.position.y - irradianceRect.y - distanceRect.y - radianceRect.y)) / VizOffsetScale;
        return float4(DDGIProbeOffsets.Load(coords).rgb, 1.f);
    }
#endif

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
    // States
    // Add one for one pixel separation between the two textures to more clearly differentiate them
    RWTexture2D<uint> DDGIProbeStates = GetDDGIProbeStatesUAV(volumeSelect);
    float2 stateRect = numProbes.xy * VizStateScale;
#if RTXGI_DDGI_PROBE_RELOCATION
    if (input.position.x <= stateRect.x && (input.position.y > irradianceRect.y + distanceRect.y + radianceRect.y + offsetRect.y + 1) &&
        input.position.y <= (irradianceRect.y + distanceRect.y + radianceRect.y + offsetRect.y + stateRect.y + 1))
    {
        coords = float2(input.position.x, (input.position.y - irradianceRect.y - distanceRect.y - radianceRect.y - offsetRect.y - 1)) / VizStateScale;
        return float4(float(DDGIProbeStates.Load(coords).r), 0.f, 0.f, 1.f);
    }
#else // !RTXGI_DDGI_PROBE_RELOCATION
    if (input.position.x <= stateRect.x && (input.position.y > irradianceRect.y + distanceRect.y + radianceRect.y + 1) &&
        input.position.y <= (irradianceRect.y + distanceRect.y + radianceRect.y + stateRect.y + 1))
    {
        coords = float2(input.position.x, (input.position.y - irradianceRect.y - distanceRect.y - radianceRect.y - 1)) / VizStateScale;
        return float4(float(DDGIProbeStates.Load(coords).r), 0.f, 0.f, 1.f);
    }
#endif // RTXGI_DDGI_PROBE_RELOCATION
#endif // RTXGI_DDGI_PROBE_STATE_CLASSIFIER

    discard;
    return float4(0.f, 0.f, 0.f, 0.f);
}
