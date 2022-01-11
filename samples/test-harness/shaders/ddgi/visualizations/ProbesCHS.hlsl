/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "Descriptors.hlsl"
#include "../../include/RayTracing.hlsl"

[shader("closesthit")]
void CHS(inout ProbesPayload payload, BuiltInTriangleIntersectionAttributes attrib)
{
    payload.hitT = RayTCurrent();
    payload.instanceIndex = (int)InstanceIndex();
    payload.volumeIndex = (InstanceID() >> 16);
    payload.instanceOffset = (InstanceID() & 0xFFFF);
    payload.worldPosition = WorldRayOrigin() + (WorldRayDirection() * payload.hitT);
}
