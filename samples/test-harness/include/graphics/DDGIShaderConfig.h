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

//-------------------------------------------------------------------------------------------------
// Required Defines: these must *match* the settings in CMake.
// If you change one of these options in CMake, you need to update them here too!

// Coordinate System
// 0: RTXGI_COORDINATE_SYSTEM_LEFT
// 1: RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
// 2: RTXGI_COORDINATE_SYSTEM_RIGHT
// 3: RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
#define RTXGI_COORDINATE_SYSTEM 2

// Use Shader Reflection?
// 0: no
// 1: yes
#define RTXGI_DDGI_SHADER_REFLECTION 0

// Bindless Resource implementation type
// 0: RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS
// 1: RTXGI_BINDLESS_TYPE_DESCRIPTOR_HEAP
#define RTXGI_BINDLESS_TYPE 0

// Should DDGI use bindless resources?
// 0: no
// 1: yes
#define RTXGI_DDGI_BINDLESS_RESOURCES 0

//-------------------------------------------------------------------------------------------------
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
#if __spirv__
    #if RTXGI_DDGI_BINDLESS_RESOURCES
        // Using the application's root signature (bindless resource arrays)
        #define RTXGI_PUSH_CONSTS_TYPE 2 
        #define RTXGI_DECLARE_PUSH_CONSTS 1
        #define RTXGI_PUSH_CONSTS_STRUCT_NAME GlobalConstants
        #define RTXGI_PUSH_CONSTS_VARIABLE_NAME GlobalConst
        #define RTXGI_PUSH_CONSTS_FIELD_DDGI_VOLUME_INDEX_NAME ddgi_volumeIndex
        #define RTXGI_PUSH_CONSTS_FIELD_DDGI_REDUCTION_INPUT_SIZE_X_NAME ddgi_reductionInputSizeX
        #define RTXGI_PUSH_CONSTS_FIELD_DDGI_REDUCTION_INPUT_SIZE_Y_NAME ddgi_reductionInputSizeY
        #define RTXGI_PUSH_CONSTS_FIELD_DDGI_REDUCTION_INPUT_SIZE_Z_NAME ddgi_reductionInputSizeZ
        #define VOLUME_CONSTS_REGISTER 5
        #define VOLUME_CONSTS_SPACE 0
        #define VOLUME_RESOURCES_REGISTER 6
        #define VOLUME_RESOURCES_SPACE 0
        #define RWTEX2DARRAY_REGISTER 9
        #define RWTEX2DARRAY_SPACE 0
    #else
        #define RTXGI_PUSH_CONSTS_TYPE 1   // use the SDK's push constants layout
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
        #define PROBE_VARIABILITY_REGISTER 5
        #define PROBE_VARIABILITY_AVERAGE_REGISTER 6
        #define PROBE_VARIABILITY_SPACE 0
    #endif
#else
    #define CONSTS_REGISTER b0
    #define CONSTS_SPACE space1
    #if RTXGI_DDGI_BINDLESS_RESOURCES && (RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS)
        // Using the application's root signature (bindless resource arrays)
        #define VOLUME_CONSTS_REGISTER t5
        #define VOLUME_CONSTS_SPACE space0
        #define RWTEX2DARRAY_REGISTER u6
        #define RWTEX2DARRAY_SPACE space1
    #else
        // Using the RTXGI SDK's root signature (not bindless)
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
        #define PROBE_VARIABILITY_REGISTER u4
        #define PROBE_VARIABILITY_AVERAGE_REGISTER u5
        #define PROBE_VARIABILITY_SPACE space1
    #endif
#endif
#endif
