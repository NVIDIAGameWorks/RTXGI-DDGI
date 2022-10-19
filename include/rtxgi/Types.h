/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

#include <cstddef>
#include <stdint.h>

namespace rtxgi
{

    typedef uint32_t uint;

    struct uint2
    {
        unsigned int x;
        unsigned int y;

        unsigned int& operator[](std::size_t idx)
        {
            return *(reinterpret_cast<unsigned int*>(this) + idx);
        }

        const unsigned int& operator[](std::size_t idx) const
        {
            return *(reinterpret_cast<const unsigned int*>(this) + idx);
        }
    };

    struct uint3
    {
        unsigned int x;
        unsigned int y;
        unsigned int z;

        unsigned int& operator[](std::size_t idx)
        {
            return *(reinterpret_cast<unsigned int*>(this) + idx);
        }

        const unsigned int& operator[](std::size_t idx) const
        {
            return *(reinterpret_cast<const unsigned int*>(this) + idx);
        }
    };

    struct uint4
    {
        unsigned int x;
        unsigned int y;
        unsigned int z;
        unsigned int w;

        unsigned int& operator[](std::size_t idx)
        {
            return *(reinterpret_cast<unsigned int*>(this) + idx);
        }

        const unsigned int& operator[](std::size_t idx) const
        {
            return *(reinterpret_cast<const unsigned int*>(this) + idx);
        }
    };

    struct int2
    {
        int x;
        int y;

        int& operator[](std::size_t idx)
        {
            return *(reinterpret_cast<int*>(this) + idx);
        }

        const int& operator[](std::size_t idx) const
        {
            return *(reinterpret_cast<const int*>(this) + idx);
        }
    };

    struct int3
    {
        int x;
        int y;
        int z;

        int& operator[](std::size_t idx)
        {
            return *(reinterpret_cast<int*>(this) + idx);
        }

        const int& operator[](std::size_t idx) const
        {
            return *(reinterpret_cast<const int*>(this) + idx);
        }
    };

    struct int4
    {
        int x;
        int y;
        int z;
        int w;

        int& operator[](std::size_t idx)
        {
            return *(reinterpret_cast<int*>(this) + idx);
        }

        const int& operator[](std::size_t idx) const
        {
            return *(reinterpret_cast<const int*>(this) + idx);
        }
    };

    struct float2
    {
        float x;
        float y;

        float& operator[](std::size_t idx)
        {
            return *(reinterpret_cast<float*>(this) + idx);
        }

        const float& operator[](std::size_t idx) const
        {
            return *(reinterpret_cast<const float*>(this) + idx);
        }
    };

    struct float3
    {
        float x;
        float y;
        float z;

        float& operator[](std::size_t idx)
        {
            return *(reinterpret_cast<float*>(this) + idx);
        }

        const float& operator[](std::size_t idx) const
        {
            return *(reinterpret_cast<const float*>(this) + idx);
        }
    };

    struct float4
    {
        float x;
        float y;
        float z;
        float w;

        float& operator[](std::size_t idx)
        {
            return *(reinterpret_cast<float*>(this) + idx);
        }

        const float& operator[](std::size_t idx) const
        {
            return *(reinterpret_cast<const float*>(this) + idx);
        }
    };

    struct float3x3
    {
        float3 r0;
        float3 r1;
        float3 r2;

        float3& operator[](std::size_t idx)
        {
            return *(reinterpret_cast<float3*>(this) + idx);
        }

        const float3& operator[](std::size_t idx) const
        {
            return *(reinterpret_cast<const float3*>(this) + idx);
        }

        float3x3 operator*(const float3x3 in) const
        {
            return float3x3 {
                float3{ (r0.x * in.r0.x) + (r0.y * in.r1.x) + (r0.z * in.r2.x), (r0.x * in.r0.y) + (r0.y * in.r1.y) + (r0.z * in.r2.y), (r0.x * in.r0.z) + (r0.y * in.r1.z) + (r0.z * in.r2.z) },
                float3{ (r1.x * in.r0.x) + (r1.y * in.r1.x) + (r1.z * in.r2.x), (r1.x * in.r0.y) + (r1.y * in.r1.y) + (r1.z * in.r2.y), (r1.x * in.r0.z) + (r1.y * in.r1.z) + (r1.z * in.r2.z) },
                float3{ (r2.x * in.r0.x) + (r2.y * in.r1.x) + (r2.z * in.r2.x), (r2.x * in.r0.y) + (r2.y * in.r1.y) + (r2.z * in.r2.y), (r2.x * in.r0.z) + (r2.y * in.r1.z) + (r2.z * in.r2.z) },
            };
        }
    };

    struct float4x4
    {
        float4 r0;
        float4 r1;
        float4 r2;
        float4 r3;

        float4& operator[](std::size_t idx)
        {
            return *(reinterpret_cast<float4*>(this) + idx);
        }

        const float4& operator[](std::size_t idx) const
        {
            return *(reinterpret_cast<const float4*>(this) + idx);
        }
    };

    struct AABB
    {
        float3 min;
        float3 max;
    };

    struct OBB
    {
        float3 origin;
        float4 rotation;    // Rotation quaternion with .xyz vector part and .w scalar part
        float3 e;           // Positive halfwidth extends
    };
}
