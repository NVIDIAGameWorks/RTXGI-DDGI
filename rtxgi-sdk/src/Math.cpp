/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "rtxgi/Math.h"
#include "rtxgi/Defines.h"

#include <math.h>

namespace rtxgi
{

    int abs(const int value)
    {
        return value < 0 ? -value : value;
    }

    float abs(const float value)
    {
        return value < 0.f ? -value : value;
    }

    int AbsFloor(const float value)
    {
        return value >= 0.f ? int(floor(value)) : int(ceil(value));
    }

    float Distance(const float3& a, const float3& b)
    {
        return sqrtf(powf(b.x - a.x, 2.f) + powf(b.y - a.y, 2.f) + powf(b.z - a.z, 2.f));
    }

    float Dot(const float3& a, const float3& b)
    {
        return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
    }

    float3 Cross(const float3& a, const float3& b)
    {
        return {(a.y * b.z) - (a.z * b.y), (a.z * b.x - a.x * b.z), (a.x * b.y - a.y * b.x)};
    }

    float3 Normalize(const float3& v)
    {
        float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
        return (v / length);
    }

    float3 Min(const float3& a, const float3& b)
    {
        return { fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z) };
    }

    float3 Max(const float3& a, const float3& b)
    {
        return { fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z) };
    }

    int Sign(const int value)
    {
        return value >= 0 ? 1 : -1;
    }

    int Sign(const float value)
    {
        return value >= 0.f ? 1 : -1;
    }

    // Convert right handed, y-up Euler angles to the specified coordinate system
    float3 ConvertEulerAngles(const float3& input, ECoordinateSystem target)
    {
        float3 result = input;
        if (target == ECoordinateSystem::RH_ZUP)
        {
            result = { input.x + 90.f, input.y, input.z };
        }
        else if (target == ECoordinateSystem::LH_YUP)
        {
            result = { -input.x, -input.y, input.z };
        }
        else if (target == ECoordinateSystem::LH_ZUP)
        {
            result = { input.z, -input.x, -input.y };
        }
        return DegreesToRadians(result);
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
            q.w = sqrtf(diagSum + 1.f) * 0.5f;
            float f = 0.25f / q.w;
            q.x = (m21 - m12) * f;
            q.y = (m02 - m20) * f;
            q.z = (m10 - m01) * f;
        }
        else if ((m00 > m11) && (m00 > m22))
        {
            q.x = sqrtf(m00 - m11 - m22 + 1.f) * 0.5f;
            float f = 0.25f / q.x;
            q.y = (m10 + m01) * f;
            q.z = (m02 + m20) * f;
            q.w = (m21 - m12) * f;
        }
        else if (m11 > m22)
        {
            q.y = sqrtf(m11 - m00 - m22 + 1.f) * 0.5f;
            float f = 0.25f / q.y;
            q.x = (m10 + m01) * f;
            q.z = (m21 + m12) * f;
            q.w = (m02 - m20) * f;
        }
        else
        {
            q.z = sqrtf(m22 - m00 - m11 + 1.f) * 0.5f;
            float f = 0.25f / q.z;
            q.x = (m02 + m20) * f;
            q.y = (m21 + m12) * f;
            q.w = (m10 - m01) * f;
        }

        return q;
    }

    float3x3 EulerAnglesToRotationMatrix(const float3& eulerAngles)
    {
        float sx = sinf(eulerAngles.x);
        float cx = cosf(eulerAngles.x);
        float sy = sinf(eulerAngles.y);
        float cy = cosf(eulerAngles.y);
        float sz = sinf(eulerAngles.z);
        float cz = cosf(eulerAngles.z);

      //float3x3 Rx = {
      //    { 1.f, 0.f, 0.f },
      //    { 0.f,  cx, -sx },
      //    { 0.f,  sx,  cx }
      //};

      //float3x3 Ry = {
      //    {  cy,  0.f,  sy },
      //    { 0.f,  1.f, 0.f },
      //    { -sy,  0.f,  cy }
      //};

      //float3x3 Rz = {
      //    {  cz, -sz, 0.f },
      //    {  sz,  cz, 0.f },
      //    { 0.f, 0.f, 1.f }
      //};

    #if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
        // Ryzx = Ry (pitch) * Rz (yaw) * Rx (roll)
        float3x3 rotation = {
            {  cy * cz, sx * sy - cx * cy * sz, cx * sy + cy * sx * sz },
            {       sz,                cx * cz,               -cz * sx },
            { -cz * sy, cy * sx + cx * sy * sz,  cx * cy - sx * sy * sz },
        };
    #else
        // Rxyz = Rx (pitch) * Ry (yaw) * Rz (roll)
        float3x3 rotation = {
            {                cy * cz,               -cy * sz,       sy },
            { cx * sz + cz * sx * sy, cx * cz - sx * sy * sz, -cy * sx },
            { sx * sz - cx * cz * sy, cz * sx + cx * sy * sz,  cx * cy },
        };
    #endif

        return rotation;
    }

    //------------------------------------------------------------------------
    // Addition
    //------------------------------------------------------------------------

    int2 operator+(const int2& lhs, const int2& rhs) { return { lhs.x + rhs.x, lhs.y + rhs.y }; }
    int2 operator+(const int2& lhs, const float2& rhs) { return { lhs.x + (int)rhs.x, lhs.y + (int)rhs.y }; }
    int2 operator+(const int2& lhs, const int& rhs) { return { lhs.x + rhs, lhs.y + rhs }; }
    int2 operator+(const int2& lhs, const float& rhs) { return { lhs.x + (int)rhs, lhs.y + (int)rhs }; }

    int3 operator+(const int3& lhs, const int3& rhs) { return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z }; }
    int3 operator+(const int3& lhs, const float3& rhs) { return { lhs.x + (int)rhs.x, lhs.y + (int)rhs.y, lhs.z + (int)rhs.z }; }
    int3 operator+(const int3& lhs, const int& rhs) { return { lhs.x + rhs, lhs.y + rhs, lhs.z + rhs }; }
    int3 operator+(const int3& lhs, const float& rhs) { return { lhs.x + (int)rhs, lhs.y + (int)rhs, lhs.z + (int)rhs }; }

    void operator+=(int2& lhs, const int2& rhs) { lhs.x += rhs.x; lhs.y += rhs.y; }
    void operator+=(int3& lhs, const int3& rhs) { lhs.x += rhs.x; lhs.y += rhs.y; lhs.z += rhs.z; }
    void operator+=(int4& lhs, const int4& rhs) { lhs.x += rhs.x; lhs.y += rhs.y; lhs.z += rhs.z; lhs.w += rhs.w; }

    float2 operator+(const float2& lhs, const float2& rhs) { return { lhs.x + rhs.x, lhs.y + rhs.y }; }
    float2 operator+(const float2& lhs, const int2& rhs) { return { lhs.x + (float)rhs.x, lhs.y + (float)rhs.y }; }
    float2 operator+(const float2& lhs, const float& rhs) { return { lhs.x + rhs, lhs.y + rhs }; }
    float2 operator+(const float2& lhs, const int& rhs) { return { lhs.x + (float)rhs, lhs.y + (float)rhs }; }

    float3 operator+(const float3& lhs, const float3& rhs) { return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z }; }
    float3 operator+(const float3& lhs, const int3& rhs) { return { lhs.x + (float)rhs.x, lhs.y + (float)rhs.y, lhs.z + (float)rhs.z }; }
    float3 operator+(const float3& lhs, const float& rhs) { return { lhs.x + rhs, lhs.y + rhs, lhs.z + rhs }; }
    float3 operator+(const float3& lhs, const int& rhs) { return { lhs.x + (float)rhs, lhs.y + (float)rhs, lhs.z + (float)rhs }; }

    float4 operator+(const float4& lhs, const float4& rhs) { return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w }; }
    float4 operator+(const float4& lhs, const float& rhs) { return { lhs.x + rhs, lhs.y + rhs, lhs.z + rhs, lhs.w + rhs }; }
    float4 operator+(const float4& lhs, const int& rhs) { return { lhs.x + (float)rhs, lhs.y + (float)rhs, lhs.z + (float)rhs, lhs.w + (float)rhs }; }

    void operator+=(float2& lhs, const float2& rhs) { lhs.x += rhs.x; lhs.y += rhs.y; }
    void operator+=(float3& lhs, const float3& rhs) { lhs.x += rhs.x; lhs.y += rhs.y; lhs.z += rhs.z; }
    void operator+=(float4& lhs, const float4& rhs) { lhs.x += rhs.x; lhs.y += rhs.y; lhs.z += rhs.z; lhs.w += rhs.w; }

    //------------------------------------------------------------------------
    // Subtraction
    //------------------------------------------------------------------------

    int2 operator-(const int2& lhs, const int2& rhs) { return { lhs.x - rhs.x, lhs.y - rhs.y }; }
    int2 operator-(const int2& lhs, const float2& rhs) { return { lhs.x - (int)rhs.x, lhs.y - (int)rhs.y }; }
    int2 operator-(const int2& lhs, const int& rhs) { return { lhs.x - rhs, lhs.y - rhs }; }
    int2 operator-(const int2& lhs, const float& rhs) { return { lhs.x - (int)rhs, lhs.y - (int)rhs }; }

    int3 operator-(const int3& lhs, const int3& rhs) { return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z }; }
    int3 operator-(const int3& lhs, const float3& rhs) { return { lhs.x - (int)rhs.x, lhs.y - (int)rhs.y, lhs.z - (int)rhs.z }; }
    int3 operator-(const int3& lhs, const int& rhs) { return { lhs.x - rhs, lhs.y - rhs, lhs.z - rhs }; }
    int3 operator-(const int3& lhs, const float& rhs) { return { lhs.x - (int)rhs, lhs.y - (int)rhs, lhs.z - (int)rhs }; }

    float2 operator-(const float2& lhs, const float2& rhs) { return { lhs.x - rhs.x, lhs.y - rhs.y }; }
    float2 operator-(const float2& lhs, const int2& rhs) { return { lhs.x - (float)rhs.x, lhs.y - (float)rhs.y }; }
    float2 operator-(const float2& lhs, const float& rhs) { return { lhs.x - rhs, lhs.y - rhs }; }
    float2 operator-(const float2& lhs, const int& rhs) { return { lhs.x - (float)rhs, lhs.y - (float)rhs }; }

    float3 operator-(const float3& lhs, const float3& rhs) { return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z }; }
    float3 operator-(const float3& lhs, const int3& rhs) { return { lhs.x - (float)rhs.x, lhs.y - (float)rhs.y, lhs.z - (float)rhs.z }; }
    float3 operator-(const float3& lhs, const float& rhs) { return { lhs.x - rhs, lhs.y - rhs, lhs.z - rhs }; }
    float3 operator-(const float3& lhs, const int& rhs) { return { lhs.x - (float)rhs, lhs.y - (float)rhs, lhs.z - (float)rhs }; }

    float4 operator-(const float4& lhs, const float4 &rhs) { return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w }; }
    float4 operator-(const float4& lhs, const float &rhs) { return { lhs.x + rhs, lhs.y + rhs, lhs.z + rhs, lhs.w + rhs }; }
    float4 operator-(const float4& lhs, const int &rhs) { return { lhs.x + (float)rhs, lhs.y + (float)rhs, lhs.z + (float)rhs, lhs.w + (float)rhs }; }

    void operator-=(float2& lhs, const float2& rhs) { lhs.x -= rhs.x; lhs.y -= rhs.y; }
    void operator-=(float3& lhs, const float3& rhs) { lhs.x -= rhs.x; lhs.y -= rhs.y; lhs.z -= rhs.z; }
    void operator-=(float4& lhs, const float4& rhs) { lhs.x -= rhs.x; lhs.y -= rhs.y; lhs.z -= rhs.z; lhs.w -= rhs.w; }

    //------------------------------------------------------------------------
    // Multiplication
    //------------------------------------------------------------------------

    int2 operator*(const int2& lhs, const int2& rhs) { return { lhs.x * rhs.x, lhs.y * rhs.y }; }
    int2 operator*(const int2& lhs, const float2& rhs) { return { lhs.x * (int)rhs.x, lhs.y * (int)rhs.y }; }
    int2 operator*(const int2& lhs, const int& rhs) { return { lhs.x * rhs, lhs.y * rhs }; }
    int2 operator*(const int2& lhs, const float& rhs) { return { lhs.x * (int)rhs, lhs.y * (int)rhs }; }

    int3 operator*(const int3& lhs, const int3& rhs) { return { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z }; }
    int3 operator*(const int3& lhs, const float3& rhs) { return { lhs.x * (int)rhs.x, lhs.y * (int)rhs.y, lhs.z * (int)rhs.z }; }
    int3 operator*(const int3& lhs, const int& rhs) { return { lhs.x * rhs, lhs.y * rhs, lhs.z * rhs }; }
    int3 operator*(const int3& lhs, const float& rhs) { return { lhs.x * (int)rhs, lhs.y * (int)rhs, lhs.z * (int)rhs }; }

    float3 operator*(const float3& lhs, const float3& rhs) { return { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z }; }
    float3 operator*(const float3& lhs, const int3& rhs) { return { lhs.x * (float)rhs.x, lhs.y * (float)rhs.y, lhs.z * (float)rhs.z }; }
    float3 operator*(const float3& lhs, const float& rhs) { return { lhs.x * rhs, lhs.y * rhs, lhs.z * (float)rhs }; }
    float3 operator*(const float3& lhs, const int& rhs) { return { lhs.x * (float)rhs, lhs.y * (float)rhs, lhs.z * (float)rhs }; }

    float4 operator*(const float4& lhs, const float4& rhs) { return { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w }; }
    float4 operator*(const float4& lhs, const float& rhs) { return { lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs }; }
    float4 operator*(const float4& lhs, const int& rhs) { return { lhs.x * (float)rhs, lhs.y * (float)rhs, lhs.z * (float)rhs, lhs.w * (float)rhs }; }

    void operator*=(float2& lhs, const float2& rhs) { lhs.x *= rhs.x; lhs.y *= rhs.y; }
    void operator*=(float3& lhs, const float3& rhs) { lhs.x *= rhs.x; lhs.y *= rhs.y; lhs.z *= rhs.z; }
    void operator*=(float4& lhs, const float4& rhs) { lhs.x *= rhs.x; lhs.y *= rhs.y; lhs.z *= rhs.z; lhs.w *= rhs.w; }

    //------------------------------------------------------------------------
    // Division
    //------------------------------------------------------------------------

    int2 operator/(const int2& lhs, const int2& rhs) { return { lhs.x / rhs.x, lhs.y / rhs.y }; }
    int2 operator/(const int2& lhs, const float2& rhs) { return { lhs.x / (int)rhs.x, lhs.y / (int)rhs.y }; }
    int2 operator/(const int2& lhs, const int& rhs) { return { lhs.x / rhs, lhs.y / rhs }; }
    int2 operator/(const int2& lhs, const float& rhs) { return { lhs.x / (int)rhs, lhs.y / (int)rhs }; }

    int3 operator/(const int3& lhs, const int3& rhs) { return { lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z }; }
    int3 operator/(const int3& lhs, const float3& rhs) { return { lhs.x / (int)rhs.x, lhs.y / (int)rhs.y, lhs.z / (int)rhs.z }; }
    int3 operator/(const int3& lhs, const int& rhs) { return { lhs.x / rhs, lhs.y / rhs, lhs.z / rhs }; }
    int3 operator/(const int3& lhs, const float& rhs) { return { lhs.x / (int)rhs, lhs.y / (int)rhs, lhs.z / (int)rhs }; }

    float3 operator/(const float3& lhs, const float3& rhs) { return { lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z }; }
    float3 operator/(const float3& lhs, const int3& rhs) { return { lhs.x / (float)rhs.x, lhs.y / (float)rhs.y, lhs.z / (float)rhs.z }; }
    float3 operator/(const float3& lhs, const float& rhs) { return { lhs.x / rhs, lhs.y / rhs, lhs.z / rhs }; }
    float3 operator/(const float3& lhs, const int& rhs) { return { lhs.x / (float)rhs, lhs.y / (float)rhs, lhs.z / (float)rhs }; }

    float4 operator/(const float4& lhs, const float4& rhs) { return { lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z, lhs.w / rhs.w }; }
    float4 operator/(const float4& lhs, const float& rhs) { return { lhs.x / rhs, lhs.y / rhs, lhs.z / rhs, lhs.w / rhs }; }
    float4 operator/(const float4& lhs, const int& rhs) { return { lhs.x / (float)rhs, lhs.y / (float)rhs, lhs.z / (float)rhs, lhs.w / (float)rhs }; }

    void operator/=(float2& lhs, const float2& rhs) { lhs.x /= rhs.x; lhs.y /= rhs.y; }
    void operator/=(float3& lhs, const float3& rhs) { lhs.x /= rhs.x; lhs.y /= rhs.y; lhs.z /= rhs.z; }
    void operator/=(float4& lhs, const float4& rhs) { lhs.x /= rhs.x; lhs.y /= rhs.y; lhs.z /= rhs.z; lhs.w = rhs.w; }

    //------------------------------------------------------------------------
    // Modulus
    //------------------------------------------------------------------------

    int2 operator%(const int2& lhs, const int2& rhs) { return { lhs.x % rhs.x, lhs.y % rhs.y }; }
    int2 operator%(const int2& lhs, const int& rhs) { return { lhs.x % rhs, lhs.y % rhs }; }

    int3 operator%(const int3& lhs, const int3& rhs) { return { lhs.x % rhs.x, lhs.y % rhs.y, lhs.z % rhs.z }; }
    int3 operator%(const int3& lhs, const int& rhs) { return { lhs.x % rhs, lhs.y % rhs, lhs.z % rhs }; }

    //------------------------------------------------------------------------
    // Equalities
    //------------------------------------------------------------------------

    bool operator==(const int2& lhs, const int2& rhs)
    {
        if (lhs.x != rhs.x) return false;
        if (lhs.y != rhs.y) return false;
        return true;
    }

    bool operator==(const int3& lhs, const int3& rhs)
    {
        if (lhs.x != rhs.x) return false;
        if (lhs.y != rhs.y) return false;
        if (lhs.z != rhs.z) return false;
        return true;
    }

    bool operator==(const float2& lhs, const float2& rhs)
    {
        if (lhs.x != rhs.x) return false;
        if (lhs.y != rhs.y) return false;
        return true;
    }

    bool operator==(const float3& lhs, const float3& rhs)
    {
        if (lhs.x != rhs.x) return false;
        if (lhs.y != rhs.y) return false;
        if (lhs.z != rhs.z) return false;
        return true;
    }

    bool operator==(const float4& lhs, const float4& rhs)
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

    bool operator!=(const int2& lhs, const int2& rhs)
    {
        if (lhs.x == rhs.x) return false;
        if (lhs.y == rhs.y) return false;
        return true;
    }

    bool operator!=(const int3& lhs, const int3& rhs)
    {
        if (lhs.x == rhs.x) return false;
        if (lhs.y == rhs.y) return false;
        if (lhs.z == rhs.z) return false;
        return true;
    }

    bool operator!=(const float2& lhs, const float2& rhs)
    {
        if (lhs.x == rhs.x) return false;
        if (lhs.y == rhs.y) return false;
        return true;
    }

    bool operator!=(const float3& lhs, const float3& rhs)
    {
        if (lhs.x == rhs.x) return false;
        if (lhs.y == rhs.y) return false;
        if (lhs.z == rhs.z) return false;
        return true;
    }

    bool operator!=(const float4& lhs, const float4& rhs)
    {
        if (lhs.x == rhs.x) return false;
        if (lhs.y == rhs.y) return false;
        if (lhs.z == rhs.z) return false;
        if (lhs.w == rhs.w) return false;
        return true;
    }
}
