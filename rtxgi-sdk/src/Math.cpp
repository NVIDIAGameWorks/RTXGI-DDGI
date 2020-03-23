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

#include <time.h>
#include <stdlib.h>
#include <math.h>

#include "rtxgi/Math.h"

namespace rtxgi
{

    float3 Normalize(float3 vector)
    {
        float length = sqrtf(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
        return (vector / length);
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
