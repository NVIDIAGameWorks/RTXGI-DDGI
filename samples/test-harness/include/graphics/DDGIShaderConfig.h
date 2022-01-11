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

// DDGI Shader Configuration options

// Required Defines: these must *match* the settings in CMake.
// If you change one of these three options in CMake, you need to update them here too!
#define RTXGI_COORDINATE_SYSTEM 2
#define RTXGI_DDGI_SHADER_REFLECTION 0
#define RTXGI_DDGI_BINDLESS_RESOURCES 0

// Optional Defines (including in this file since we compile with warnings as errors)
// If you change one of these three options in CMake, you need to update them here too!
#define RTXGI_DDGI_DEBUG_PROBE_INDEXING 0
#define RTXGI_DDGI_DEBUG_OCTAHEDRAL_INDEXING 0
#define RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING 0

#if RTXGI_DDGI_RESOURCE_MANAGEMENT && RTXGI_DDGI_BINDLESS_RESOURCES
    #error RTXGI SDK DDGI Managed Mode is not compatible with bindless resources!
#endif

// Shader resource registers and spaces (required when *not* using managed resources or shader reflection)
#if !RTXGI_DDGI_RESOURCE_MANAGEMENT && !RTXGI_DDGI_SHADER_REFLECTION
#if SPIRV
    #if RTXGI_DDGI_BINDLESS_RESOURCES                                    // Defines when using application's global root signature (bindless)
        #define VOLUME_CONSTS_REGISTER 5
        #define VOLUME_CONSTS_SPACE 0
        #define RWTEX2D_REGISTER 6
        #define RWTEX2D_SPACE 0
    #else
        #define VOLUME_CONSTS_REGISTER 0
        #define VOLUME_CONSTS_SPACE 0
        #define RAY_DATA_REGISTER 1
        #define RAY_DATA_SPACE 0
        #if RTXGI_DDGI_BLEND_RADIANCE
            #define OUTPUT_REGISTER 2
        #else
            #define OUTPUT_REGISTER 3
        #endif
        #define OUTPUT_SPACE 0
        #define PROBE_DATA_REGISTER 4
        #define PROBE_DATA_SPACE 0
    #endif
#else
    #define CONSTS_REGISTER b0
    #define CONSTS_SPACE space1
    #if RTXGI_DDGI_BINDLESS_RESOURCES                                    // Defines when using application's global root signature (bindless)
        #define VOLUME_CONSTS_REGISTER t5
        #define VOLUME_CONSTS_SPACE space0
        #define RWTEX2D_REGISTER u6
        #define RWTEX2D_SPACE space0
    #else                                                                // Defines when using the RTXGI SDK's root signature (not bindless)
        #define VOLUME_CONSTS_REGISTER t0
        #define VOLUME_CONSTS_SPACE space1
        #define RAY_DATA_REGISTER u0
        #define RAY_DATA_SPACE space1
        #if RTXGI_DDGI_BLEND_RADIANCE
            #define OUTPUT_REGISTER u1
        #else
            #define OUTPUT_REGISTER u2
        #endif
        #define OUTPUT_SPACE space1
        #define PROBE_DATA_REGISTER u3
        #define PROBE_DATA_SPACE space1
    #endif
#endif
#endif

