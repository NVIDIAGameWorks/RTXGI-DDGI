#
# Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.
#

cmake_minimum_required(VERSION 3.15)

project(RTXGI)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "lib")

set(RTXGI_SDK_VERSION_MAJOR 1)
set(RTXGI_SDK_VERSION_MINOR 0)
set(RTXGI_SDK_VERSION_REVISION 0)

if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4" )
    message( FATAL_ERROR "The RTXGI SDK requires the Win10 64-bit toolset. Select a Visual Studio Win64 generator." )
endif()

file(GLOB SOURCE
	"include/rtxgi/Common.h"
	"include/rtxgi/Defines.h"
	"include/rtxgi/Math.h"
	"include/rtxgi/Random.h"
	"include/rtxgi/Types.h"
	"src/Math.cpp"
	"src/Random.cpp"
)

file(GLOB DDGI_HEADERS
	"include/rtxgi/ddgi/DDGIVolume.h"
	"include/rtxgi/ddgi/DDGIVolumeDefines.h"
	"include/rtxgi/ddgi/DDGIVolumeDescGPU.h"
)

file(GLOB DDGI_SOURCE
	"src/ddgi/DDGIVolume.cpp"
)

file(GLOB SHADER_SOURCE
	"shaders/Common.hlsl"
)

file(GLOB DDGI_SHADER_SOURCE
	"shaders/ddgi/Irradiance.hlsl"
	"shaders/ddgi/ProbeBlendingCS.hlsl"
	"shaders/ddgi/ProbeBorderUpdateCS.hlsl"
	"shaders/ddgi/ProbeCommon.hlsl"
    "shaders/ddgi/ProbeRelocationCS.hlsl"
    "shaders/ddgi/ProbeStateClassifierCS.hlsl"
)

# shaders do not participate in the build
set_source_files_properties(${SHADER_SOURCE} PROPERTIES VS_TOOL_OVERRIDE "None")
set_source_files_properties(${DDGI_SHADER_SOURCE} PROPERTIES VS_TOOL_OVERRIDE "None")

# setup the SDK library project
include_directories("include")
add_library(RTXGI STATIC ${SOURCE} ${DDGI_HEADERS} ${DDGI_SOURCE} ${SHADER_SOURCE} ${DDGI_SHADER_SOURCE})
target_include_directories(RTXGI PUBLIC "include" "include\\thirdparty")

# add visual studio filters
source_group("Header Files\\rtxgi\\ddgi" FILES ${DDGI_HEADERS})
source_group("Source Files\\ddgi" FILES ${DDGI_SOURCE})
source_group("Shaders" FILES ${SHADER_SOURCE})
source_group("Shaders\\ddgi" FILES ${DDGI_SHADER_SOURCE})

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT RTXGI)
