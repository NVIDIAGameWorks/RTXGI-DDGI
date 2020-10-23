/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RT_COMMON_HLSL
#define RT_COMMON_HLSL

struct Payload
{                                         // Byte Offset
    float3  albedo;                       // 12
    float   opacity;                      // 16
    float3  worldPosition;                // 28
    float   metallic;                     // 32
    float3  normal;                       // 44
    float3  shadingNormal;                // 56
    float   roughness;                    // 60
    float   hitT;                         // 64
    uint    hitKind;                      // 78
    uint    instanceIndex;                // 72
};

struct PackedPayload
{                                         // Byte Offset     Format
    uint3 albedoAndNormal;                //                 X: 16: Albedo.r  16: Albedo.g
                                          //                 Y: 16: Albedo.b  16: Normal.x
                                          //                 Z: 16: Normal.y     16: Normal.z
                                          // 12
    uint  metallicAndRoughness;           // 16                 16: Metallic        16: Roughness
    uint3 worldPosAndShadingNormal;       //                 X: 16: WorldPos.x      16: WorldPos.y
                                          //                 Y: 16: WorldPos.z      16: ShadingNormal.x
                                          // 28              Z: 16: ShadingNormal.y 16: ShadingNormal.z
    uint  opacityAndHitKind;              // 32              X: 16: Opacity         16: Hit Kind
    uint  instanceIndex;                  // 36                 32: InstanceIndex
    float hitT;                           // 40                 32: HitT
};

/**
* Pack the payload into a compressed format.
* Complement of UnpackPayload().
*/
PackedPayload PackPayload(Payload input)
{
    PackedPayload output = (PackedPayload)0;
    output.albedoAndNormal.x  =  f32tof16(input.albedo.r);
    output.albedoAndNormal.x |= f32tof16(input.albedo.g) << 16;
    output.albedoAndNormal.y  = f32tof16(input.albedo.b);
    output.albedoAndNormal.y |= f32tof16(input.normal.x) << 16;
    output.albedoAndNormal.z  = f32tof16(input.normal.y);
    output.albedoAndNormal.z |= f32tof16(input.normal.z) << 16;
    output.metallicAndRoughness  = f32tof16(input.metallic);
    output.metallicAndRoughness |= f32tof16(input.roughness) << 16;
    output.worldPosAndShadingNormal.x  = f32tof16(input.worldPosition.x);
    output.worldPosAndShadingNormal.x |= f32tof16(input.worldPosition.y) << 16;
    output.worldPosAndShadingNormal.y  = f32tof16(input.worldPosition.z);
    output.worldPosAndShadingNormal.y |= f32tof16(input.shadingNormal.x) << 16;
    output.worldPosAndShadingNormal.z  = f32tof16(input.shadingNormal.y);
    output.worldPosAndShadingNormal.z |= f32tof16(input.shadingNormal.z) << 16;
    output.opacityAndHitKind  = f32tof16(input.opacity);
    output.opacityAndHitKind |= input.hitKind << 16;
    output.instanceIndex = input.instanceIndex;
    output.hitT = input.hitT;
    return output;
}


/**
* Unpack the compressed payload into the full sized payload format.
* Complement of PackPayload().
*/
Payload UnpackPayload(PackedPayload input)
{
    Payload output = (Payload)0;
    output.albedo.x = f16tof32(input.albedoAndNormal.x);
    output.albedo.y = f16tof32(input.albedoAndNormal.x >> 16);
    output.albedo.z = f16tof32(input.albedoAndNormal.y);
    output.normal.x = f16tof32(input.albedoAndNormal.y >> 16);
    output.normal.y = f16tof32(input.albedoAndNormal.z);
    output.normal.z = f16tof32(input.albedoAndNormal.z >> 16);
    output.metallic = f16tof32(input.metallicAndRoughness);
    output.roughness = f16tof32(input.metallicAndRoughness >> 16);
    output.worldPosition.x = f16tof32(input.worldPosAndShadingNormal.x);
    output.worldPosition.y = f16tof32(input.worldPosAndShadingNormal.x >> 16);
    output.worldPosition.z = f16tof32(input.worldPosAndShadingNormal.y);
    output.shadingNormal.x = f16tof32(input.worldPosAndShadingNormal.y >> 16);
    output.shadingNormal.y = f16tof32(input.worldPosAndShadingNormal.z);
    output.shadingNormal.z = f16tof32(input.worldPosAndShadingNormal.z >> 16);
    output.opacity = f16tof32(input.opacityAndHitKind);
    output.hitKind = input.opacityAndHitKind >> 16;
    output.instanceIndex = input.instanceIndex;
    output.hitT = input.hitT;
    return output;
}

#endif /* RT_COMMON_HLSL */
