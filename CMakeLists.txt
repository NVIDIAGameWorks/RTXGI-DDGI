#
# Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.
#

cmake_minimum_required(VERSION 3.10)

# --------------------------------------
# RTXGI SDK Project
# --------------------------------------

project(RTXGI)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "lib")

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /MP")
endif()

set(RTXGI_SDK_VERSION_MAJOR 1)
set(RTXGI_SDK_VERSION_MINOR 2)
set(RTXGI_SDK_VERSION_REVISION 11)

# Only 64-bit platforms are supported
if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4" )
    message(FATAL_ERROR "The RTXGI SDK requires a 64-bit toolset. GPU ray tracing does not support 32-bit platforms.")
endif()

# Look for Windows and Vulkan SDKs
include("FindSDKs.cmake")

# D3D12, only available on Windows
if(WIN32 AND RTXGI_API_D3D12_DXIL_PATH)
    option(RTXGI_API_D3D12_ENABLE "Enable D3D12 support" ON)
endif()

# Vulkan
if(RTXGI_API_VULKAN_SDK)
    option(RTXGI_API_VULKAN_ENABLE "Enable Vulkan support" ON)
    if(NOT Vulkan_INCLUDE_DIR)
        set(RTXGI_API_VULKAN_HEADERS_INCLUDE "" CACHE STRING "Path to the Vulkan Headers include directory.")
        if(NOT RTXGI_API_VULKAN_HEADERS_INCLUDE STREQUAL "")
            set(Vulkan_INCLUDE_DIR "${RTXGI_API_VULKAN_HEADERS_INCLUDE}")
        endif()
    endif()
endif()

# RTXGI Coordinate System
set(RTXGI_COORDINATE_SYSTEM "Right Hand, Y-Up" CACHE STRING "The 3D coordinate system to use")
set(RTXGI_COORDINATE_SYSTEM "Right Hand, Z-Up" CACHE STRING "The 3D coordinate system to use")
set(RTXGI_COORDINATE_SYSTEM "Left Hand, Y-Up" CACHE STRING "The 3D coordinate system to use")
set(RTXGI_COORDINATE_SYSTEM "Left Hand, Z-Up" CACHE STRING "The 3D coordinate system to use")
set_property(CACHE RTXGI_COORDINATE_SYSTEM PROPERTY STRINGS "Right Hand, Y-Up" "Right Hand, Z-Up" "Left Hand, Y-Up" "Left Hand, Z-Up")

# RTXGI features
option(RTXGI_GFX_NAME_OBJECTS "Enable naming of graphics objects (for debugging)" ON)

# RTXGI DDGI features
option(RTXGI_DDGI_RESOURCE_MANAGEMENT "Enable SDK resource management" OFF)
option(RTXGI_DDGI_USE_SHADER_CONFIG_FILE "Enable using a config file to specify shader defines" OFF)

file(GLOB SOURCE
    "include/rtxgi/Common.h"
    "include/rtxgi/Defines.h"
    "include/rtxgi/Math.h"
    "include/rtxgi/Types.h"
    "src/Math.cpp"
)

file(GLOB DDGI_HEADERS
    "include/rtxgi/ddgi/DDGIVolume.h"
    "include/rtxgi/ddgi/DDGIConstants.h"
    "include/rtxgi/ddgi/DDGIVolumeDescGPU.h"
)

file(GLOB DDGI_HEADERS_D3D12
    "include/rtxgi/ddgi/gfx/DDGIVolume_D3D12.h"
)

file(GLOB DDGI_HEADERS_VULKAN
    "include/rtxgi/ddgi/gfx/DDGIVolume_VK.h"
)

file(GLOB DDGI_SOURCE
    "src/ddgi/DDGIVolume.cpp"
)

file(GLOB DDGI_SOURCE_D3D12
    "src/ddgi/gfx/DDGIVolume_D3D12.cpp"
)

file(GLOB DDGI_SOURCE_VULKAN
    "src/ddgi/gfx/DDGIVolume_VK.cpp"
)

file(GLOB SHADER_SOURCE
    "shaders/Common.hlsl"
    "shaders/Platform.hlsl"
)

file(GLOB DDGI_SHADER_INCLUDE
    "shaders/ddgi/include/Common.hlsl"
    "shaders/ddgi/include/ProbeCommon.hlsl"
    "shaders/ddgi/include/ProbeDataCommon.hlsl"
    "shaders/ddgi/include/ProbeIndexing.hlsl"
    "shaders/ddgi/include/ProbeOctahedral.hlsl"
    "shaders/ddgi/include/ProbeRayCommon.hlsl"
)

file(GLOB DDGI_SHADER_SOURCE
    "shaders/ddgi/Irradiance.hlsl"
    "shaders/ddgi/ProbeBlendingCS.hlsl"
    "shaders/ddgi/ProbeBorderUpdateCS.hlsl"
    "shaders/ddgi/ProbeClassificationCS.hlsl"
    "shaders/ddgi/ProbeRelocationCS.hlsl"
)

# Shaders do not participate in the build
set_source_files_properties(${SHADER_SOURCE} PROPERTIES VS_TOOL_OVERRIDE "None")
set_source_files_properties(${DDGI_SHADER_INCLUDE} ${DDGI_SHADER_SOURCE} PROPERTIES VS_TOOL_OVERRIDE "None")

# Setup the RTXGI library target and options
function(SetupRTXGIOptions ARG_TARGET_LIB )
    # Add include directories
    target_include_directories(${ARG_TARGET_LIB} PUBLIC "include")

    # Set Coordinate System
    if(${RTXGI_COORDINATE_SYSTEM} MATCHES "Left Hand, Y-Up")
        target_compile_definitions(${ARG_TARGET_LIB} PUBLIC RTXGI_COORDINATE_SYSTEM=0)
    elseif(${RTXGI_COORDINATE_SYSTEM} MATCHES "Left Hand, Z-Up")
        target_compile_definitions(${ARG_TARGET_LIB} PUBLIC RTXGI_COORDINATE_SYSTEM=1)
    elseif(${RTXGI_COORDINATE_SYSTEM} MATCHES "Right Hand, Y-Up")
        target_compile_definitions(${ARG_TARGET_LIB} PUBLIC RTXGI_COORDINATE_SYSTEM=2)
    elseif(${RTXGI_COORDINATE_SYSTEM} MATCHES "Right Hand, Z-Up")
        target_compile_definitions(${ARG_TARGET_LIB} PUBLIC RTXGI_COORDINATE_SYSTEM=3)
    else()
        # Default to right hand, y-up (if they write in some unsupported option)
        target_compile_definitions(${ARG_TARGET_LIB} PUBLIC RTXGI_COORDINATE_SYSTEM=2)
    endif()

    # Set GFX Object Naming
    if(RTXGI_GFX_NAME_OBJECTS)
        target_compile_definitions(${ARG_TARGET_LIB} PUBLIC RTXGI_GFX_NAME_OBJECTS)
    endif()

    # Set Resource Management Mode
    target_compile_definitions(${ARG_TARGET_LIB} PUBLIC RTXGI_DDGI_RESOURCE_MANAGEMENT=$<BOOL:${RTXGI_DDGI_RESOURCE_MANAGEMENT}>)

    # Set config file use
    target_compile_definitions(${ARG_TARGET_LIB} PUBLIC RTXGI_DDGI_USE_SHADER_CONFIG_FILE=$<BOOL:${RTXGI_DDGI_USE_SHADER_CONFIG_FILE}>)

endfunction()

# Setup the D3D12 library
if(WIN32 AND RTXGI_API_D3D12_ENABLE)
    # Set the target library's name
    set(TARGET_LIB RTXGI-D3D12)

    # Add the output lib target
    add_library(${TARGET_LIB} STATIC ${SOURCE} ${DDGI_HEADERS} ${DDGI_HEADERS_D3D12} ${DDGI_SOURCE} ${DDGI_SOURCE_D3D12} ${SHADER_SOURCE} ${DDGI_SHADER_INCLUDE} ${DDGI_SHADER_SOURCE})

    # Setup the library and its options
    SetupRTXGIOptions(${TARGET_LIB})

    # Add the project to a folder
    set_target_properties(${TARGET_LIB} PROPERTIES FOLDER "RTXGI SDK")
endif()

# Setup the Vulkan library
if(RTXGI_API_VULKAN_ENABLE)
    # Set the target library's name
    set(TARGET_LIB RTXGI-VK)

    # Add the output lib target
    add_library(${TARGET_LIB} STATIC ${SOURCE} ${DDGI_HEADERS} ${DDGI_HEADERS_VULKAN} ${DDGI_SOURCE} ${DDGI_SOURCE_VULKAN} ${SHADER_SOURCE} ${DDGI_SHADER_INCLUDE} ${DDGI_SHADER_SOURCE})

    # Setup the library and its options
    SetupRTXGIOptions(${TARGET_LIB})

    # Set compiler options
    if(NOT MSVC)
        target_compile_options(${TARGET_LIB} PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
    endif()

    # Add the include directories
    target_include_directories(${TARGET_LIB} PUBLIC ${Vulkan_INCLUDE_DIR})

    # Add the project to a folder
    set_target_properties(${TARGET_LIB} PROPERTIES FOLDER "RTXGI SDK")
endif()

if(WIN32)
    # Add Visual Studio filters
    source_group("Header Files/rtxgi/ddgi" FILES ${DDGI_HEADERS})
    source_group("Header Files/rtxgi/ddgi/gfx" FILES ${DDGI_HEADERS_D3D12} ${DDGI_HEADERS_VULKAN})
    source_group("Source Files/ddgi" FILES ${DDGI_SOURCE})
    source_group("Source Files/ddgi/gfx" FILES ${DDGI_SOURCE_D3D12} ${DDGI_SOURCE_VULKAN})
    source_group("Shaders" FILES ${SHADER_SOURCE})
    source_group("Shaders/ddgi" FILES ${DDGI_SHADER_SOURCE})
    source_group("Shaders/ddgi/include" FILES ${DDGI_SHADER_INCLUDE})

    # Set the default project. If D3D12 is an option, set it as default.
    if(RTXGI_API_D3D12_ENABLE)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT RTXGI-D3D12)
    elseif(NOT RTXGI_API_D3D12_ENABLE AND RTXGI_API_VULKAN_ENABLE)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT RTXGI-VK)
    endif()
endif()