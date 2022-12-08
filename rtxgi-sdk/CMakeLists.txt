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
set(RTXGI_SDK_VERSION_MINOR 3)
set(RTXGI_SDK_VERSION_REVISION 5)

# Only 64-bit platforms are supported
if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4" )
    message(FATAL_ERROR "The RTXGI SDK requires a 64-bit toolset. GPU ray tracing does not support 32-bit platforms.")
endif()

# Look for Windows and Vulkan SDKs
include("FindSDKs.cmake")

# Library type (static lib or dll)
option(RTXGI_STATIC_LIB "Generate a Static Library (*.lib)" OFF)

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

file(GLOB GFX_VULKAN_SOURCE
    "include/rtxgi/VulkanExtensions.h"
    "src/VulkanExtensions.cpp"
)

file(GLOB DDGI_HEADERS
    "include/rtxgi/ddgi/DDGIVolume.h"
    "include/rtxgi/ddgi/DDGIRootConstants.h"
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
    "shaders/ddgi/include/DDGIRootConstants.hlsl"
)

file(GLOB DDGI_SHADER_INCLUDE_VALIDATION
    "shaders/ddgi/include/validation/ProbeBlendingDefines.hlsl"
    "shaders/ddgi/include/validation/ProbeClassificationDefines.hlsl"
    "shaders/ddgi/include/validation/ProbeRelocationDefines.hlsl"
    "shaders/ddgi/include/validation/ReductionDefines.hlsl"
)

file(GLOB DDGI_SHADER_SOURCE
    "shaders/ddgi/Irradiance.hlsl"
    "shaders/ddgi/ProbeBlendingCS.hlsl"
    "shaders/ddgi/ProbeClassificationCS.hlsl"
    "shaders/ddgi/ProbeRelocationCS.hlsl"
    "shaders/ddgi/ReductionCS.hlsl"
)

# Shaders do not participate in the build
set_source_files_properties(${SHADER_SOURCE} PROPERTIES VS_TOOL_OVERRIDE "None")
set_source_files_properties(${DDGI_SHADER_INCLUDE} ${DDGI_SHADER_INCLUDE_VALIDATION} ${DDGI_SHADER_SOURCE} PROPERTIES VS_TOOL_OVERRIDE "None")

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

    if(RTXGI_STATIC_LIB)
        # Add the static library output target
        add_library(${TARGET_LIB} STATIC
            ${SOURCE}
            ${DDGI_HEADERS}
            ${DDGI_HEADERS_D3D12}
            ${DDGI_SOURCE}
            ${DDGI_SOURCE_D3D12}
            ${SHADER_SOURCE}
            ${DDGI_SHADER_INCLUDE}
            ${DDGI_SHADER_INCLUDE_VALIDATION}
            ${DDGI_SHADER_SOURCE})
    else()
        # Add the dynamic library output target
        add_library(${TARGET_LIB} SHARED
            ${SOURCE}
            ${DDGI_HEADERS}
            ${DDGI_HEADERS_D3D12}
            ${DDGI_SOURCE}
            ${DDGI_SOURCE_D3D12}
            ${SHADER_SOURCE}
            ${DDGI_SHADER_INCLUDE}
            ${DDGI_SHADER_INCLUDE_VALIDATION}
            ${DDGI_SHADER_SOURCE})
    endif()

    # Setup the library and its options
    SetupRTXGIOptions(${TARGET_LIB})

    # Add statically linked libs
    target_link_libraries(${TARGET_LIB} PRIVATE d3d12)

    # Set the lib's filename
    set_target_properties(${TARGET_LIB} PROPERTIES OUTPUT_NAME "rtxgi-d3d12")

    # Add the project to a folder
    set_target_properties(${TARGET_LIB} PROPERTIES FOLDER "RTXGI SDK")

    if(NOT RTXGI_STATIC_LIB)
        target_compile_definitions(${TARGET_LIB} PUBLIC RTXGI_EXPORT_DLL)

        set(RTXGI_SDK_D3D12_BINARY ${CMAKE_CURRENT_BINARY_DIR}/bin/$<CONFIG>/rtxgi-d3d12.dll PARENT_SCOPE)
        set(RTXGI_SDK_D3D12_SYMBOLS ${CMAKE_CURRENT_BINARY_DIR}/bin/$<CONFIG>/rtxgi-d3d12.pdb PARENT_SCOPE)
    endif()
endif()

# Setup the Vulkan library
if(RTXGI_API_VULKAN_ENABLE)
    # Set the target library's name
    set(TARGET_LIB RTXGI-VK)

    if(RTXGI_STATIC_LIB)
        # Add the static library output target
        add_library(${TARGET_LIB} STATIC
            ${SOURCE}
            ${GFX_VULKAN_SOURCE}
            ${DDGI_HEADERS}
            ${DDGI_HEADERS_VULKAN}
            ${DDGI_SOURCE}
            ${DDGI_SOURCE_VULKAN}
            ${SHADER_SOURCE}
            ${DDGI_SHADER_INCLUDE}
            ${DDGI_SHADER_INCLUDE_VALIDATION}
            ${DDGI_SHADER_SOURCE})
    else()
        # Add the dynamic library output target
        add_library(${TARGET_LIB} SHARED
            ${SOURCE}
            ${GFX_VULKAN_SOURCE}
            ${DDGI_HEADERS}
            ${DDGI_HEADERS_VULKAN}
            ${DDGI_SOURCE}
            ${DDGI_SOURCE_VULKAN}
            ${SHADER_SOURCE}
            ${DDGI_SHADER_INCLUDE}
            ${DDGI_SHADER_INCLUDE_VALIDATION}
            ${DDGI_SHADER_SOURCE})
    endif()

    # Setup the library and its options
    SetupRTXGIOptions(${TARGET_LIB})

    # Set compiler options
    if(NOT MSVC)
        target_compile_options(${TARGET_LIB} PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
    endif()

    # Add the include directories
    target_include_directories(${TARGET_LIB} PUBLIC ${Vulkan_INCLUDE_DIR})

    # Add statically linked libs
    if(WIN32)
        target_link_libraries(${TARGET_LIB} PRIVATE ${Vulkan_LIBRARY})
    elseif(UNIX AND NOT APPLE)
        target_link_libraries(${TARGET_LIB} PRIVATE -lvulkan)
    endif()

    # Set the lib's filename
    set_target_properties(${TARGET_LIB} PROPERTIES OUTPUT_NAME "rtxgi-vulkan")

    # Add the project to a folder
    set_target_properties(${TARGET_LIB} PROPERTIES FOLDER "RTXGI SDK")

    if(NOT RTXGI_STATIC_LIB)
        target_compile_definitions(${TARGET_LIB} PUBLIC RTXGI_EXPORT_DLL)

        if(WIN32)
            set(RTXGI_SDK_VK_BINARY ${CMAKE_CURRENT_BINARY_DIR}/bin/$<CONFIG>/rtxgi-vulkan.dll PARENT_SCOPE)
            set(RTXGI_SDK_VK_SYMBOLS ${CMAKE_CURRENT_BINARY_DIR}/bin/$<CONFIG>/rtxgi-vulkan.pdb PARENT_SCOPE)
        elseif(UNIX AND NOT APPLE)
            set(RTXGI_SDK_VK_BINARY ${CMAKE_CURRENT_BINARY_DIR}/librtxgi-vulkan.so PARENT_SCOPE)
        endif()
    endif()
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
    source_group("Shaders/ddgi/include/validation" FILES ${DDGI_SHADER_INCLUDE_VALIDATION})

    # Set the default project. If D3D12 is an option, set it as default.
    if(RTXGI_API_D3D12_ENABLE)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT RTXGI-D3D12)
    elseif(NOT RTXGI_API_D3D12_ENABLE AND RTXGI_API_VULKAN_ENABLE)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT RTXGI-VK)
    endif()
endif()