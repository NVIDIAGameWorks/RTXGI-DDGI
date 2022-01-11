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
#error Required define RTXGI_DDGI_RESOURCE_MANAGEMENT is not defined for ProbeBorderUpdateCSCS.hlsl!
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
#error Required define RTXGI_DDGI_SHADER_REFLECTION is not defined for ProbeBorderUpdateCS.hlsl!
#else

#if !RTXGI_DDGI_SHADER_REFLECTION

// REGISTERs AND SPACEs (SHADER REFLECTION DISABLED)

#if !SPIRV

// CONSTS_REGISTER and CONSTS_SPACE must be passed in as defines at shader compilation time *when not using reflection*.
// These defines specify the shader register and space used for root / push DDGI constants.
// Ex: CONSTS_REGISTER b0
// Ex: CONSTS_SPACE space1

#ifndef CONSTS_REGISTER
#error Required define CONSTS_REGISTER is not defined for ProbeBorderUpdateCS.hlsl!
#endif

#ifndef CONSTS_SPACE
#error Required define CONSTS_SPACE is not defined for ProbeBorderUpdateCS.hlsl!
#endif

#endif

#endif // !RTXGI_DDGI_SHADER_REFLECTION

#endif // #ifndef RTXGI_DDGI_SHADER_REFLECTION

// -------- RESOURCE BINDING DEFINES --------------------------------------------------------------

// RTXGI_DDGI_BINDLESS_RESOURCES must be passed in as a define at shader compilation time.
// This define specifies whether resources will be accessed bindlessly or not.
// Ex: RTXGI_DDGI_BINDLESS_RESOURCES [0|1]

#ifndef RTXGI_DDGI_BINDLESS_RESOURCES
#error Required define RTXGI_DDGI_BINDLESS_RESOURCES is not defined for ProbeBorderUpdateCS.hlsl!
#else

#if !RTXGI_DDGI_SHADER_REFLECTION

#if RTXGI_DDGI_BINDLESS_RESOURCES

// BINDLESS RESOURCE DEFINES (SHADER REFLECTION DISABLED)

// RWTEX2D_REGISTER and RWTEX2D_SPACE must be passed in as defines at shader compilation time *when not using reflection*.
// These defines specify the shader register and space used for the DDGIVolume constants structured buffer.
// Ex: RWTEX2D_REGISTER t5
// Ex: RWTEX2D_SPACE space0

#ifndef RWTEX2D_REGISTER
#error Required bindless mode define RWTEX2D_REGISTER is not defined for ProbeBorderUpdateCS.hlsl!
#endif

#ifndef RWTEX2D_SPACE
#error Required bindless mode define RWTEX2D_SPACE is not defined for ProbeBorderUpdateCS.hlsl!
#endif

// RTXGI_DDGI_BLEND_RADIANCE must be passed in as a define at shader compilation time *when using bindless*.
// This define specifies whether the shader blends radiance or distance values.
// Ex: RTXGI_DDGI_BLEND_RADIANCE [0|1]
#ifndef RTXGI_DDGI_BLEND_RADIANCE
#error Required define RTXGI_DDGI_BLEND_RADIANCE is not defined for ProbeBorderUpdateCS.hlsl!
#endif

#else

// BOUND RESOURCE DEFINES (SHADER REFLECTION DISABLED)

// OUTPUT_REGISTER and OUTPUT_SPACE must be passed in as defines at shader compilation time *when not using reflection*.
// These defines specify the shader register and space used for the DDGIVolume irradiance or distance texture.
// Ex: OUTPUT_REGISTER u1
// Ex: OUTPUT_SPACE space1

#ifndef OUTPUT_REGISTER
#error Required define OUTPUT_REGISTER is not defined for ProbeBorderUpdateCS.hlsl!
#endif

#ifndef OUTPUT_SPACE
#error Required define OUTPUT_SPACE is not defined for ProbeBorderUpdateCS.hlsl!
#endif

#endif // RTXGI_DDGI_BINDLESS_RESOURCES

#endif // !RTXGI_DDGI_SHADER_REFLECTION

#endif // #ifndef RTXGI_DDGI_BINDLESS_RESOURCES

// -------- CONFIGURATION DEFINES -----------------------------------------------------------------

// RTXGI_DDGI_PROBE_NUM_TEXELS must be passed in as a define at shader compilation time.
// This define specifies the number of texels of a single dimension of a probe.
// Ex: RTXGI_DDGI_PROBE_NUM_TEXELS 6  => irradiance data is 6x6 texels for a single probe
// Ex: RTXGI_DDGI_PROBE_NUM_TEXELS 14 => distance data is 14x14 texels for a single probe
#ifndef RTXGI_DDGI_PROBE_NUM_TEXELS
#error Required define RTXGI_DDGI_PROBE_NUM_TEXELS is not defined for ProbeBorderUpdateCS.hlsl!
#endif

// -------- OPTIONAL DEFINES -----------------------------------------------------------------

// Define RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING before compiling SDK HLSL shaders to toggle
// a visualization mode that outputs the coordinates of the texel to be copied to the border texel. Useful when debugging.
// 0: Disabled (default).
// 1: Enabled.
#ifndef RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING
#pragma message "Optional define RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING is not defined, defaulting to 0." 
#define RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING 0
#endif

// ------------------------------------------------------------------------------------------------

#include "include/Common.hlsl"

#if RTXGI_DDGI_SHADER_REFLECTION || SPIRV
#define CONSTS_REG_DECL 
#define VOLUME_CONSTS_REG_DECL 
#if RTXGI_DDGI_BINDLESS_RESOURCES
    #define RWTEX2D_REG_DECL 
#else
    #define OUTPUT_REG_DECL 
#endif

#else

#define CONSTS_REG_DECL : register(CONSTS_REGISTER, CONSTS_SPACE)
#define VOLUME_CONSTS_REG_DECL : register(VOLUME_CONSTS_REGISTER, VOLUME_CONSTS_SPACE)
#if RTXGI_DDGI_BINDLESS_RESOURCES
    #define RWTEX2D_REG_DECL : register(RWTEX2D_REGISTER, RWTEX2D_SPACE)
#else
    #define OUTPUT_REG_DECL : register(OUTPUT_REGISTER, OUTPUT_SPACE)
#endif // RTXGI_DDGI_BINDLESS_RESOURCES

#endif // RTXGI_DDGI_SHADER_REFLECTION || SPIRV

// Root / Push Constants
#if !SPIRV

// D3D12 can have multiple root constants across different root parameter slots, so this constant
// buffer can be declared here for both bindless and bound resource access methods
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

#if RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING
// DDGIVolume constants structured buffer
RTXGI_VK_BINDING(VOLUME_CONSTS_REGISTER, VOLUME_CONSTS_SPACE)
StructuredBuffer<DDGIVolumeDescGPUPacked> DDGIVolumes VOLUME_CONSTS_REG_DECL;
#endif

#if RTXGI_DDGI_BINDLESS_RESOURCES
// DDGIVolume ray data, probe irradiance / distance, probe data
RTXGI_VK_BINDING(RWTEX2D_REGISTER, RWTEX2D_SPACE)
RWTexture2D<float4> RWTex2D[] RWTEX2D_REG_DECL;
#else
// DDGIVolume probe irradiance or filtered distance
RTXGI_VK_BINDING(OUTPUT_REGISTER, OUTPUT_SPACE)
RWTexture2D<float4> Output OUTPUT_REG_DECL;
#endif

[numthreads(8, 8, 1)]
void DDGIProbeBorderRowUpdateCS(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    uint probeSideLength = (RTXGI_DDGI_PROBE_NUM_TEXELS + 2);
    uint probeSideLengthMinusOne = (probeSideLength - 1);

    // Map thread index to border row texel coordinates
    uint2 threadCoordinates = DispatchThreadID.xy;
    threadCoordinates.y *= probeSideLength;

    // Early out: ignore the corner texels
    int mod = (DispatchThreadID.x % probeSideLength);
    if (mod == 0 || mod == probeSideLengthMinusOne) return;

    // Get the volume index and uav offset
    uint volumeIndex = GetVolumeIndex();

#if RTXGI_DDGI_BINDLESS_RESOURCES
    uint uavOffset = GetUAVOffset();

    // Get the volume's irradiance and distance texture UAV
    #if RTXGI_DDGI_BLEND_RADIANCE
        RWTexture2D<float4> Output = RWTex2D[uavOffset + (volumeIndex * 4) + 1];
    #else
        RWTexture2D<float4> Output = RWTex2D[uavOffset + (volumeIndex * 4) + 2];
    #endif
#endif

    // Compute the interior texel coordinates to copy (top row)
    uint probeStart = uint(threadCoordinates.x / probeSideLength) * probeSideLength;
    uint offset = probeSideLengthMinusOne - (threadCoordinates.x % probeSideLength);

    uint2 copyCoordinates = uint2(probeStart + offset, (threadCoordinates.y + 1));

    // Visualize border copy indexing and early out
#if RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING
    // Get the volume's constants
    DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumes[volumeIndex]);

    // Write out colored indexing
    if(volume.probeIrradianceFormat == RTXGI_DDGI_FORMAT_PROBE_IRRADIANCE_R32G32B32A32_FLOAT)
    {
        Output[threadCoordinates] = float4(threadCoordinates, copyCoordinates);
        threadCoordinates.y += probeSideLengthMinusOne;
        copyCoordinates = uint2(probeStart + offset, threadCoordinates.y - 1);
        Output[threadCoordinates] = float4(threadCoordinates, copyCoordinates);
        return;
    }
#endif

    // Top row
    Output[threadCoordinates] = Output[copyCoordinates];

    // Compute the interior texel coordinate to copy (bottom row)
    threadCoordinates.y += probeSideLengthMinusOne;
    copyCoordinates = uint2(probeStart + offset, threadCoordinates.y - 1);

    // Bottom row
    Output[threadCoordinates] = Output[copyCoordinates];
}

[numthreads(8, 8, 1)]
void DDGIProbeBorderColumnUpdateCS(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    uint probeSideLength = (RTXGI_DDGI_PROBE_NUM_TEXELS + 2);
    uint probeSideLengthMinusOne = (probeSideLength - 1);

    // Map thread index to border row texel coordinates
    uint2 threadCoordinates = DispatchThreadID.xy;
    threadCoordinates.x *= probeSideLength;

    uint2 copyCoordinates = uint2(0, 0);

    // Get the volume index and uav offset
    uint volumeIndex = GetVolumeIndex();

#if RTXGI_DDGI_BINDLESS_RESOURCES
    uint uavOffset = GetUAVOffset();

    // Get the volume's irradiance and distance texture UAV
    #if RTXGI_DDGI_BLEND_RADIANCE
        RWTexture2D<float4> Output = RWTex2D[uavOffset + (volumeIndex * 4) + 1];
    #else
        RWTexture2D<float4> Output = RWTex2D[uavOffset + (volumeIndex * 4) + 2];
    #endif
#endif

#if RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING
    // Get the volume's constants
    DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumes[volumeIndex]);
#endif

    // Handle the corner texels
    int mod = (threadCoordinates.y % probeSideLength);
    if (mod == 0 || mod == probeSideLengthMinusOne)
    {
        // Left corner
        copyCoordinates.x = threadCoordinates.x + RTXGI_DDGI_PROBE_NUM_TEXELS;
        copyCoordinates.y = threadCoordinates.y - sign(mod - 1) * RTXGI_DDGI_PROBE_NUM_TEXELS;

        Output[threadCoordinates] = Output[copyCoordinates];

    #if RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING
        if(volume.probeIrradianceFormat == RTXGI_DDGI_FORMAT_PROBE_IRRADIANCE_R32G32B32A32_FLOAT)
        {
            Output[threadCoordinates] = float4(threadCoordinates, copyCoordinates);
        }
    #endif

        // Right corner
        threadCoordinates.x += probeSideLengthMinusOne;
        copyCoordinates.x = threadCoordinates.x - RTXGI_DDGI_PROBE_NUM_TEXELS;

        Output[threadCoordinates] = Output[copyCoordinates];

    #if RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING
        if(volume.probeIrradianceFormat == RTXGI_DDGI_FORMAT_PROBE_IRRADIANCE_R32G32B32A32_FLOAT)
        {
            Output[threadCoordinates] = float4(threadCoordinates, copyCoordinates);
        }
    #endif

        return;
    }

    // Compute the interior texel coordinates to copy (left column)
    uint probeStart = uint(threadCoordinates.y / probeSideLength) * probeSideLength;
    uint offset = probeSideLengthMinusOne - (threadCoordinates.y % probeSideLength);

    copyCoordinates = uint2(threadCoordinates.x + 1, probeStart + offset);

    // Visualize border copy indexing and early out
#if RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING
    if(volume.probeIrradianceFormat == RTXGI_DDGI_FORMAT_PROBE_IRRADIANCE_R32G32B32A32_FLOAT)
    {
        Output[threadCoordinates] = float4(threadCoordinates, copyCoordinates);
        threadCoordinates.x += probeSideLengthMinusOne;
        copyCoordinates = uint2(threadCoordinates.x - 1, probeStart + offset);
        Output[threadCoordinates] = float4(threadCoordinates, copyCoordinates);
        return;
    }
#endif

    // Left column
    Output[threadCoordinates] = Output[copyCoordinates];

    // Compute the interior texel coordinate to copy (right column)
    threadCoordinates.x += probeSideLengthMinusOne;
    copyCoordinates = uint2(threadCoordinates.x - 1, probeStart + offset);

    // Right column
    Output[threadCoordinates] = Output[copyCoordinates];
}
