/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "include/RTCommon.hlsl"
#include "include/RTGlobalRS.hlsl"
#include "include/RTLocalRS.hlsl"

void WritePayload(float3 color, float3 worldPosition, float3 normal, float hitT, uint hitKind, uint instanceIndex, inout PayloadData payload)
{
    payload.baseColor = color;
    payload.worldPosition = worldPosition;
    payload.normal = normal;
    payload.hitT = hitT;
    payload.hitKind = hitKind;
    payload.instanceIndex = instanceIndex;
}

 // ---[ Closest Hit Shaders ]---

[shader("closesthit")]
void ClosestHit(inout PayloadData payload, BuiltInTriangleIntersectionAttributes attrib)
{
    uint primitiveIndex = PrimitiveIndex();
    float3 barycentrics = float3((1.0f - attrib.barycentrics.x - attrib.barycentrics.y), attrib.barycentrics.x, attrib.barycentrics.y);
    VertexAttributes vertex = GetVertexAttributes(primitiveIndex, barycentrics);

    WritePayload(
        materialColor,
        mul(ObjectToWorld3x4(), float4(vertex.position, 1)).xyz,
        vertex.normal,
        RayTCurrent(),
        HitKind(),
        InstanceIndex(),
        payload);
}

[shader("closesthit")]
void ClosestHitManual(inout PayloadData payload, BuiltInTriangleIntersectionAttributes attrib)
{
    uint primitiveIndex = PrimitiveIndex();
    uint materialIndex = primitiveIndex / 2;

    float3 barycentrics = float3((1.0f - attrib.barycentrics.x - attrib.barycentrics.y), attrib.barycentrics.x, attrib.barycentrics.y);
    VertexAttributes vertex = GetVertexAttributes(primitiveIndex, barycentrics);

    WritePayload(
        materialColors[materialIndex].rgb,
        mul(ObjectToWorld3x4(), float4(vertex.position, 1)).xyz,
        vertex.normal,
        RayTCurrent(),
        HitKind(),
        InstanceIndex(),
        payload);
}
