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

#define RTXGI_DDGI_SDK_MANAGED_RESOURCES 0

#define RTXGI_DDGI_DEBUG_FORMAT_RADIANCE 0

#define RTXGI_DDGI_DEBUG_FORMAT_IRRADIANCE 0

#define RTXGI_DDGI_DEBUG_FORMAT_OFFSETS 0

// --- Probe Update ----------------------------------

#define RTXGI_DDGI_BLENDING_USE_SHARED_MEMORY 1

#define RTXGI_DDGI_DEBUG_PROBE_INDEXING 0

#define RTXGI_DDGI_DEBUG_OCTAHEDRAL_INDEXING 0

#define RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING 0

// --- Indirect Lighting -----------------------------

#define RTXGI_DDGI_COMPUTE_IRRADIANCE 1

#define RTXGI_DDGI_COMPUTE_IRRADIANCE_RECURSIVE 1

// --- Early Access Features -------------------------

#define RTXGI_DDGI_PROBE_RELOCATION 1

#define RTXGI_DDGI_PROBE_STATE_CLASSIFIER 1

// ---------------------------------------------------

#endif /* RTXGI_DDGI_VOLUME_DEFINES_H */
