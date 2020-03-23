/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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

    const static float RTXGI_PI = 3.1415926535897932f;

    float3 Normalize(float3 vector);

    // --- Addition ------------------------------------------------------------

    int2 operator+(const int2 &lhs, const int2 &rhs);
    int2 operator+(const int2 &lhs, const float2 &rhs);
    int2 operator+(const int2 &lhs, const int &rhs);

    int3 operator+(const int3 &lhs, const int3 &rhs);
    int3 operator+(const int3 &lhs, const float3 &rhs);
    int3 operator+(const int3 &lhs, const int &rhs);

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
