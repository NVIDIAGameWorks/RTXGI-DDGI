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

#include "Types.h"

namespace rtxgi
{

    static const float RTXGI_PI = 3.1415926535897932f;
    static const float RTXGI_2PI = 6.2831853071795864f;

    int AbsFloor(float f);

    template<typename T>
    T RadiansToDegrees(const T& radians)
    {
        return radians * 180.f / RTXGI_PI;
    };

    template<typename T>
    T DegreesToRadians(const T& degrees)
    {
        return degrees * RTXGI_PI / 180.f;
    };

    float3 Normalize(const float3& v);

    float3x3 EulerAnglesToRotationMatrixYXZ(const float3& eulerAngles);

    float4 QuaternionConjugate(const float4& q);
    float4 RotationMatrixToQuaternion(const float3x3& m);

    // --- Addition ------------------------------------------------------------

    int2 operator+(const int2 &lhs, const int2 &rhs);
    int2 operator+(const int2 &lhs, const float2 &rhs);
    int2 operator+(const int2 &lhs, const int &rhs);

    int3 operator+(const int3 &lhs, const int3 &rhs);
    int3 operator+(const int3 &lhs, const float3 &rhs);
    int3 operator+(const int3 &lhs, const int &rhs);

    void operator+=(int2 &lhs, const int2 &rhs);
    void operator+=(int3 &lhs, const int3 &rhs);
    void operator+=(int4 &lhs, const int4 &rhs);

    float2 operator+(const float2 &lhs, const float2 &rhs);
    float2 operator+(const float2 &lhs, const int2 &rhs);
    float2 operator+(const float2 &lhs, const float &rhs);
    float2 operator+(const float2 &lhs, const int &rhs);
    
    float3 operator+(const float3 &lhs, const float3 &rhs);
    float3 operator+(const float3 &lhs, const int3 &rhs);
    float3 operator+(const float3 &lhs, const float &rhs);
    float3 operator+(const float3 &lhs, const int &rhs);

    float4 operator+(const float4 &lhs, const float4 &rhs);
    float4 operator+(const float4 &lhs, const float &rhs);
    float4 operator+(const float4 &lhs, const int &rhs);
    
    void operator+=(float2 &lhs, const float2 &rhs);
    void operator+=(float3 &lhs, const float3 &rhs);
    void operator+=(float4 &lhs, const float4 &rhs);

    // --- Subtraction ---------------------------------------------------------

    int2 operator-(const int2 &lhs, const int2 &rhs);
    int2 operator-(const int2 &lhs, const float2 &rhs);
    int2 operator-(const int2 &lhs, const int &rhs);
    int2 operator-(const int2 &lhs, const float &rhs);

    int3 operator-(const int3 &lhs, const int3 &rhs);
    int3 operator-(const int3 &lhs, const float3 &rhs);
    int3 operator-(const int3 &lhs, const int &rhs);
    int3 operator-(const int3 &lhs, const float &rhs);

    float2 operator-(const float2 &lhs, const float2 &rhs);
    float2 operator-(const float2 &lhs, const int2 &rhs);
    float2 operator-(const float2 &lhs, const float &rhs);
    float2 operator-(const float2 &lhs, const int &rhs);

    float3 operator-(const float3 &lhs, const float3 &rhs);
    float3 operator-(const float3 &lhs, const int3 &rhs);
    float3 operator-(const float3 &lhs, const float &rhs);
    float3 operator-(const float3 &lhs, const int &rhs);

    float4 operator-(const float4 &lhs, const float4 &rhs);
    float4 operator-(const float4 &lhs, const float &rhs);
    float4 operator-(const float4 &lhs, const int &rhs);

    void operator-=(float2 &lhs, const float2 &rhs);
    void operator-=(float3 &lhs, const float3 &rhs);
    void operator-=(float4 &lhs, const float4 &rhs);

    // --- Multiplication ------------------------------------------------------

    int2 operator*(const int2 &lhs, const int2 &rhs);
    int2 operator*(const int2 &lhs, const float2 &rhs);
    int2 operator*(const int2 &lhs, const int &rhs);
    int2 operator*(const int2 &lhs, const float &rhs);

    int3 operator*(const int3 &lhs, const int3 &rhs);
    int3 operator*(const int3 &lhs, const float3 &rhs);
    int3 operator*(const int3 &lhs, const int &rhs);
    int3 operator*(const int3 &lhs, const float &rhs);

    float3 operator*(const float3 &lhs, const float3 &rhs);
    float3 operator*(const float3 &lhs, const int3 &rhs);
    float3 operator*(const float3 &lhs, const float &rhs);
    float3 operator*(const float3 &lhs, const int &rhs);

    float4 operator*(const float4 &lhs, const float4 &rhs);
    float4 operator*(const float4 &lhs, const float &rhs);
    float4 operator*(const float4 &lhs, const int &rhs);

    void operator*=(float2 &lhs, const float2 &rhs);
    void operator*=(float3 &lhs, const float3 &rhs);
    void operator*=(float4 &lhs, const float4 &rhs);

    // --- Division ------------------------------------------------------------

    int2 operator/(const int2 &lhs, const int2 &rhs);
    int2 operator/(const int2 &lhs, const float2 &rhs);
    int2 operator/(const int2 &lhs, const int &rhs);
    int2 operator/(const int2 &lhs, const float &rhs);

    int3 operator/(const int3 &lhs, const int3 &rhs);
    int3 operator/(const int3 &lhs, const float3 &rhs);
    int3 operator/(const int3 &lhs, const int &rhs);
    int3 operator/(const int3 &lhs, const float &rhs);

    float3 operator/(const float3 &lhs, const float3 &rhs);
    float3 operator/(const float3 &lhs, const int3 &rhs);
    float3 operator/(const float3 &lhs, const float &rhs);
    float3 operator/(const float3 &lhs, const int &rhs);

    float4 operator/(const float4 &lhs, const float4 &rhs);
    float4 operator/(const float4 &lhs, const float &rhs);
    float4 operator/(const float4 &lhs, const int &rhs);

    void operator/=(float2 &lhs, const float2 &rhs);
    void operator/=(float3 &lhs, const float3 &rhs);
    void operator/=(float4 &lhs, const float4 &rhs);

    // --- Modulus ------------------------------------------------------------
    int2 operator%(const int2& lhs, const int2& rhs);
    int2 operator%(const int2& lhs, const int& rhs);

    int3 operator%(const int3& lhs, const int3& rhs);
    int3 operator%(const int3& lhs, const int& rhs);

    // --- Equalities ------------------------------------------------------------

    bool operator==(const int2 &lhs, const int2&rhs);
    bool operator==(const int3 &lhs, const int3&rhs);
    bool operator==(const float2 &lhs, const float2 &rhs);
    bool operator==(const float3 &lhs, const float3 &rhs);
    bool operator==(const float4 &lhs, const float4 &rhs);

    // --- Inequalities ------------------------------------------------------------

    bool operator!=(const int2 &lhs, const int2 &rhs);
    bool operator!=(const int3 &lhs, const int3 &rhs);
    bool operator!=(const float2 &lhs, const float2 &rhs);
    bool operator!=(const float3 &lhs, const float3 &rhs);
    bool operator!=(const float4 &lhs, const float4 &rhs);

}
