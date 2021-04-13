/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

#include <time.h>
#include <stdlib.h>
#include <cmath>

#include "rtxgi/Defines.h"
#include "rtxgi/Math.h"

namespace rtxgi
{
    int AbsFloor(float f)
    {
        return f >= 0.f ? int(floor(f)) : int(ceil(f));
    }

    float3 Normalize(const float3& v)
    {
        float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
        return (v / length);
    }

    float3x3 EulerAnglesToRotationMatrixYXZ(const float3& eulerAngles)
    {
        float sx = std::sin(eulerAngles.x);
        float cx = std::cos(eulerAngles.x);
        float sy = std::sin(eulerAngles.y);
        float cy = std::cos(eulerAngles.y);
        float sz = std::sin(eulerAngles.z);
        float cz = std::cos(eulerAngles.z);

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
        float3x3 rotYXZ = {
            { cy * cz + sx * sy * sz, cz * sx * sy - cy * sz, cx * sy },
            { cx * sz,                cx * cz,               -sx      },
            {-cz * sy + cy * sx * sz, cy * cz * sx + sy * sz, cx * cy }
        };
#else // Swap signs of all sin()
        float3x3 rotYXZ = {
            { cy * cz - sx * sy * sz, cz * sx * sy + cy * sz,-cx * sy },
            {-cx * sz,                cx * cz,                sx      },
            { cz * sy + cy * sx * sz,-cy * cz * sx + sy * sz, cx * cy }
        };
#endif

        return rotYXZ;
    }

    float4 QuaternionConjugate(const float4& q)
    {
        return { -q.x, -q.y, -q.z, q.w };
    }

    float4 RotationMatrixToQuaternion(const float3x3& m)
    {
        float4 q = { 0.f, 0.f, 0.f, 0.f };

        float m00 = m.r0.x, m01 = m.r0.y, m02 = m.r0.z;
        float m10 = m.r1.x, m11 = m.r1.y, m12 = m.r1.z;
        float m20 = m.r2.x, m21 = m.r2.y, m22 = m.r2.z;
        float diagSum = m00 + m11 + m22;

        if (diagSum > 0.f)
        {
            q.w = std::sqrt(diagSum + 1.f) * 0.5f;
            float f = 0.25f / q.w;
            q.x = (m21 - m12) * f;
            q.y = (m02 - m20) * f;
            q.z = (m10 - m01) * f;
        }
        else if ((m00 > m11) && (m00 > m22))
        {
            q.x = std::sqrt(m00 - m11 - m22 + 1.f) * 0.5f;
            float f = 0.25f / q.x;
            q.y = (m10 + m01) * f;
            q.z = (m02 + m20) * f;
            q.w = (m21 - m12) * f;
        }
        else if (m11 > m22)
        {
            q.y = std::sqrt(m11 - m00 - m22 + 1.f) * 0.5f;
            float f = 0.25f / q.y;
            q.x = (m10 + m01) * f;
            q.z = (m21 + m12) * f;
            q.w = (m02 - m20) * f;
        }
        else
        {
            q.z = std::sqrt(m22 - m00 - m11 + 1.f) * 0.5f;
            float f = 0.25f / q.z;
            q.x = (m02 + m20) * f;
            q.y = (m21 + m12) * f;
            q.w = (m10 - m01) * f;
        }

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
        // By default, a quaternion rotation through a positive angle is counterclockwise when the axis points toward the viewer.
        // It needs to be reversed (by conjugate), in case of left-hand coordinate system.
        q = QuaternionConjugate(q);
#endif

        return q;
    }

    //------------------------------------------------------------------------
    // Addition
    //------------------------------------------------------------------------

    int2 operator+(const int2 &lhs, const int2 &rhs) { return { lhs.x + rhs.x, lhs.y + rhs.y }; }
    int2 operator+(const int2 &lhs, const float2 &rhs) { return { lhs.x + (int)rhs.x, lhs.y + (int)rhs.y }; }
    int2 operator+(const int2 &lhs, const int &rhs) { return { lhs.x + rhs, lhs.y + rhs }; }
    int2 operator+(const int2 &lhs, const float &rhs) { return { lhs.x + (int)rhs, lhs.y + (int)rhs }; }

    int3 operator+(const int3 &lhs, const int3 &rhs) { return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z }; }
    int3 operator+(const int3 &lhs, const float3 &rhs) { return { lhs.x + (int)rhs.x, lhs.y + (int)rhs.y, lhs.z + (int)rhs.z }; }
    int3 operator+(const int3 &lhs, const int &rhs) { return { lhs.x + rhs, lhs.y + rhs, lhs.z + rhs }; }
    int3 operator+(const int3 &lhs, const float &rhs) { return { lhs.x + (int)rhs, lhs.y + (int)rhs, lhs.z + (int)rhs }; }

    void operator+=(int2 &lhs, const int2 &rhs) { lhs.x += rhs.x; lhs.y += rhs.y; }
    void operator+=(int3 &lhs, const int3 &rhs) { lhs.x += rhs.x; lhs.y += rhs.y; lhs.z += rhs.z; }
    void operator+=(int4 &lhs, const int4 &rhs) { lhs.x += rhs.x; lhs.y += rhs.y; lhs.z += rhs.z; lhs.w += rhs.w; }

    float2 operator+(const float2 &lhs, const float2 &rhs) { return { lhs.x + rhs.x, lhs.y + rhs.y }; }
    float2 operator+(const float2 &lhs, const int2 &rhs) { return { lhs.x + rhs.x, lhs.y + rhs.y }; }
    float2 operator+(const float2 &lhs, const float &rhs) { return { lhs.x + rhs, lhs.y + rhs }; }
    float2 operator+(const float2 &lhs, const int &rhs) { return { lhs.x + rhs, lhs.y + rhs }; }
    
    float3 operator+(const float3 &lhs, const float3 &rhs) { return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z }; }
    float3 operator+(const float3 &lhs, const int3 &rhs) { return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z }; }
    float3 operator+(const float3 &lhs, const float &rhs) { return { lhs.x + rhs, lhs.y + rhs, lhs.z + rhs }; }
    float3 operator+(const float3 &lhs, const int &rhs) { return { lhs.x + rhs, lhs.y + rhs, lhs.z + rhs }; }

    float4 operator+(const float4 &lhs, const float4 &rhs) { return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w }; }
    float4 operator+(const float4 &lhs, const float &rhs) { return { lhs.x + rhs, lhs.y + rhs, lhs.z + rhs, lhs.w + rhs }; }
    float4 operator+(const float4 &lhs, const int &rhs) { return { lhs.x + rhs, lhs.y + rhs, lhs.z + rhs, lhs.w + rhs }; }

    void operator+=(float2 &lhs, const float2 &rhs) { lhs.x += rhs.x; lhs.y += rhs.y; }
    void operator+=(float3 &lhs, const float3 &rhs) { lhs.x += rhs.x; lhs.y += rhs.y; lhs.z += rhs.z; }
    void operator+=(float4 &lhs, const float4 &rhs) { lhs.x += rhs.x; lhs.y += rhs.y; lhs.z += rhs.z; lhs.w += rhs.w; }

    //------------------------------------------------------------------------
    // Subtraction
    //------------------------------------------------------------------------

    int2 operator-(const int2 &lhs, const int2 &rhs) { return { lhs.x - rhs.x, lhs.y - rhs.y }; }
    int2 operator-(const int2 &lhs, const float2 &rhs) { return { lhs.x - (int)rhs.x, lhs.y - (int)rhs.y }; }
    int2 operator-(const int2 &lhs, const int &rhs) { return { lhs.x - rhs, lhs.y - rhs }; }
    int2 operator-(const int2 &lhs, const float &rhs) { return { lhs.x - (int)rhs, lhs.y - (int)rhs }; }
    
    int3 operator-(const int3 &lhs, const int3 &rhs) { return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z }; }
    int3 operator-(const int3 &lhs, const float3 &rhs) { return { lhs.x - (int)rhs.x, lhs.y - (int)rhs.y, lhs.z - (int)rhs.z }; }
    int3 operator-(const int3 &lhs, const int &rhs) { return { lhs.x - rhs, lhs.y - rhs, lhs.z - rhs }; }
    int3 operator-(const int3 &lhs, const float &rhs) { return { lhs.x - (int)rhs, lhs.y - (int)rhs, lhs.z - (int)rhs }; }

    float2 operator-(const float2 &lhs, const float2 &rhs) { return { lhs.x - rhs.x, lhs.y - rhs.y }; }
    float2 operator-(const float2 &lhs, const int2 &rhs) { return { lhs.x - rhs.x, lhs.y - rhs.y }; }
    float2 operator-(const float2 &lhs, const float &rhs) { return { lhs.x - rhs, lhs.y - rhs }; }
    float2 operator-(const float2 &lhs, const int &rhs) { return { lhs.x - rhs, lhs.y - rhs }; }

    float3 operator-(const float3 &lhs, const float3 &rhs) { return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z }; }
    float3 operator-(const float3 &lhs, const int3 &rhs) { return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z }; }
    float3 operator-(const float3 &lhs, const float &rhs) { return { lhs.x - rhs, lhs.y - rhs, lhs.z - rhs }; }
    float3 operator-(const float3 &lhs, const int &rhs) { return { lhs.x - rhs, lhs.y - rhs, lhs.z - rhs }; }

    float4 operator-(const float4 &lhs, const float4 &rhs) { return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w }; }
    float4 operator-(const float4 &lhs, const float &rhs) { return { lhs.x + rhs, lhs.y + rhs, lhs.z + rhs, lhs.w + rhs }; }
    float4 operator-(const float4 &lhs, const int &rhs) { return { lhs.x + rhs, lhs.y + rhs, lhs.z + rhs, lhs.w + rhs }; }

    void operator-=(float2 &lhs, const float2 &rhs) { lhs.x -= rhs.x; lhs.y -= rhs.y; }
    void operator-=(float3 &lhs, const float3 &rhs) { lhs.x -= rhs.x; lhs.y -= rhs.y; lhs.z -= rhs.z; }
    void operator-=(float4 &lhs, const float4 &rhs) { lhs.x -= rhs.x; lhs.y -= rhs.y; lhs.z -= rhs.z; lhs.w -= rhs.w; }

    //------------------------------------------------------------------------
    // Multiplication
    //------------------------------------------------------------------------

    int2 operator*(const int2 &lhs, const int2 &rhs) { return { lhs.x * rhs.x, lhs.y * rhs.y }; }
    int2 operator*(const int2 &lhs, const float2 &rhs) { return { lhs.x * (int)rhs.x, lhs.y * (int)rhs.y }; }
    int2 operator*(const int2 &lhs, const int &rhs) { return { lhs.x * rhs, lhs.y * rhs }; }
    int2 operator*(const int2 &lhs, const float &rhs) { return { lhs.x * (int)rhs, lhs.y * (int)rhs }; }
    
    int3 operator*(const int3 &lhs, const int3 &rhs) { return { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z }; }
    int3 operator*(const int3 &lhs, const float3 &rhs) { return { lhs.x * (int)rhs.x, lhs.y * (int)rhs.y, lhs.z * (int)rhs.z }; }
    int3 operator*(const int3 &lhs, const int &rhs) { return { lhs.x * rhs, lhs.y * rhs, lhs.z * rhs }; }
    int3 operator*(const int3 &lhs, const float &rhs) { return { lhs.x * (int)rhs, lhs.y * (int)rhs, lhs.z * (int)rhs }; }

    float3 operator*(const float3 &lhs, const float3 &rhs) { return { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z }; }
    float3 operator*(const float3 &lhs, const int3 &rhs) { return { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z }; }
    float3 operator*(const float3 &lhs, const float &rhs) { return { lhs.x * rhs, lhs.y * rhs, lhs.z * rhs }; }
    float3 operator*(const float3 &lhs, const int &rhs) { return { lhs.x * rhs, lhs.y * rhs, lhs.z * rhs }; }

    float4 operator*(const float4 &lhs, const float4 &rhs) { return { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w }; }
    float4 operator*(const float4 &lhs, const float &rhs) { return { lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs }; }
    float4 operator*(const float4 &lhs, const int &rhs) { return { lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs }; }

    void operator*=(float2 &lhs, const float2 &rhs) { lhs.x *= rhs.x; lhs.y *= rhs.y; }
    void operator*=(float3 &lhs, const float3 &rhs) { lhs.x *= rhs.x; lhs.y *= rhs.y; lhs.z *= rhs.z; }
    void operator*=(float4 &lhs, const float4 &rhs) { lhs.x *= rhs.x; lhs.y *= rhs.y; lhs.z *= rhs.z; lhs.w *= rhs.w; }

    //------------------------------------------------------------------------
    // Division
    //------------------------------------------------------------------------

    int2 operator/(const int2 &lhs, const int2 &rhs) { return { lhs.x / rhs.x, lhs.y / rhs.y }; }
    int2 operator/(const int2 &lhs, const float2 &rhs) { return { lhs.x / (int)rhs.x, lhs.y / (int)rhs.y }; }
    int2 operator/(const int2 &lhs, const int &rhs) { return { lhs.x / rhs, lhs.y / rhs }; }
    int2 operator/(const int2 &lhs, const float &rhs) { return { lhs.x / (int)rhs, lhs.y / (int)rhs }; }

    int3 operator/(const int3 &lhs, const int3 &rhs) { return { lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z }; }
    int3 operator/(const int3 &lhs, const float3 &rhs) { return { lhs.x / (int)rhs.x, lhs.y / (int)rhs.y, lhs.z / (int)rhs.z }; }
    int3 operator/(const int3 &lhs, const int &rhs) { return { lhs.x / rhs, lhs.y / rhs, lhs.z / rhs }; }
    int3 operator/(const int3 &lhs, const float &rhs) { return { lhs.x / (int)rhs, lhs.y / (int)rhs, lhs.z / (int)rhs }; }

    float3 operator/(const float3 &lhs, const float3 &rhs) { return { lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z }; }
    float3 operator/(const float3 &lhs, const int3 &rhs) { return { lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z }; }
    float3 operator/(const float3 &lhs, const float &rhs) { return { lhs.x / rhs, lhs.y / rhs, lhs.z / rhs }; }
    float3 operator/(const float3 &lhs, const int &rhs) { return { lhs.x / rhs, lhs.y / rhs, lhs.z / rhs }; }

    float4 operator/(const float4 &lhs, const float4 &rhs) { return { lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z, lhs.w / rhs.w }; }
    float4 operator/(const float4 &lhs, const float &rhs) { return { lhs.x / rhs, lhs.y / rhs, lhs.z / rhs, lhs.w / rhs }; }
    float4 operator/(const float4 &lhs, const int &rhs) { return { lhs.x / rhs, lhs.y / rhs, lhs.z / rhs, lhs.w / rhs }; }

    void operator/=(float2 &lhs, const float2 &rhs) { lhs.x /= rhs.x; lhs.y /= rhs.y; }
    void operator/=(float3 &lhs, const float3 &rhs) { lhs.x /= rhs.x; lhs.y /= rhs.y; lhs.z /= rhs.z; }
    void operator/=(float4 &lhs, const float4 &rhs) { lhs.x /= rhs.x; lhs.y /= rhs.y; lhs.z /= rhs.z; lhs.w = rhs.w; }
    
    //------------------------------------------------------------------------
    // Modulus
    //------------------------------------------------------------------------

    int2 operator%(const int2 &lhs, const int2 &rhs) { return { lhs.x % rhs.x, lhs.y % rhs.y }; }
    int2 operator%(const int2 &lhs, const int &rhs) { return { lhs.x % rhs, lhs.y % rhs }; }

    int3 operator%(const int3 &lhs, const int3 &rhs) { return { lhs.x % rhs.x, lhs.y % rhs.y, lhs.z % rhs.z }; }
    int3 operator%(const int3 &lhs, const int &rhs) { return { lhs.x % rhs, lhs.y % rhs, lhs.z % rhs }; }

    //------------------------------------------------------------------------
    // Equalities
    //------------------------------------------------------------------------

    bool operator==(const int2 &lhs, const int2 &rhs)
    {
        if (lhs.x != rhs.x) return false;
        if (lhs.y != rhs.y) return false;
        return true;
    }

    bool operator==(const int3 &lhs, const int3 &rhs)
    {
        if (lhs.x != rhs.x) return false;
        if (lhs.y != rhs.y) return false;
        if (lhs.z != rhs.z) return false;
        return true;
    }

    bool operator==(const float2 &lhs, const float2 &rhs)
    {
        if (lhs.x != rhs.x) return false;
        if (lhs.y != rhs.y) return false;
        return true;
    }

    bool operator==(const float3 &lhs, const float3 &rhs)
    {
        if (lhs.x != rhs.x) return false;
        if (lhs.y != rhs.y) return false;
        if (lhs.z != rhs.z) return false;
        return true;
    }

    bool operator==(const float4 &lhs, const float4 &rhs)
    {
        if (lhs.x != rhs.x) return false;
        if (lhs.y != rhs.y) return false;
        if (lhs.z != rhs.z) return false;
        if (lhs.w != rhs.w) return false;
        return true;
    }

    //------------------------------------------------------------------------
    // Inequalities
    //------------------------------------------------------------------------

    bool operator!=(const int2 &lhs, const int2 &rhs)
    {
        if (lhs.x == rhs.x) return false;
        if (lhs.y == rhs.y) return false;
        return true;
    }

    bool operator!=(const int3 &lhs, const int3 &rhs)
    {
        if (lhs.x == rhs.x) return false;
        if (lhs.y == rhs.y) return false;
        if (lhs.z == rhs.z) return false;
        return true;
    }

    bool operator!=(const float2 &lhs, const float2 &rhs)
    {
        if (lhs.x == rhs.x) return false;
        if (lhs.y == rhs.y) return false;
        return true;
    }

    bool operator!=(const float3 &lhs, const float3 &rhs)
    {
        if (lhs.x == rhs.x) return false;
        if (lhs.y == rhs.y) return false;
        if (lhs.z == rhs.z) return false;
        return true;
    }

    bool operator!=(const float4 &lhs, const float4 &rhs)
    {
        if (lhs.x == rhs.x) return false;
        if (lhs.y == rhs.y) return false;
        if (lhs.z == rhs.z) return false;
        if (lhs.w == rhs.w) return false;
        return true;
    }
}
