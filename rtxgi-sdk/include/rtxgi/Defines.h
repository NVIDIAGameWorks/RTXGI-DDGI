/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RTXGI_DEFINES_H
#define RTXGI_DEFINES_H

// Define RTXGI_GFX_NAME_OBJECTS before including DDGIVolume.h to enable debug names
// for graphics objects for use with debugging tools (e.g. NVIDIA Nsight Graphics).
// Exposed in CMake as RTXGI_GFX_NAME_OBJECTS
//#define RTXGI_GFX_NAME_OBJECTS

// Default performance marker colors for use with debugging tools (e.g. NVIDIA Nsight Graphics).
#define RTXGI_PERF_MARKER_RED 204, 28, 41
#define RTXGI_PERF_MARKER_GREEN 105, 148, 79
#define RTXGI_PERF_MARKER_BLUE 65, 126, 211
#define RTXGI_PERF_MARKER_ORANGE 217, 122, 46
#define RTXGI_PERF_MARKER_YELLOW 217, 207, 46
#define RTXGI_PERF_MARKER_PURPLE 152, 78, 163
#define RTXGI_PERF_MARKER_BROWN 166, 86, 40
#define RTXGI_PERF_MARKER_GREY 190, 190, 190

// Coordinate system defines
#define RTXGI_COORDINATE_SYSTEM_LEFT 0
#define RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP 1
#define RTXGI_COORDINATE_SYSTEM_RIGHT 2
#define RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP 3

// Define RTXGI_COORDINATE_SYSTEM before including DDGIVolume.h and before compiling
// SDK HLSL shaders to use another coordinate system. Default is right handed, y-up.
// Exposed in CMake as RTXGI_COORDINATE_SYSTEM
#ifndef RTXGI_COORDINATE_SYSTEM
#error RTXGI_COORDINATE_SYSTEM is not defined!
#endif

#endif // RTXGI_DEFINES_H
