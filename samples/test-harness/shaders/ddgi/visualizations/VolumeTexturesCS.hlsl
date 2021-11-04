/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

// -------- CONFIGURATION DEFINES -----------------------------------------------------------------

// THGP_DIM_X must be passed in as a define at shader compilation time.
// This define specifies the number of threads in the thread group in the X dimension.
// Ex: THGP_DIM_X 8
#ifndef THGP_DIM_X
#error Required define THGP_DIM_X is not defined for VolumeTexturesCS.hlsl!
#endif

// THGP_DIM_Y must be passed in as a define at shader compilation time.
// This define specifies the number of threads in the thread group in the X dimension.
// Ex: THGP_DIM_Y 4
#ifndef THGP_DIM_Y
#error Required define THGP_DIM_Y is not defined for VolumeTexturesCS.hlsl!
#endif

// -------------------------------------------------------------------------------------------

#include "Descriptors.hlsl"
#include "../../include/Common.hlsl"

#include "../../../../../rtxgi-sdk/shaders/ddgi/include/ProbeCommon.hlsl"

// ---[ Compute Shader ]---

[numthreads(THGP_DIM_X, THGP_DIM_Y, 1)]
void CS(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    // Get the DDGIVolume index
    uint volumeIndex = GetGlobalConst(ddgivis, volumeIndex);

    // Load the DDGIVolume constants
    DDGIVolumeDescGPU DDGIVolume = UnpackDDGIVolumeDescGPU(DDGIVolumes[volumeIndex]);

    // Get probe dimensions
    float numIrradianceProbeTexels = (DDGIVolume.probeNumIrradianceTexels + 2);
    float numDistanceProbeTexels = (DDGIVolume.probeNumDistanceTexels + 2);

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    float2 numProbes = float2(DDGIVolume.probeCounts.x * DDGIVolume.probeCounts.y, DDGIVolume.probeCounts.z);
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
    float2 numProbes = float2(DDGIVolume.probeCounts.y * DDGIVolume.probeCounts.z, DDGIVolume.probeCounts.x);
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
    float2 numProbes = float2(DDGIVolume.probeCounts.x * DDGIVolume.probeCounts.y, DDGIVolume.probeCounts.z);
#endif

    float3 color = float3(0.f, 0.f, 0.f);
    float2 coords = float2(0.f, 0.f);

    // Irradiance
    float2 irradianceRect = numProbes.xy * numIrradianceProbeTexels * GetGlobalConst(ddgivis, irradianceTextureScale);
    if(DispatchThreadID.x <= irradianceRect.x && DispatchThreadID.y <= irradianceRect.y)
    {
        Texture2D<float4> ProbeIrradiance = GetDDGIVolumeIrradianceSRV(volumeIndex);

        // Sample the irradiance texture
        coords = ((float2)DispatchThreadID.xy / irradianceRect.xy);
        float3 result = ProbeIrradiance.SampleLevel(PointClampSampler, coords, 0).rgb;

        // Decode the tone curve
        float3 exponent = DDGIVolume.probeIrradianceEncodingGamma * 0.5f;
        color = pow(result, exponent);

        // Go back to linear irradiance
        color *= color;

        // Multiply by the area of the integration domain (2PI) to complete the irradiance estimate. Divide by PI to normalize for the display.
        color *= 2.f;

        // Adjust for energy loss due to reduced precision in the R10G10B10A2 irradiance texture format
        if (DDGIVolume.probeIrradianceFormat == RTXGI_DDGI_FORMAT_PROBE_IRRADIANCE_R10G10B10A2_FLOAT)
        {
            color *= 1.0989f;
        }

        // Convert to sRGB before storing
        color = LinearToSRGB(color);

        // Overwrite GBufferA's albedo and mark the pixel to not be lit
        GBufferA[DispatchThreadID.xy] = float4(color, 0.f);

        return;
    }

    // Distance
    float2 distanceRect = numProbes.xy * numDistanceProbeTexels * GetGlobalConst(ddgivis, distanceTextureScale);
    float  xmax = distanceRect.x;
    float  ymin = irradianceRect.y + 5;
    float  ymax = (ymin + distanceRect.y);
    if (DispatchThreadID.x <= xmax.x && DispatchThreadID.y > ymin && DispatchThreadID.y <= ymax)
    {
        Texture2D<float4> ProbeDistance = GetDDGIVolumeDistanceSRV(volumeIndex);

        // Sample the distance texture
        coords = float2(DispatchThreadID.x, (DispatchThreadID.y - ymin)) / distanceRect.xy;
        color.r = (ProbeDistance.SampleLevel(PointClampSampler, coords, 0).r) / GetGlobalConst(ddgivis, distanceTextureScale);

        // Normalize for display
        color.r = saturate(color.r / GetGlobalConst(ddgivis, distanceDivisor));

        // Overwrite GBufferA's albedo and mark the pixel to not be lit
        GBufferA[DispatchThreadID.xy] = float4(color.rrr, 0.f);

        return;
    }

    // Ray Data
    float2 radianceRect = float2(DDGIVolume.probeNumRays, DDGIVolume.probeCounts.x * DDGIVolume.probeCounts.y * DDGIVolume.probeCounts.z) *GetGlobalConst(ddgivis, rayDataTextureScale);
    xmax =  radianceRect.x;
    ymin += distanceRect.y + 5;
    ymax = (ymin + radianceRect.y);
    if (DispatchThreadID.x <= xmax && DispatchThreadID.y > ymin && DispatchThreadID.y <= ymax)
    {
        Texture2D<float4> RayData = GetDDGIVolumeRayDataSRV(volumeIndex);

        // Sample the ray data texture
        coords = float2(DispatchThreadID.x, (DispatchThreadID.y - ymin)) / radianceRect.xy;

        if(DDGIVolume.probeRayDataFormat == RTXGI_DDGI_FORMAT_PROBE_RAY_DATA_R32G32B32A32_FLOAT)
        {
            color = RayData.SampleLevel(PointClampSampler, coords, 0).rgb;
        }
        else if(DDGIVolume.probeRayDataFormat == RTXGI_DDGI_FORMAT_PROBE_RAY_DATA_R32G32_FLOAT)
        {
            color = RTXGIUintToFloat3(asuint(RayData.SampleLevel(PointClampSampler, coords, 0).r));
        }

        // Overwrite GBufferA's albedo and mark the pixel to not be lit
        GBufferA[DispatchThreadID.xy] = float4(color, 0.f);

        return;
    }

    // Relocation offsets
    float2 offsetRect = 0;
    ymin += radianceRect.y + 5;
    if (DDGIVolume.probeRelocationEnabled)
    {
        offsetRect = numProbes.xy * GetGlobalConst(ddgivis, relocationOffsetTextureScale);
        xmax = offsetRect.x;
        ymax = (ymin + offsetRect.y);

        if (DispatchThreadID.x <= xmax && DispatchThreadID.y > ymin && DispatchThreadID.y <= ymax)
        {
            // Get the probe's texture coordinates
            coords = float2(DispatchThreadID.x, (DispatchThreadID.y - ymin)) / offsetRect;

            // Sample the probe data texture
            Texture2D<float4> ProbeData = GetDDGIVolumeProbeDataSRV(volumeIndex);
            color = ProbeData.SampleLevel(PointClampSampler, coords, 0).rgb;

            // Overwrite GBufferA's albedo and mark the pixel to not be lit
            GBufferA[DispatchThreadID.xy] = float4(color, 0.f);

            return;
        }
    }

    // Classification States
    if(DDGIVolume.probeClassificationEnabled)
    {
        float2 statesRect = numProbes.xy * GetGlobalConst(ddgivis, classificationStateTextureScale);
        xmax = statesRect.x;

        if (DDGIVolume.probeRelocationEnabled)
        {
            ymin += offsetRect.y + 5;
        }
        else
        {
            ymin += 5;
        }

        ymax = (ymin + statesRect.y);
        if (DispatchThreadID.x <= xmax && DispatchThreadID.y > ymin && DispatchThreadID.y <= ymax)
        {
            // Set probe state colors
            color = float3(1.f, 0.f, 0.f);

            // Get the probe texture coordinates
            coords = float2(DispatchThreadID.x, (DispatchThreadID.y - ymin)) / statesRect;

            // Sample the probe data texture
            Texture2D<float4> ProbeData = GetDDGIVolumeProbeDataSRV(volumeIndex);
            if (ProbeData.SampleLevel(PointClampSampler, coords, 0).a == RTXGI_DDGI_PROBE_STATE_ACTIVE)
            {
                color = float3(0.f, 1.f, 0.f);
            }

            // Overwrite GBufferA's albedo and mark the pixel to not be lit
            GBufferA[DispatchThreadID.xy] = float4(color, 0.f);

            return;
        }
    }

}
