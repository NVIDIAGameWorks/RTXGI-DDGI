/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

// Shader defines
#define RTXGI_DDGI_SHADER_REFLECTION 0           // shader reflection
#define RTXGI_DDGI_DEBUG_PROBE_INDEXING 0        // blending probe index visualization

// Resource binding mode
#ifndef RTXGI_DDGI_BINDLESS_RESOURCES
#error RTXGI_DDGI_BINDLESS_RESOURCES is not defined!
#endif

// Blending shared memory mode
#ifndef RTXGI_DDGI_BLEND_SHARED_MEMORY
#error RTXGI_DDGI_BLEND_SHARED_MEMORY is not defined!
#endif

// Debug visualization modes
#ifndef RTXGI_DDGI_DEBUG_OCTAHEDRAL_INDEXING
#error RTXGI_DDGI_DEBUG_OCTAHEDRAL_INDEXING is not defined!
#endif

#ifndef RTXGI_DDGI_DEBUG_OCTAHEDRAL_INDEXING
#error RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING is not defined!
#endif
