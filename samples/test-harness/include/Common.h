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

#if defined(_WIN32) || defined(WIN32)
#define WIN32_LEAN_AND_MEAN    // Exclude rarely-used items from Windows headers
#define NOMINMAX               // Exclude Windows defines of min and max
#include <Windows.h>
#include <tchar.h>
#endif

#include <algorithm>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>

#include <math.h>

#if defined(_WIN32) || defined(WIN32)
#include <Unknwn.h>                             // for IUnknown interface that IDxCBlob extends
#elif __linux__
#include "thirdparty/directx/winadapter.h"      // Windows adapter for Linux
#endif

#include <DirectXMath.h>     // Removes sal.h include since this is covered by winadapter.h on Linux

#ifdef API_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>

#if defined(API_D3D12) && defined(_WIN32) || defined(WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#if _DEBUG
    #define _CRTDBG_MAP_ALLOC
    #include <stdlib.h>
#if defined(_WIN32) || defined(WIN32)
    #include <crtdbg.h>
#elif __linux__
    // TODO: memory leak checking on UNIX
#endif
#endif

enum class ERenderMode
{
    PATH_TRACE = 0,
    DDGI,
    Count
};

enum class ELightType
{
    DIRECTIONAL,
    SPOT,
    POINT,
    COUNT
};

// Macros
#define SAFE_RELEASE(x) { if (x) { x->Release(); x = 0; } }
#define SAFE_DELETE(x) { if(x) delete x; x = NULL; }
#define SAFE_DELETE_ARRAY(x) { if(x) delete[] x; x = NULL; }
#define ALIGN(_alignment, _val) (((_val + _alignment - 1) / _alignment) * _alignment)
#define CHECK(status, message, log) if(!status) { log << "\nFailed to " << message; std::flush(log); return false; }

inline static uint32_t DivRoundUp(uint32_t x, uint32_t y)
{
    if (x % y) return 1 + x / y;
    else return x / y;
}

// Defines
#define COORDINATE_SYSTEM_LEFT 0
#define COORDINATE_SYSTEM_LEFT_Z_UP 1
#define COORDINATE_SYSTEM_RIGHT 2
#define COORDINATE_SYSTEM_RIGHT_Z_UP 3
// #define COORDINATE_SYSTEM COORDINATE_SYSTEM_RIGHT  // set by CMake

inline const char* GetCoordinateSystemName(uint32_t coordinateSystem)
{
    if(coordinateSystem == 0) return "Left Hand, Y-Up";
    else if(coordinateSystem == 1) return "Left Hand, Z-Up";
    else if(coordinateSystem == 2) return "Right Hand, Y-Up";
    else if(coordinateSystem == 3) return "Right Hand, Z-Up";
    else return "Unknown";
}

#if defined(_WIN32) || defined(WIN32)
#define GPU_COMPRESSION
#endif
