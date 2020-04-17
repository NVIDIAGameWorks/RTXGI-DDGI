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
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../bin")

project(RTXGISamples)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../rtxgi-sdk ${CMAKE_CURRENT_SOURCE_DIR}/../rtxgi-sdk/build)

# Locate Win 10 Kit & DXC
if (NOT WIN_SDK_BIN_DIR)

    # try to locate a suitable DXC compiler
    get_filename_component(kit10_dir "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]" REALPATH)
    file(GLOB W10SDK_VERSIONS RELATIVE ${kit10_dir}/Include ${kit10_dir}/Include/10.*)          # enumerate pre-release and not yet known release versions
    list(APPEND W10SDK_VERSIONS "10.0.17763.0")   # enumerate well known release versions
    list(REMOVE_DUPLICATES W10SDK_VERSIONS)
    list(SORT W10SDK_VERSIONS)  # sort from low to high
    list(REVERSE W10SDK_VERSIONS) # reverse to start from high

    foreach(W10SDK_VER ${W10SDK_VERSIONS})

        if (W10SDK_VER VERSION_LESS "10.0.17763.0")
            continue()
        endif()

        unset(DXC_EXE_CANDIDATE CACHE)
        set(WINSDK_PATHS "${kit10_dir}/bin/${W10SDK_VER}/x64" "C:/Program Files (x86)/Windows Kits/10/bin/${W10SDK_VER}/x64" "C:/Program Files/Windows Kits/10/bin/${W10SDK_VER}/x64")
        find_program(DXC_EXE_CANDIDATE dxc PATHS ${WINSDK_PATHS} NO_DEFAULT_PATH)
        if (EXISTS ${DXC_EXE_CANDIDATE} AND NOT DXC_EXECUTABLE)            
            set(DXC_EXECUTABLE ${DXC_EXE_CANDIDATE} CACHE FILEPATH "" FORCE)
            get_filename_component(WIN_SDK_BIN_DIR ${DXC_EXE_CANDIDATE} DIRECTORY CACHE)
            message(STATUS "Found Windows Kit: ${WIN_SDK_BIN_DIR} (found suitable version ${W10SDK_VER})")
        endif()
    endforeach()

    if (NOT DXC_EXECUTABLE)
        message(FATAL_ERROR "Failed to locate a suitable version of the Windows 10 Kit (10.0.17763 or greater required).")
    endif()

    unset(DXC_EXE_CANDIDATE CACHE)
    unset(WINSDK_PATHS)
endif()

add_subdirectory(test-harness)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT TestHarness)

