/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RT_LOCAL_ROOT_SIGNATURE_HLSL
#define RT_LOCAL_ROOT_SIGNATURE_HLSL

// --- [ Local Root Signature ] --------------------

cbuffer MaterialCB                 : register(b3)
{
    float3  albedo;
    float   opacity;
    float3  emissiveColor;
    float   roughness;
    float   metallic;
    int     alphaMode;
    float   alphaCutoff;
    int     doubleSided;
    int     albedoTexIdx;
    int     roughnessMetallicTexIdx;
    int     normalTexIdx;
    int     emissiveTexIdx;
};

ByteAddressBuffer Indices          : register(t3);
ByteAddressBuffer Vertices         : register(t4);
//Texture2D<float4> BlueNoiseRGB   : register(t5);   // not used, part of the global root signature
Texture2D<float4> Textures[]       : register(t6);

// ---[ Helper Functions ]-------------------------------------------------

struct VertexAttributes
{
    float3 position;
    float3 normal;
    float3 tangent;
    float3 bitangent;
    float2 uv0;
};

uint3 GetIndices(uint primitiveIndex)
{
    uint baseIndex = (primitiveIndex * 3);
    int address = (baseIndex * 4);
    return Indices.Load3(address);
}

float2 GetInterpolatedUV0(uint primitiveIndex, float3 barycentrics)
{
    // Get the triangle indices
    uint3 indices = GetIndices(primitiveIndex);

    // Interpolate the texture coordinates
    float2 uv0 = float2(0.f, 0.f);
    for (uint i = 0; i < 3; i++)
    {
        int address = (indices[i] * 12) * 4;    // 12 floats (3: pos, 3: normals, 4:tangent, 2:uv0)
        address += 40;                          // 40 bytes (10 * 4): skip position, normal, and tangent
        uv0 += asfloat(Vertices.Load2(address)) * barycentrics[i];
    }

    return uv0;
}

VertexAttributes GetInterpolatedAttributes(uint primitiveIndex, float3 barycentrics)
{
    // Get the triangle indices
    uint3 indices = GetIndices(primitiveIndex);

    // Interpolate the vertex attributes
    float direction = 0;
    VertexAttributes v = (VertexAttributes)0;
    for (uint i = 0; i < 3; i++)
    {
        int address = (indices[i] * 12) * 4;

        // Load and interpolate position
        v.position += asfloat(Vertices.Load3(address)) * barycentrics[i];
        address += 12;
        
        // Load and interpolate normal
        v.normal += asfloat(Vertices.Load3(address)) * barycentrics[i];
        address += 12;
        
        // Load and interpolate tangent
        v.tangent += asfloat(Vertices.Load3(address)) * barycentrics[i];
        address += 12;

        // Load bitangent direction
        direction = asfloat(Vertices.Load(address));
        address += 4;

        // Load and interpolate texture coordinates
        v.uv0 += asfloat(Vertices.Load2(address)) * barycentrics[i];
    }

    v.normal = normalize(v.normal);
    v.tangent = normalize(v.tangent);
    v.bitangent = normalize(cross(v.normal, v.tangent) * direction);

    return v;
}

#endif /* RT_LOCAL_ROOT_SIGNATURE_HLSL */
