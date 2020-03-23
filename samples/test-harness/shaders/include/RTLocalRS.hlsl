/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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

// Used by CHS when loading a binary scene file
cbuffer ColorRootConstants    : register(b2, space2)
{
    float3 materialColor;
    float  materialColorPad;
};

ByteAddressBuffer Indices    : register(t3);
ByteAddressBuffer Vertices    : register(t4);

// ---[ Helper Functions ]-------------------------------------------------

struct VertexAttributes
{
    float3 position;
    float3 normal;
};

uint3 GetIndices(uint primitiveIndex)
{
    uint baseIndex = (primitiveIndex * 3);
    int address = (baseIndex * 4);
    return Indices.Load3(address);
}

VertexAttributes GetVertexAttributes(uint primitiveIndex, float3 barycentrics)
{
    uint3 indices = GetIndices(primitiveIndex);
    float3 positions[3];
    float3 normals[3];

    VertexAttributes v;
    for (uint i = 0; i < 3; i++)
    {
        int address = (indices[i] * 6) * 4;
        positions[i] = asfloat(Vertices.Load3(address));
        address += 12;
        normals[i] = asfloat(Vertices.Load3(address));
    }

    v.position = positions[0] * barycentrics[0] + positions[1] * barycentrics[1] + positions[2] * barycentrics[2];
    v.normal = normals[0] * barycentrics[0] + normals[1] * barycentrics[1] + normals[2] * barycentrics[2];
    v.normal = normalize(v.normal);

    return v;
}

#endif /* RT_LOCAL_ROOT_SIGNATURE_HLSL */
