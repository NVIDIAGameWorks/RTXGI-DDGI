/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

// -------- MANAGED RESOURCES DEFINES -------------------------------------------------------------

// RTXGI_DDGI_RESOURCE_MANAGEMENT must be passed in as a define at shader compilation time.
// This define specifies if the shader resources are managed by the SDK (and not the application).
// Ex: RTXGI_DDGI_RESOURCE_MANAGEMENT [0|1]
#ifndef RTXGI_DDGI_RESOURCE_MANAGEMENT
    #error Required define RTXGI_DDGI_RESOURCE_MANAGEMENT is not defined for ProbeRelocationCS.hlsl!
#endif

// -------- SHADER REFLECTION DEFINES -------------------------------------------------------------

// RTXGI_DDGI_SHADER_REFLECTION must be passed in as a define at shader compilation time.
// This define specifies if the shader resources will be determined using shader reflection.
// Ex: RTXGI_DDGI_SHADER_REFLECTION [0|1]
#ifndef RTXGI_DDGI_SHADER_REFLECTION
    #error Required define RTXGI_DDGI_SHADER_REFLECTION is not defined for ProbeRelocationCS.hlsl!
#else
    #if !RTXGI_DDGI_SHADER_REFLECTION
        // REGISTERs AND SPACEs (SHADER REFLECTION DISABLED)

        // MANAGED RESOURCES DEFINES
        #if RTXGI_DDGI_RESOURCE_MANAGEMENT
            #ifdef __spirv__
                #define RTXGI_PUSH_CONSTS_TYPE 1
                #define VOLUME_CONSTS_REGISTER 0
                #define VOLUME_CONSTS_SPACE 0
                #define RAY_DATA_REGISTER 1
                #define RAY_DATA_SPACE 0
                #define PROBE_DATA_REGISTER 4
                #define PROBE_DATA_SPACE 0
            #else
                #define CONSTS_REGISTER b0
                #define CONSTS_SPACE space1
                #define VOLUME_CONSTS_REGISTER t0
                #define VOLUME_CONSTS_SPACE space1
                #define RAY_DATA_REGISTER u0
                #define RAY_DATA_SPACE space1
                #define PROBE_DATA_REGISTER u3
                #define PROBE_DATA_SPACE space1
            #endif
        #endif // RTXGI_DDGI_RESOURCE_MANAGEMENT

        // VOLUME_CONSTS_REGISTER and VOLUME_CONSTS_SPACE must be passed in as defines at shader compilation time *when not using reflection*.
        // These defines specify the shader register and space used for the DDGIVolumeDescGPUPacked structured buffer.
        // Ex: VOLUME_CONSTS_REGISTER t5
        // Ex: VOLUME_CONSTS_SPACE space0
        #ifndef VOLUME_CONSTS_REGISTER
            #error Required define VOLUME_CONSTS_REGISTER is not defined for ProbeRelocationCS.hlsl!
        #endif
        #ifndef VOLUME_CONSTS_SPACE
            #error Required define VOLUME_CONSTS_SPACE is not defined for ProbeRelocationCS.hlsl!
        #endif
    #endif // !RTXGI_DDGI_SHADER_REFLECTION
#endif // RTXGI_DDGI_SHADER_REFLECTION

// -------- RESOURCE BINDING DEFINES --------------------------------------------------------------

// RTXGI_DDGI_BINDLESS_RESOURCES must be passed in as a define at shader compilation time.
// This define specifies whether resources will be accessed bindlessly or not.
// Ex: RTXGI_DDGI_BINDLESS_RESOURCES [0|1]
#ifndef RTXGI_DDGI_BINDLESS_RESOURCES
    #error Required define RTXGI_DDGI_BINDLESS_RESOURCES is not defined for ProbeRelocationCS.hlsl!
#else
    #if !RTXGI_DDGI_SHADER_REFLECTION
        // Shader Reflection DISABLED
        #if RTXGI_DDGI_BINDLESS_RESOURCES
            // Bindless Resources ENABLED

            // RTXGI_BINDLESS_TYPE must be passed in as a define at shader compilation time when *bindless resources are used*.
            // This define specifies whether bindless resources will be accessed through bindless resource arrays or the (D3D12) descriptor heap.
            // Ex: RTXGI_BINDLESS_TYPE [RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS(0)|RTXGI_BINDLESS_TYPE_DESCRIPTOR_HEAP(1)]
            #ifndef RTXGI_BINDLESS_TYPE
                #error Required define RTXGI_BINDLESS_TYPE is not defined for ProbeBlendingCS.hlsl!
            #endif

            #if RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS
                // Bindless resources are accessed using SM6.5 and below style resource arrays

                // VOLUME_RESOURCES_REGISTER and VOLUME_RESOURCES_SPACE must be passed in as defines at shader compilation time
                // *not* using reflection and using bindless resource arrays.
                // These defines specify the shader register and space used for the DDGIVolumeResourceIndices structured buffer.
                // Ex: VOLUME_RESOURCES_REGISTER t6
                // Ex: VOLUME_RESOURCES_SPACE space0
                #ifndef VOLUME_RESOURCES_REGISTER
                    #error Required define VOLUME_RESOURCES_REGISTER is not defined for ProbeRelocationCS.hlsl!
                #endif
                #ifndef VOLUME_RESOURCES_REGISTER
                    #error Required define VOLUME_RESOURCES_REGISTER is not defined for ProbeRelocationCS.hlsl!
                #endif

                // RWTEX2DARRAY_REGISTER and RWTEX2DARRAY_SPACE must be passed in as defines at shader compilation time
                // *not* using reflection and using bindless resource arrays.
                // These defines specify the shader register and space of the RWTexture2DArray resource array that the DDGIVolume's
                // ray data, irradiance, distance, and probe data texture arrays are retrieved from bindlessly.
                // Ex: RWTEX2DARRAY_REGISTER u6
                // Ex: RWTEX2DARRAY_SPACE space1
                #ifndef RWTEX2DARRAY_REGISTER
                    #error Required bindless mode define RWTEX2DARRAY_REGISTER is not defined for ProbeBlendingCS.hlsl!
                #endif
                #ifndef RWTEX2DARRAY_SPACE
                    #error Required bindless mode define RWTEX2DARRAY_SPACE is not defined for ProbeBlendingCS.hlsl!
                #endif

            #endif // RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS

        #else // RTXGI_DDGI_BINDLESS_RESOURCES

            // Bindless Resources DISABLED (BOUND RESOURCE DEFINES)

            // RAY_DATA_REGISTER and RAY_DATA_SPACE must be passed in as defines at shader compilation time *when not using reflection*.
            // These defines specify the shader register and space used for the DDGIVolume ray data texture.
            // Ex: RAY_DATA_REGISTER u0
            // Ex: RAY_DATA_SPACE space1
            #ifndef RAY_DATA_REGISTER
                #error Required define RAY_DATA_REGISTER is not defined for ProbeRelocationCS.hlsl!
            #endif
            #ifndef RAY_DATA_SPACE
                #error Required define RAY_DATA_SPACE is not defined for ProbeRelocationCS.hlsl!
            #endif

            // PROBE_DATA_REGISTER and PROBE_DATA_SPACE must be passed in as defines at shader compilation time *when not using reflection*.
            // These defines specify the shader register and space used for the DDGIVolume probe data texture array.
            // Ex: PROBE_DATA_REGISTER u2
            // Ex: PROBE_DATA_SPACE space1
            #ifndef PROBE_DATA_REGISTER
                #error Required define PROBE_DATA_REGISTER is not defined for ProbeRelocationCS.hlsl!
            #endif
            #ifndef PROBE_DATA_SPACE
                #error Required define PROBE_DATA_SPACE is not defined for ProbeRelocationCS.hlsl!
            #endif

        #endif // RTXGI_DDGI_BINDLESS_RESOURCES
    #endif // !RTXGI_DDGI_SHADER_REFLECTION
#endif // RTXGI_DDGI_BINDLESS_RESOURCES

// ------------------------------------------------------------------------------------------------
