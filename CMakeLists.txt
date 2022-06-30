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

project(RTXGI)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "bin")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Add Vulkan headers on ARM since there is no official ARM Vulkan SDK
if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
    add_subdirectory(thirdparty/Vulkan-Headers)
    set(RTXGI_API_VULKAN_SDK "1")
    set(Vulkan_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/Vulkan-Headers/include/")
endif()

# SDK
add_subdirectory(rtxgi-sdk)

# Samples
option(RTXGI_BUILD_SAMPLES "Include the RTXGI sample application(s)" ON)
if(RTXGI_BUILD_SAMPLES)
    # GLFW3
    option(GLFW_BUILD_EXAMPLES "" OFF)
    option(GLFW_BUILD_TESTS "" OFF)
    option(GLFW_BUILD_DOCS "" OFF)
    option(GLFW_INSTALL "" OFF)
    add_subdirectory(thirdparty/glfw)
    set_target_properties(glfw PROPERTIES FOLDER "Thirdparty/GLFW3")
    set_target_properties(update_mappings PROPERTIES FOLDER "Thirdparty/GLFW3")

    # TinyGLTF
    option(TINYGLTF_BUILD_LOADER_EXAMPLE "" OFF)
    add_subdirectory(thirdparty/tinygltf)
    target_compile_definitions(tinygltf PUBLIC _CRT_SECURE_NO_WARNINGS) # suppress the sprintf CRT warnings
    set_target_properties(tinygltf PROPERTIES FOLDER "Thirdparty/")

    # Samples
    add_subdirectory(samples)

    # Set the default project. If D3D is an option, set it as default.
    if(RTXGI_API_D3D12_ENABLE)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT TestHarness-D3D12)
    elseif(NOT RTXGI_API_D3D12_ENABLE AND RTXGI_API_VULKAN_ENABLE)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT TestHarness-VK)
    endif()
endif()
