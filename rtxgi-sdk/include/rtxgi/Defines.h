/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RTXGI_DEFINES_H
#define RTXGI_DEFINES_H

#ifndef RTXGI_NAME_D3D_OBJECTS
#define RTXGI_NAME_D3D_OBJECTS 1
#endif 

#ifndef RTXGI_PERF_MARKERS
#define RTXGI_PERF_MARKERS 1
#endif

#define RTXGI_COORDINATE_SYSTEM_LEFT 0
#define RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP 1
#define RTXGI_COORDINATE_SYSTEM_RIGHT 2
#define RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP 3

#ifndef RTXGI_COORDINATE_SYSTEM
#define RTXGI_COORDINATE_SYSTEM RTXGI_COORDINATE_SYSTEM_RIGHT
#endif

#endif /* RTXGI_DEFINES_H */
