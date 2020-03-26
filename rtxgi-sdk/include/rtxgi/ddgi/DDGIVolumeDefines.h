/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RTXGI_DDGI_VOLUME_DEFINES_H
#define RTXGI_DDGI_VOLUME_DEFINES_H

// --- Resources -------------------------------------

#ifndef RTXGI_DDGI_SDK_MANAGED_RESOURCES
#define RTXGI_DDGI_SDK_MANAGED_RESOURCES 0
#endif

#ifndef RTXGI_DDGI_DEBUG_FORMAT_RADIANCE
#define RTXGI_DDGI_DEBUG_FORMAT_RADIANCE 0
#endif

#ifndef RTXGI_DDGI_DEBUG_FORMAT_IRRADIANCE
#define RTXGI_DDGI_DEBUG_FORMAT_IRRADIANCE 0
#endif

#ifndef RTXGI_DDGI_DEBUG_FORMAT_OFFSETS
#define RTXGI_DDGI_DEBUG_FORMAT_OFFSETS 0
#endif

// --- Probe Update ----------------------------------

#ifndef RTXGI_DDGI_BLENDING_USE_SHARED_MEMORY
#define RTXGI_DDGI_BLENDING_USE_SHARED_MEMORY 1
#endif

#ifndef RTXGI_DDGI_DEBUG_PROBE_INDEXING
#define RTXGI_DDGI_DEBUG_PROBE_INDEXING 0
#endif

#ifndef RTXGI_DDGI_DEBUG_OCTAHEDRAL_INDEXING
#define RTXGI_DDGI_DEBUG_OCTAHEDRAL_INDEXING 0
#endif

#ifndef RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING
#define RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING 0
#endif

// --- Indirect Lighting -----------------------------

#ifndef RTXGI_DDGI_COMPUTE_IRRADIANCE
#define RTXGI_DDGI_COMPUTE_IRRADIANCE 1
#endif

#ifndef RTXGI_DDGI_COMPUTE_IRRADIANCE_RECURSIVE
#define RTXGI_DDGI_COMPUTE_IRRADIANCE_RECURSIVE 1
#endif

// --- Early Access Features -------------------------

#ifndef RTXGI_DDGI_PROBE_RELOCATION
#define RTXGI_DDGI_PROBE_RELOCATION 1
#endif

#ifndef RTXGI_DDGI_PROBE_STATE_CLASSIFIER
#define RTXGI_DDGI_PROBE_STATE_CLASSIFIER 1
#endif

// ---------------------------------------------------

#endif /* RTXGI_DDGI_VOLUME_DEFINES_H */
