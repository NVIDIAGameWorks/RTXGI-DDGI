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

#include "Common.h"
#include "Types.h"

namespace rtxgi
{

    static const float RTXGI_PI = 3.1415926535897932f;
    static const float RTXGI_2PI = 6.2831853071795864f;

    enum class ECoordinateSystem
    {
        LH_YUP = 0,
        LH_ZUP,
        RH_YUP,
        RH_ZUP,
    };

    RTXGI_API int    abs(const int value);
    RTXGI_API float  abs(const float value);
    RTXGI_API int    AbsFloor(const float f);
    RTXGI_API float  Distance(const float3& a, const float3& b);
    RTXGI_API float  Dot(const float3& a, const float3& b);
    RTXGI_API float3 Cross(const float3& a, const float3& b);
    RTXGI_API float3 Normalize(const float3& v);
    RTXGI_API float3 Min(const float3& a, const float3& b);
    RTXGI_API float3 Max(const float3& a, const float3& b);
    RTXGI_API int    Sign(const int value);
    RTXGI_API int    Sign(const float value);

    template<typename T>
    RTXGI_API T RadiansToDegrees(const T& radians) { return radians * 180.f / RTXGI_PI; }

    template<typename T>
    RTXGI_API T DegreesToRadians(const T& degrees) { return degrees * RTXGI_PI / 180.f; }

    RTXGI_API float3 ConvertEulerAngles(const float3& input, ECoordinateSystem target);
    RTXGI_API float4 QuaternionConjugate(const float4& q);
    RTXGI_API float4 RotationMatrixToQuaternion(const float3x3& m);

    RTXGI_API float3x3 EulerAnglesToRotationMatrix(const float3& eulerAngles);

    // --- Addition ------------------------------------------------------------

    RTXGI_API int2 operator+(const int2& lhs, const int2& rhs);
    RTXGI_API int2 operator+(const int2& lhs, const float2& rhs);
    RTXGI_API int2 operator+(const int2& lhs, const int& rhs);

    RTXGI_API int3 operator+(const int3& lhs, const int3& rhs);
    RTXGI_API int3 operator+(const int3& lhs, const float3& rhs);
    RTXGI_API int3 operator+(const int3& lhs, const int& rhs);

    RTXGI_API void operator+=(int2& lhs, const int2& rhs);
    RTXGI_API void operator+=(int3& lhs, const int3& rhs);
    RTXGI_API void operator+=(int4& lhs, const int4& rhs);

    RTXGI_API float2 operator+(const float2& lhs, const float2& rhs);
    RTXGI_API float2 operator+(const float2& lhs, const int2& rhs);
    RTXGI_API float2 operator+(const float2& lhs, const float& rhs);
    RTXGI_API float2 operator+(const float2& lhs, const int& rhs);

    RTXGI_API float3 operator+(const float3& lhs, const float3& rhs);
    RTXGI_API float3 operator+(const float3& lhs, const int3& rhs);
    RTXGI_API float3 operator+(const float3& lhs, const float& rhs);
    RTXGI_API float3 operator+(const float3& lhs, const int& rhs);

    RTXGI_API float4 operator+(const float4& lhs, const float4& rhs);
    RTXGI_API float4 operator+(const float4& lhs, const float& rhs);
    RTXGI_API float4 operator+(const float4& lhs, const int& rhs);

    RTXGI_API void operator+=(float2& lhs, const float2& rhs);
    RTXGI_API void operator+=(float3& lhs, const float3& rhs);
    RTXGI_API void operator+=(float4& lhs, const float4& rhs);

    // --- Subtraction ---------------------------------------------------------

    RTXGI_API int2 operator-(const int2 &lhs, const int2& rhs);
    RTXGI_API int2 operator-(const int2 &lhs, const float2& rhs);
    RTXGI_API int2 operator-(const int2 &lhs, const int& rhs);
    RTXGI_API int2 operator-(const int2 &lhs, const float& rhs);

    RTXGI_API int3 operator-(const int3 &lhs, const int3& rhs);
    RTXGI_API int3 operator-(const int3 &lhs, const float3& rhs);
    RTXGI_API int3 operator-(const int3 &lhs, const int& rhs);
    RTXGI_API int3 operator-(const int3 &lhs, const float& rhs);

    RTXGI_API float2 operator-(const float2 &lhs, const float2& rhs);
    RTXGI_API float2 operator-(const float2 &lhs, const int2& rhs);
    RTXGI_API float2 operator-(const float2 &lhs, const float& rhs);
    RTXGI_API float2 operator-(const float2 &lhs, const int& rhs);

    RTXGI_API float3 operator-(const float3 &lhs, const float3& rhs);
    RTXGI_API float3 operator-(const float3 &lhs, const int3& rhs);
    RTXGI_API float3 operator-(const float3 &lhs, const float& rhs);
    RTXGI_API float3 operator-(const float3 &lhs, const int& rhs);

    RTXGI_API float4 operator-(const float4 &lhs, const float4& rhs);
    RTXGI_API float4 operator-(const float4 &lhs, const float& rhs);
    RTXGI_API float4 operator-(const float4 &lhs, const int& rhs);

    RTXGI_API void operator-=(float2& lhs, const float2& rhs);
    RTXGI_API void operator-=(float3& lhs, const float3& rhs);
    RTXGI_API void operator-=(float4& lhs, const float4& rhs);

    // --- Multiplication ------------------------------------------------------

    RTXGI_API int2 operator*(const int2& lhs, const int2& rhs);
    RTXGI_API int2 operator*(const int2& lhs, const float2& rhs);
    RTXGI_API int2 operator*(const int2& lhs, const int& rhs);
    RTXGI_API int2 operator*(const int2& lhs, const float& rhs);

    RTXGI_API int3 operator*(const int3& lhs, const int3& rhs);
    RTXGI_API int3 operator*(const int3& lhs, const float3& rhs);
    RTXGI_API int3 operator*(const int3& lhs, const int& rhs);
    RTXGI_API int3 operator*(const int3& lhs, const float& rhs);

    RTXGI_API float3 operator*(const float3& lhs, const float3& rhs);
    RTXGI_API float3 operator*(const float3& lhs, const int3& rhs);
    RTXGI_API float3 operator*(const float3& lhs, const float& rhs);
    RTXGI_API float3 operator*(const float3& lhs, const int& rhs);

    RTXGI_API float4 operator*(const float4& lhs, const float4& rhs);
    RTXGI_API float4 operator*(const float4& lhs, const float& rhs);
    RTXGI_API float4 operator*(const float4& lhs, const int& rhs);

    RTXGI_API void operator*=(float2& lhs, const float2& rhs);
    RTXGI_API void operator*=(float3& lhs, const float3& rhs);
    RTXGI_API void operator*=(float4& lhs, const float4& rhs);

    // --- Division ------------------------------------------------------------

    RTXGI_API int2 operator/(const int2& lhs, const int2& rhs);
    RTXGI_API int2 operator/(const int2& lhs, const float2& rhs);
    RTXGI_API int2 operator/(const int2& lhs, const int& rhs);
    RTXGI_API int2 operator/(const int2& lhs, const float& rhs);

    RTXGI_API int3 operator/(const int3& lhs, const int3& rhs);
    RTXGI_API int3 operator/(const int3& lhs, const float3& rhs);
    RTXGI_API int3 operator/(const int3& lhs, const int& rhs);
    RTXGI_API int3 operator/(const int3& lhs, const float& rhs);

    RTXGI_API float3 operator/(const float3& lhs, const float3& rhs);
    RTXGI_API float3 operator/(const float3& lhs, const int3& rhs);
    RTXGI_API float3 operator/(const float3& lhs, const float& rhs);
    RTXGI_API float3 operator/(const float3& lhs, const int& rhs);

    RTXGI_API float4 operator/(const float4& lhs, const float4& rhs);
    RTXGI_API float4 operator/(const float4& lhs, const float& rhs);
    RTXGI_API float4 operator/(const float4& lhs, const int& rhs);

    RTXGI_API void operator/=(float2& lhs, const float2& rhs);
    RTXGI_API void operator/=(float3& lhs, const float3& rhs);
    RTXGI_API void operator/=(float4& lhs, const float4& rhs);

    // --- Modulus ------------------------------------------------------------

    RTXGI_API int2 operator%(const int2& lhs, const int2& rhs);
    RTXGI_API int2 operator%(const int2& lhs, const int& rhs);

    RTXGI_API int3 operator%(const int3& lhs, const int3& rhs);
    RTXGI_API int3 operator%(const int3& lhs, const int& rhs);

    // --- Equalities ------------------------------------------------------------

    RTXGI_API bool operator==(const int2& lhs, const int2& rhs);
    RTXGI_API bool operator==(const int3& lhs, const int3& rhs);
    RTXGI_API bool operator==(const float2& lhs, const float2& rhs);
    RTXGI_API bool operator==(const float3& lhs, const float3& rhs);
    RTXGI_API bool operator==(const float4& lhs, const float4& rhs);

    // --- Inequalities ------------------------------------------------------------

    RTXGI_API bool operator!=(const int2& lhs, const int2& rhs);
    RTXGI_API bool operator!=(const int3& lhs, const int3& rhs);
    RTXGI_API bool operator!=(const float2& lhs, const float2& rhs);
    RTXGI_API bool operator!=(const float3& lhs, const float3& rhs);
    RTXGI_API bool operator!=(const float4& lhs, const float4& rhs);

}
