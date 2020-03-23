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

namespace rtxgi
{
    struct RTXGI_VERSION
    {
        static const int major = 1;
        static const int minor = 0;
        static const int revision = 0;

        inline static const char* getVersionString()
        {
            return "1.00.00";
        }
    };

    #define RTXGI_SAFE_RELEASE( x ) { if ( x ) { x->Release(); x = NULL; } }
    #define RTXGI_SAFE_DELETE( x ) { if( x ) delete x; x = NULL; }
    #define RTXGI_SAFE_DELETE_ARRAY( x ) { if( x ) delete[] x; x = NULL; }
    #define RTXGI_ALIGN(_alignment, _val) (((_val + _alignment - 1) / _alignment) * _alignment)

    /*
    * ERTXGIStatus
    * An enumeration for SDK status return codes.
    */
    enum ERTXGIStatus
    {
        OK,
        ERROR_DDGI_INVALID_PROBE_COUNTS,
        ERROR_DDGI_INVALID_RESOURCE_DESCRIPTOR_HEAP,
        ERROR_DDGI_INVALID_RESOURCE_CONSTANT_BUFFER,
        ERROR_DDGI_MAP_FAILURE_CONSTANT_BUFFER,

        // --- DDGI Status Codes -----------------------------------------

        // SDK Managed Resources
        ERROR_DDGI_INVALID_D3D_DEVICE,
        ERROR_DDGI_INVALID_PROBE_RADIANCE_BLENDING_BYTECODE,
        ERROR_DDGI_INVALID_PROBE_DISTANCE_BLENDING_BYTECODE,
        ERROR_DDGI_INVALID_PROBE_BORDER_ROW_BYTECODE,
        ERROR_DDGI_INVALID_PROBE_BORDER_COLUMN_BYTECODE,
        ERROR_DDGI_INVALID_PROBE_RELOCATION_BYTECODE,
        ERROR_DDGI_INVALID_PROBE_STATE_CLASSIFIER_BYTECODE,
        ERROR_DDGI_INVALID_PROBE_STATE_CLASSIFIER_ACTIVATE_ALL_BYTECODE,
        ERROR_DDGI_ALLOCATE_FAILURE_RT_RADIANCE_TEXTURE,
        ERROR_DDGI_ALLOCATE_FAILURE_IRRADIANCE_TEXTURE,
        ERROR_DDGI_ALLOCATE_FAILURE_DISTANCE_TEXTURE,
        ERROR_DDGI_ALLOCATE_FAILURE_OFFSETS_TEXTURE,
        ERROR_DDGI_ALLOCATE_FAILURE_STATES_TEXTURE,
        ERROR_DDGI_CREATE_FAILURE_ROOT_SIGNATURE,
        ERROR_DDGI_CREATE_FAILURE_COMPUTE_PSO,

        // Application Managed Resources
        ERROR_DDGI_INVALID_RESOURCE_ROOT_SIGNATURE,
        ERROR_DDGI_INVALID_RESOURCE_RADIANCE_BLENDING_PSO,
        ERROR_DDGI_INVALID_RESOURCE_DISTANCE_BLENDING_PSO,
        ERROR_DDGI_INVALID_RESOURCE_BORDER_ROW_PSO,
        ERROR_DDGI_INVALID_RESOURCE_BORDER_COLUMN_PSO,
        ERROR_DDGI_INVALID_RESOURCE_PROBE_RELOCATION_PSO,
        ERROR_DDGI_INVALID_RESOURCE_PROBE_STATE_CLASSIFIER_PSO,
        ERROR_DDGI_INVALID_RESOURCE_PROBE_STATE_CLASSIFIER_ACTIVATE_ALL_PSO,
        ERROR_DDGI_INVALID_RESOURCE_RT_RADIANCE_TEXTURE,
        ERROR_DDGI_INVALID_RESOURCE_IRRADIANCE_TEXTURE,
        ERROR_DDGI_INVALID_RESOURCE_DISTANCE_TEXTURE,
        ERROR_DDGI_INVALID_RESOURCE_OFFSETS_TEXTURE,
        ERROR_DDGI_INVALID_RESOURCE_STATES_TEXTURE,

        // --- DDGI Status Codes -----------------------------------------

    };

}
