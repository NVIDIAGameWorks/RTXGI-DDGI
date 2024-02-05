#
# Copyright (c) 2019-2023, NVIDIA CORPORATION.  All rights reserved.
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

# Helper to download and unzip a package from a URL
# Uses a zero-length file to identify the version of the package
function(CheckAndDownloadPackage NAME VERSION LOCAL_PATH URL)
    # Do we already have the correct version?
    if(NOT EXISTS ${LOCAL_PATH}/${VERSION}.ver)
        # Was there a previous version that we need to delete?
        if(EXISTS ${LOCAL_PATH})
            message(STATUS "Deleting old " ${NAME})
            file(REMOVE_RECURSE ${LOCAL_PATH})
        endif()
        message(STATUS "Obtaining " ${NAME} " " ${VERSION})
        file(DOWNLOAD ${URL} ${LOCAL_PATH}.zip)
        message(STATUS "Extracting " ${NAME})
        file(ARCHIVE_EXTRACT INPUT ${LOCAL_PATH}.zip DESTINATION ${LOCAL_PATH})
        file(REMOVE ${LOCAL_PATH}.zip)
        # Create an empty file so we know which version we have
        file(WRITE ${LOCAL_PATH}/${VERSION}.ver)
    endif()
endfunction()

# Add Vulkan headers on ARM since there is no official ARM Vulkan SDK
if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
    add_subdirectory(thirdparty/Vulkan-Headers)
    set(RTXGI_API_VULKAN_SDK "1")
    set(Vulkan_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/Vulkan-Headers/include/")
endif()

# Download D3D Agility SDK and DXC binaries
if(WIN32)
    CheckAndDownloadPackage("Agility SDK" "v1.606.3" ${CMAKE_CURRENT_SOURCE_DIR}/external/agilitysdk https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/1.606.3)
    CheckAndDownloadPackage("DXC" "v1.7.2308" ${CMAKE_CURRENT_SOURCE_DIR}/external/dxc https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.7.2308/dxc_2023_08_14.zip)
elseif(UNIX AND NOT APPLE)
    CheckAndDownloadPackage("DXC" "v1.7.2308" ${CMAKE_CURRENT_SOURCE_DIR}/external/dxc https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.7.2308/linux_dxc_2023_08_14.x86_64.tar.gz)
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
