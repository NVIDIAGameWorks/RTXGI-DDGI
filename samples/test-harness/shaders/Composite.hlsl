/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "include/Common.hlsl"
#include "include/Descriptors.hlsl"
#include "include/Random.hlsl"

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
    float3 color = float3(0.f, 0.f, 0.f);
    float  ambientOcclusion = 1.f;

    // Load the albedo and convert to linear before lighting
    RWTexture2D<float4> GBufferA = GetRWTex2D(GBUFFERA_INDEX);
    float4 albedo = GBufferA.Load(input.position.xy);

    // Store the albedo for pixels that aren't lit (e.g. visualizations)
    color = albedo.rgb;

    // Get the usage flags
    uint useFlags = GetGlobalConst(composite, useFlags);

    // Primary ray hit, need to light it
    if (albedo.a >= COMPOSITE_FLAG_LIGHT_PIXEL)
    {
        // Get the (bindless) resources
        RWTexture2D<float4> GBufferB = GetRWTex2D(GBUFFERB_INDEX);
        RWTexture2D<float4> GBufferC = GetRWTex2D(GBUFFERC_INDEX);
        RWTexture2D<float4> GBufferD = GetRWTex2D(GBUFFERD_INDEX);

        // Convert albedo back to linear
        albedo.rgb = SRGBToLinear(albedo.rgb);

        // Load world position, hit distance, and normal
        float4 worldPosHitT = GBufferB.Load(input.position.xy);
        float3 normal = GBufferC.Load(input.position.xy).xyz;

        // Load the direct lighting
        color = GBufferD.Load(input.position.xy).rgb;

        // Load indirect lighting from DDGI
        if (useFlags & COMPOSITE_FLAG_USE_DDGI)
        {
            // Add direct and indirect lighting
            RWTexture2D<float4> DDGIOutput = GetRWTex2D(DDGI_OUTPUT_INDEX);
            float3 indirect = DDGIOutput.Load(input.position.xy).rgb;
            color += indirect;
        }

        if (useFlags & COMPOSITE_FLAG_USE_RTAO)
        {
            // Load ambient occlusion and multiply with lighting
            RWTexture2D<float4> RTAOOutput = GetRWTex2D(RTAO_OUTPUT_INDEX);
            ambientOcclusion = RTAOOutput.Load(input.position.xy).x;
            color *= ambientOcclusion;
        }
    }

    // Get the show flags
    uint showFlags = GetGlobalConst(composite, showFlags);

    // Get the post processing useFlags
    uint ppUseFlags = GetGlobalConst(post, useFlags);

    if ((useFlags & COMPOSITE_FLAG_USE_RTAO) && (showFlags & COMPOSITE_FLAG_SHOW_RTAO))
    {
        // Visualize the ambient occlusion data and early out
        if (ppUseFlags & POSTPROCESS_FLAG_USE_GAMMA) return float4(ambientOcclusion.xxx, 1.f);
        return float4(LinearToSRGB(ambientOcclusion.xxx), 1.f);
    }

    if ((useFlags & COMPOSITE_FLAG_USE_DDGI) && (showFlags & COMPOSITE_FLAG_SHOW_DDGI_INDIRECT))
    {
        // Show only the indirect lighting from DDGI
        RWTexture2D<float4> DDGIOutput = GetRWTex2D(DDGI_OUTPUT_INDEX);
        float3 indirect = DDGIOutput.Load(input.position.xy).rgb;
        color = indirect;
    }

    // Early out, no post processing
    if (ppUseFlags == POSTPROCESS_FLAG_USE_NONE) return float4(color, 1.f);

    // Selectively apply exposure, tonemapping, and dither based on the GBuffer's composite flag
    if(albedo.a > COMPOSITE_FLAG_IGNORE_PIXEL)
    {
        // Exposure
        if (ppUseFlags & POSTPROCESS_FLAG_USE_EXPOSURE)
        {
            color *= GetGlobalConst(post, exposure);
        }

        // Tonemapping
        if (ppUseFlags & POSTPROCESS_FLAG_USE_TONEMAPPING)
        {
            color = ACESFilm(color);
        }

        // Dither to reduce SDR color banding
        if (ppUseFlags & POSTPROCESS_FLAG_USE_DITHER)
        {
            Texture2D<float4> BlueNoise = GetTex2D(BLUE_NOISE_INDEX);
            color += GetLowDiscrepancyBlueNoise(int2(input.position.xy), GetGlobalConst(app, frameNumber), 1.f / 256.f, BlueNoise);
        }
    }

    // Gamma correction
    if (ppUseFlags & POSTPROCESS_FLAG_USE_GAMMA)
    {
        color = LinearToSRGB(color);
    }

    return float4(color, 1.f);
}
