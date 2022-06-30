#
# Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.
#

# If on Windows, locate the Win10 SDK path if it is not already provided
if(WIN32 AND NOT RTXGI_WIN10_SDK)
    # Search for a suitable Win10 SDK
    get_filename_component(kit10_dir "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]" REALPATH)
    file(GLOB RTXGI_WIN10SDK_VERSIONS RELATIVE ${kit10_dir}/Include ${kit10_dir}/Include/10.*) # enumerate pre-release and not yet known release versions
    list(APPEND RTXGI_WIN10SDK_VERSIONS "10.0.17763.0") # enumerate well known release versions
    list(REMOVE_DUPLICATES RTXGI_WIN10SDK_VERSIONS)
    list(SORT RTXGI_WIN10SDK_VERSIONS)  # sort from low to high
    list(REVERSE RTXGI_WIN10SDK_VERSIONS) # reverse to start from high

    foreach(RTXGI_WIN10_SDK_VER ${RTXGI_WIN10SDK_VERSIONS})
        if(RTXGI_WIN10_SDK_VER VERSION_LESS "10.0.17763.0")
            continue()
        endif()
        set(RTXGI_API_D3D12_DXIL_PATH "${kit10_dir}/bin/${RTXGI_WIN10_SDK_VER}/x64" CACHE FILEPATH "" FORCE)
        message(STATUS "Found Suitable Windows 10 SDK: ${RTXGI_WIN10_SDK_VER}")
        break()
    endforeach()

    if(NOT RTXGI_API_D3D12_DXIL_PATH)
        message(WARNING "Failed to locate a suitable version of the Windows 10 SDK (10.0.17763 or greater).")
    endif()
endif()

# If on Windows or x86 Linux, locate the Vulkan SDK path if it is not already provided
if(NOT RTXGI_API_VULKAN_SDK AND NOT (${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64"))
    # Search for a suitable Vulkan SDK
    find_package(Vulkan "1.2.170")
    if(Vulkan_FOUND)
        string(REPLACE "Include" "" VK_SDK ${Vulkan_INCLUDE_DIR})
        set(RTXGI_API_VULKAN_SDK "1")
        message(STATUS "Found a suitable version of the Vulkan SDK: ${VK_SDK}")
    else()
        set(RTXGI_API_VULKAN_SDK "0")
    endif()
endif()

if(NOT RTXGI_API_VULKAN_SDK AND ${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
    set(RTXGI_API_VULKAN_SDK "1")
endif()
