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
# RTXGI Test Harness Project
# --------------------------------------

project(TestHarness)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Only 64-bit platforms are supported
if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4" )
    message(FATAL_ERROR "The RTXGI SDK requires a 64-bit toolset. GPU ray tracing does not support 32-bit platforms.")
endif()

file(GLOB TEST_HARNESS_CONFIG
    "config/cornell.ini"
    "config/furnace.ini"
    "config/multi-cornell.ini"
    "config/sponza.ini"
    "config/tunnel.ini"
    "config/two-rooms.ini"
)

file(GLOB TEST_HARNESS_SOURCE
    "include/Benchmark.h"
    "include/Caches.h"
    "include/Common.h"
    "include/Configs.h"
    "include/Geometry.h"
    "include/Graphics.h"
    "include/ImageCapture.h"
    "include/Inputs.h"
    "include/Instrumentation.h"
    "include/Scenes.h"
    "include/Shaders.h"
    "include/Textures.h"
    "include/Window.h"
    "src/Benchmark.cpp"
    "src/Caches.cpp"
    "src/Configs.cpp"
    "src/Geometry.cpp"
    "src/Inputs.cpp"
    "src/ImageCapture.cpp"
    "src/Instrumentation.cpp"
    "src/main.cpp"
    "src/Scenes.cpp"
    "src/Shaders.cpp"
    "src/Textures.cpp"
    "src/UI.cpp"
    "src/Window.cpp"
)

file(GLOB TEST_HARNESS_SOURCE_D3D12
    "include/Direct3D12.h"
    "src/Direct3D12.cpp"
)

file(GLOB TEST_HARNESS_SOURCE_VULKAN
    "include/Vulkan.h"
    "include/VulkanExtensions.h"
    "src/Vulkan.cpp"
    "src/VulkanExtensions.cpp"
)

file(GLOB TEST_HARNESS_GRAPHICS_INCLUDE
    "include/graphics/Composite.h"
    "include/graphics/DDGI.h"
    "include/graphics/DDGIDefines.h"
    "include/graphics/DDGIShaderConfig.h"
    "include/graphics/DDGIVisualizations.h"
    "include/graphics/GBuffer.h"
    "include/graphics/PathTracing.h"
    "include/graphics/RTAO.h"
    "include/graphics/Types.h"
    "include/graphics/UI.h"
)

file(GLOB TEST_HARNESS_GRAPHICS_INCLUDE_D3D12
    "include/graphics/Composite_D3D12.h"
    "include/graphics/DDGI_D3D12.h"
    "include/graphics/DDGIVisualizations_D3D12.h"
    "include/graphics/GBuffer_D3D12.h"
    "include/graphics/PathTracing_D3D12.h"
    "include/graphics/RTAO_D3D12.h"
    "include/graphics/UI_D3D12.h"
)

file(GLOB TEST_HARNESS_GRAPHICS_INCLUDE_VULKAN
    "include/graphics/Composite_VK.h"
    "include/graphics/DDGI_VK.h"
    "include/graphics/DDGIVisualizations_VK.h"
    "include/graphics/GBuffer_VK.h"
    "include/graphics/PathTracing_VK.h"
    "include/graphics/RTAO_VK.h"
    "include/graphics/UI_VK.h"
)

file(GLOB TEST_HARNESS_GRAPHICS_SOURCE
    "src/graphics/DDGI.cpp"
)

file(GLOB TEST_HARNESS_GRAPHICS_SOURCE_D3D12
    "src/graphics/Composite_D3D12.cpp"
    "src/graphics/DDGI_D3D12.cpp"
    "src/graphics/DDGIVisualizations_D3D12.cpp"
    "src/graphics/GBuffer_D3D12.cpp"
    "src/graphics/PathTracing_D3D12.cpp"
    "src/graphics/RTAO_D3D12.cpp"
    "src/graphics/UI_D3D12.cpp"
)

file(GLOB TEST_HARNESS_GRAPHICS_SOURCE_VULKAN
    "src/graphics/Composite_VK.cpp"
    "src/graphics/DDGI_VK.cpp"
    "src/graphics/DDGIVisualizations_VK.cpp"
    "src/graphics/GBuffer_VK.cpp"
    "src/graphics/PathTracing_VK.cpp"
    "src/graphics/RTAO_VK.cpp"
    "src/graphics/UI_VK.cpp"
)

file(GLOB TEST_HARNESS_SHADER_INCLUDE
    "shaders/include/Common.hlsl"
    "shaders/include/Descriptors.hlsl"
    "shaders/include/Lighting.hlsl"
    "shaders/include/Platform.hlsl"
    "shaders/include/Random.hlsl"
    "shaders/include/RayTracing.hlsl"
)

file(GLOB TEST_HARNESS_SHADER_SOURCE
    "shaders/AHS.hlsl"
    "shaders/CHS.hlsl"
    "shaders/Composite.hlsl"
    "shaders/GBufferRGS.hlsl"
    "shaders/IndirectCS.hlsl"
    "shaders/Miss.hlsl"
    "shaders/PathTraceRGS.hlsl"
    "shaders/RTAOFilterCS.hlsl"
    "shaders/RTAOTraceRGS.hlsl"
)

file(GLOB TEST_HARNESS_DDGI_SHADER_SOURCE
    "shaders/ddgi/ProbeTraceRGS.hlsl"
)

file(GLOB TEST_HARNESS_DDGIVIS_SHADER_SOURCE
    "shaders/ddgi/visualizations/Descriptors.hlsl"
    "shaders/ddgi/visualizations/ProbesRGS.hlsl"
    "shaders/ddgi/visualizations/ProbesCHS.hlsl"
    "shaders/ddgi/visualizations/ProbesMiss.hlsl"
    "shaders/ddgi/visualizations/ProbesUpdateCS.hlsl"
    "shaders/ddgi/visualizations/VolumeTexturesCS.hlsl"
)

###############################################################
# Third Party
###############################################################

file(GLOB THIRD_PARTY_DIRECTXTEX_INCLUDE
    "include/thirdparty/directxtex/DirectXTex.h"
)

file(GLOB THIRD_PARTY_DIRECTXTEX_SOURCE
    "src/thirdparty/directxtex/DirectXTexUtil.cpp"
    "src/thirdparty/directxtex/DirectXTexDDS.cpp"
    "src/thirdparty/directxtex/DirectXTexImage.cpp"
    "src/thirdparty/directxtex/DirectXTexConvert.cpp"
    "src/thirdparty/directxtex/DirectXTexMipmaps.cpp"
    "src/thirdparty/directxtex/DirectXTexCompress.cpp"
    "src/thirdparty/directxtex/BC.cpp"
    "src/thirdparty/directxtex/BC4BC5.cpp"
    "src/thirdparty/directxtex/BC6HBC7.cpp"
)

file(GLOB THIRD_PARTY_DIRECTXTEX_GPU_COMPRESS_SOURCE
    "src/thirdparty/directxtex/DirectXTexCompressGPU.cpp"
    "src/thirdparty/directxtex/BCDirectCompute.cpp"
)

file(GLOB THIRD_PARTY_IMGUI_SOURCE
    "../../thirdparty/imgui/imgui.cpp"
    "../../thirdparty/imgui/imgui_demo.cpp"
    "../../thirdparty/imgui/imgui_draw.cpp"
    "../../thirdparty/imgui/imgui_tables.cpp"
    "../../thirdparty/imgui/imgui_widgets.cpp"
    "../../thirdparty/imgui/backends/imgui_impl_glfw.cpp"
)

file(GLOB THIRD_PARTY_IMGUI_D3D12_SOURCE
    "../../thirdparty/imgui/backends/imgui_impl_dx12.cpp"
)

file(GLOB THIRD_PARTY_IMGUI_VULKAN_SOURCE
    "../../thirdparty/imgui/backends/imgui_impl_vulkan.cpp"
)

# Shaders do not participate in the build
set_source_files_properties(${TEST_HARNESS_SHADER_INCLUDE} PROPERTIES VS_TOOL_OVERRIDE "None")
set_source_files_properties(${TEST_HARNESS_SHADER_SOURCE} PROPERTIES VS_TOOL_OVERRIDE "None")
set_source_files_properties(${TEST_HARNESS_DDGI_SHADER_SOURCE} PROPERTIES VS_TOOL_OVERRIDE "None")
set_source_files_properties(${TEST_HARNESS_DDGIVIS_SHADER_SOURCE} PROPERTIES VS_TOOL_OVERRIDE "None")

# Test Harness options
option(RTXGISAMPLES_GFX_NAME_OBJECTS "Enable naming of graphics objects (for debugging)" ON)
option(RTXGISAMPLES_GFX_PERF_MARKERS "Enable GPU performance markers" ON)

# Test Harness DDGI options
option(RTXGISAMPLES_TEST_HARNESS_DDGI_BINDLESS_RESOURCES "Enable use of bindless resources in DDGI shaders" ON)
option(RTXGISAMPLES_TEST_HARNESS_DDGI_BLEND_SHARED_MEMORY "Enable use of shared memory in the DDGI blending passes" ON)
option(RTXGISAMPLES_TEST_HARNESS_DDGI_DEBUG_OCTAHEDRAL_INDEXING "Enable an octahedral texture indexing visualization (for debugging)" OFF)
option(RTXGISAMPLES_TEST_HARNESS_DDGI_DEBUG_BORDER_COPY_INDEXING "Enable a border texture copy indexing visualization (for debugging)" OFF)

# Setup the Test Harness options
function(SetupOptions ARG_TARGET_EXE)

    if(WIN32)
        # Setup the dxcompiler and dxil binary options
        set(RTXGISAMPLES_DXC_BINARIES "Packman" CACHE STRING "The dxcompiler and dxil binaries to use")
        set(RTXGISAMPLES_DXC_BINARIES "Win10 SDK" CACHE STRING "The dxcompiler and dxil binaries to use")
        set_property(CACHE RTXGISAMPLES_DXC_BINARIES PROPERTY STRINGS "Packman" "Win10 SDK")
    endif()

    # Set GFX object naming
    if(RTXGISAMPLES_GFX_NAME_OBJECTS)
        target_compile_definitions(${ARG_TARGET_EXE} PRIVATE GFX_NAME_OBJECTS)
    endif()

    # Set application perf markers
    if(RTXGISAMPLES_GFX_PERF_MARKERS)
        target_compile_definitions(${ARG_TARGET_EXE} PRIVATE GFX_PERF_MARKERS)
    endif()

    # Set resource access method
    if(RTXGISAMPLES_TEST_HARNESS_DDGI_BINDLESS_RESOURCES AND RTXGI_DDGI_RESOURCE_MANAGEMENT)
        message(FATAL_ERROR "Test Harness bindless resource mode and RTXGI SDK managed resource modes are not compatible and cannot both be enabled.")
    endif()
    target_compile_definitions(${ARG_TARGET_EXE} PRIVATE RTXGI_DDGI_BINDLESS_RESOURCES=$<BOOL:${RTXGISAMPLES_TEST_HARNESS_DDGI_BINDLESS_RESOURCES}>)

    # Set shared memory use in DDGI probe blending
    target_compile_definitions(${ARG_TARGET_EXE} PRIVATE RTXGI_DDGI_BLEND_SHARED_MEMORY=$<BOOL:${RTXGISAMPLES_TEST_HARNESS_DDGI_BLEND_SHARED_MEMORY}>)

    # Set debug options
    target_compile_definitions(${ARG_TARGET_EXE} PRIVATE RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING=$<BOOL:${RTXGISAMPLES_TEST_HARNESS_DDGI_DEBUG_BORDER_COPY_INDEXING}>)
    target_compile_definitions(${ARG_TARGET_EXE} PRIVATE RTXGI_DDGI_DEBUG_OCTAHEDRAL_INDEXING=$<BOOL:${RTXGISAMPLES_TEST_HARNESS_DDGI_DEBUG_OCTAHEDRAL_INDEXING}>)

endfunction()

# Include Paths
get_filename_component(PARENT_DIR ${PROJECT_SOURCE_DIR} DIRECTORY)
get_filename_component(ROOT_DIR ${PARENT_DIR} DIRECTORY)
set(DIRECTXMATH_INCLUDE "${ROOT_DIR}/thirdparty/DirectXMath/Inc")
set(GLFW_INCLUDE "${ROOT_DIR}/thirdparty/glfw/include")
set(IMGUI_INCLUDE "${ROOT_DIR}/thirdparty/imgui")
set(IMGUI_BACKENDS_INCLUDE "${ROOT_DIR}/thirdparty/imgui/backends")
set(TINYGLTF_INCLUDE "${ROOT_DIR}/thirdparty/tinygltf")

# ---- WINDOWS / D3D12 --------------------------------------------------------------------------------------

if(RTXGI_API_D3D12_ENABLE)
    # Set the executable name
    set(TARGET_EXE TestHarness-D3D12)

    # Add the executable
    add_executable(${TARGET_EXE} WIN32
        ${TEST_HARNESS_CONFIG}
        ${TEST_HARNESS_SOURCE}
        ${TEST_HARNESS_SOURCE_D3D12}
        ${TEST_HARNESS_GRAPHICS_INCLUDE}
        ${TEST_HARNESS_GRAPHICS_SOURCE}
        ${TEST_HARNESS_GRAPHICS_INCLUDE_D3D12}
        ${TEST_HARNESS_GRAPHICS_SOURCE_D3D12}
        ${TEST_HARNESS_SHADER_INCLUDE}
        ${TEST_HARNESS_SHADER_SOURCE}
        ${TEST_HARNESS_DDGI_SHADER_SOURCE}
        ${TEST_HARNESS_DDGIVIS_SHADER_SOURCE}
        ${THIRD_PARTY_DIRECTXTEX_INCLUDE}
        ${THIRD_PARTY_DIRECTXTEX_SOURCE}
        ${THIRD_PARTY_DIRECTXTEX_GPU_COMPRESS_SOURCE}
        ${THIRD_PARTY_DIRECTXTK_INCLUDE}
        ${THIRD_PARTY_IMGUI_SOURCE}
        ${THIRD_PARTY_IMGUI_D3D12_SOURCE}
    )

    # Add dependencies
    add_dependencies(${TARGET_EXE} glfw tinygltf)

    # Add the include directories
    target_include_directories(${TARGET_EXE} PRIVATE
        "include"
        "include/graphics"
        "include/thirdparty"
        "include/thirdparty/directxtex"
        "include/thirdparty/directxtk"
        "include/thirdparty/dxc"
        "include/thirdparty/stb"
        ${DIRECTXMATH_INCLUDE}
        ${GLFW_INCLUDE}
        ${IMGUI_INCLUDE}
        ${IMGUI_BACKENDS_INCLUDE}
        ${TINYGLTF_INCLUDE}
    )

    # Add statically linked libs
    target_link_libraries(${TARGET_EXE} RTXGI-D3D12 glfw d3d11 d3d12 dxgi)

    # Add common compiler definitions for exposed Test Harness options
    SetupOptions(${TARGET_EXE})

    # Add compiler definitions
    target_compile_definitions(${TARGET_EXE} PRIVATE API_D3D12)

    # Set the binary output directory
    set_target_properties(${TARGET_EXE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ../bin/d3d12/$<CONFIG>)

    # Set the default scene config
    set_target_properties(${TARGET_EXE} PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS ${CMAKE_SOURCE_DIR}/samples/test-harness/config/cornell.ini)

    # Add to the samples folder
    set_target_properties(${TARGET_EXE} PROPERTIES FOLDER "RTXGI Samples")
endif() # D3D12_ENABLE

# ---- WINDOWS, UNIX / Vulkan --------------------------------------------------------------------------------------

if(RTXGI_API_VULKAN_ENABLE)
    # Set the executable name
    set(TARGET_EXE TestHarness-VK)

    if(WIN32)
        # Add the executable
        add_executable(${TARGET_EXE} WIN32
            ${TEST_HARNESS_CONFIG}
            ${TEST_HARNESS_SOURCE}
            ${TEST_HARNESS_SOURCE_VULKAN}
            ${TEST_HARNESS_GRAPHICS_INCLUDE}
            ${TEST_HARNESS_GRAPHICS_SOURCE}
            ${TEST_HARNESS_GRAPHICS_INCLUDE_VULKAN}
            ${TEST_HARNESS_GRAPHICS_SOURCE_VULKAN}
            ${TEST_HARNESS_SHADER_INCLUDE}
            ${TEST_HARNESS_SHADER_SOURCE}
            ${TEST_HARNESS_DDGI_SHADER_SOURCE}
            ${TEST_HARNESS_DDGIVIS_SHADER_SOURCE}
            ${THIRD_PARTY_INCLUDE}
            ${THIRD_PARTY_DIRECTXTEX_INCLUDE}
            ${THIRD_PARTY_DIRECTXTEX_SOURCE}
            ${THIRD_PARTY_DIRECTXTEX_GPU_COMPRESS_SOURCE}
            ${THIRD_PARTY_IMGUI_SOURCE}
            ${THIRD_PARTY_IMGUI_VULKAN_SOURCE}
        )

        # Add the include directories
        target_include_directories(${TARGET_EXE} PRIVATE
            "include"
            "include/graphics"
            "include/thirdparty"
            "include/thirdparty/directxtex"
            "include/thirdparty/dxc"
            "include/thirdparty/stb"
            ${DIRECTXMATH_INCLUDE}
            ${GLFW_INCLUDE}
            ${IMGUI_INCLUDE}
            ${IMGUI_BACKENDS_INCLUDE}
            ${TINYGLTF_INCLUDE}
        )

        # Add VS filters
        source_group("Shaders" FILES ${TEST_HARNESS_SHADER_SOURCE})
        source_group("Shaders/Include" FILES ${TEST_HARNESS_SHADER_INCLUDE})
        source_group("Shaders/DDGI" FILES ${TEST_HARNESS_DDGI_SHADER_SOURCE})
        source_group("Shaders/DDGI/Visualizations" FILES ${TEST_HARNESS_DDGIVIS_SHADER_SOURCE})

    elseif(UNIX AND NOT APPLE)

        if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
            # DirectXTex not available on ARM64
            set(THIRD_PARTY_DIRECTXTEX_INCLUDE "")
            set(THIRD_PARTY_DIRECTXTEX_SOURCE "")
        endif()

        # Add the executable
        add_executable(${TARGET_EXE}
            ${TEST_HARNESS_CONFIG}
            ${TEST_HARNESS_SOURCE}
            ${TEST_HARNESS_SOURCE_VULKAN}
            ${TEST_HARNESS_GRAPHICS_INCLUDE}
            ${TEST_HARNESS_GRAPHICS_SOURCE}
            ${TEST_HARNESS_GRAPHICS_INCLUDE_VULKAN}
            ${TEST_HARNESS_GRAPHICS_SOURCE_VULKAN}
            ${TEST_HARNESS_SHADER_INCLUDE}
            ${TEST_HARNESS_SHADER_SOURCE}
            ${TEST_HARNESS_DDGI_SHADER_SOURCE}
            ${TEST_HARNESS_DDGIVIS_SHADER_SOURCE}
            ${THIRD_PARTY_INCLUDE}
            ${THIRD_PARTY_DIRECTXTEX_INCLUDE}
            ${THIRD_PARTY_DIRECTXTEX_SOURCE}
            ${THIRD_PARTY_IMGUI_SOURCE}
            ${THIRD_PARTY_IMGUI_VULKAN_SOURCE}
        )

        # Add the include directories
        target_include_directories(${TARGET_EXE} PRIVATE
            "include"
            "include/graphics"
            "include/thirdparty"
            "include/thirdparty/directx"
            "include/thirdparty/directxmath"
            "include/thirdparty/directxtex"
            "include/thirdparty/dxc"
            "include/thirdparty/stb"
            ${GLFW_INCLUDE}
            ${IMGUI_INCLUDE}
            ${IMGUI_BACKENDS_INCLUDE}
            ${TINYGLTF_INCLUDE}
        )

    endif()

    # Add dependencies
    add_dependencies(${TARGET_EXE} glfw tinygltf)

    # Add compiler definitions
    target_compile_definitions(${TARGET_EXE} PUBLIC API_VULKAN)

    # Add common compiler definitions for exposed RTXGI options
    SetupRTXGIOptions(${TARGET_EXE})

    # Add common compiler definitions for exposed Test Harness options
    SetupOptions(${TARGET_EXE})

    # Add statically linked libs
    if(WIN32)
        # Note: Even when targeting Vulkan, Windows uses D3D11 GPU-based texture compression with DirectXTex
        target_link_libraries(${TARGET_EXE} RTXGI-VK ${Vulkan_LIBRARY} glfw d3d11)
    elseif(UNIX AND NOT APPLE)
        # Note: UNIX can't use D3D11 GPU-based texture compression with DirectXTex
        target_link_libraries(${TARGET_EXE} RTXGI-VK -lglfw -lvulkan -ldl -lpthread -lX11 -lXrandr -lXi -lstdc++fs)
    endif()

    # Set the binary output directory
    set_target_properties(${TARGET_EXE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ../bin/vulkan/$<CONFIG>)

    # Set the default scene config
    set_target_properties(${TARGET_EXE} PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS ${CMAKE_SOURCE_DIR}/samples/test-harness/config/cornell.ini)

    # Add to the samples folder
    set_target_properties(${TARGET_EXE} PROPERTIES FOLDER "RTXGI Samples")
endif() # VULKAN_ENABLE

if(WIN32 AND MSVC)

    # Add VS filters
    source_group("Config" FILES ${TEST_HARNESS_CONFIG})

    # Headers
    source_group("Header Files/Graphics" FILES ${TEST_HARNESS_GRAPHICS_INCLUDE} ${TEST_HARNESS_GRAPHICS_INCLUDE_D3D12} ${TEST_HARNESS_GRAPHICS_INCLUDE_VULKAN})
    source_group("Header Files/Thirdparty" FILES ${THIRD_PARTY_INCLUDE})
    source_group("Header Files/Thirdparty/DirectXTex" FILES ${THIRD_PARTY_DIRECTXTEX_INCLUDE})

    # Source
    source_group("Source Files/Graphics" FILES ${TEST_HARNESS_GRAPHICS_SOURCE} ${TEST_HARNESS_GRAPHICS_SOURCE_D3D12} ${TEST_HARNESS_GRAPHICS_SOURCE_VULKAN})
    source_group("Source Files/Thirdparty/DirectXTex" FILES ${THIRD_PARTY_DIRECTXTEX_SOURCE} ${THIRD_PARTY_DIRECTXTEX_GPU_COMPRESS_SOURCE})
    source_group("Source Files/Thirdparty/ImGui" FILES ${THIRD_PARTY_IMGUI_SOURCE} ${THIRD_PARTY_IMGUI_D3D12_SOURCE} ${THIRD_PARTY_IMGUI_VULKAN_SOURCE})

    # Set arguments for Visual Studio Smart Command Line Arguments extension
    # https://github.com/MBulli/SmartCommandlineArgs
    configure_file("${ROOT_DIR}/samples/test-harness/.args" "${CMAKE_BINARY_DIR}/samples/test-harness/TestHarness-D3D12.args.json" COPYONLY)
    configure_file("${ROOT_DIR}/samples/test-harness/.args" "${CMAKE_BINARY_DIR}/samples/test-harness/TestHarness-VK.args.json" COPYONLY)
endif() # WIN32 and MSVC

# Set the dxc binaries location (Windows only)
if(WIN32)
    if(${RTXGISAMPLES_DXC_BINARIES} MATCHES "Packman")
        set(DXC_BINARY  ${ROOT_DIR}/dxc/bin/dxcompiler.dll)
        set(DXIL_BINARY ${ROOT_DIR}/dxc/bin/dxil.dll)
    elseif(${RTXGISAMPLES_DXC_BINARIES} MATCHES "Win10 SDK")
        set(DXC_BINARY  ${RTXGI_API_D3D12_DXIL_PATH}/dxcompiler.dll)
        set(DXIL_BINARY ${RTXGI_API_D3D12_DXIL_PATH}/dxil.dll)
    endif()
endif()

if(WIN32 AND RTXGI_API_D3D12_ENABLE)
    # Copy required DLLs next to the executable
    add_custom_command( TARGET TestHarness-D3D12 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DXIL_BINARY} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/d3d12/$<CONFIG>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DXC_BINARY} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/d3d12/$<CONFIG>
    )

    # Copy logo image to the working directories
    add_custom_command( TARGET TestHarness-D3D12 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ROOT_DIR}/docs/nvidia.jpg ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/d3d12/$<CONFIG>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ROOT_DIR}/docs/nvidia.jpg ${CMAKE_BINARY_DIR}/samples/test-harness/
    )
endif()

if(WIN32 AND RTXGI_API_VULKAN_ENABLE)
    # Copy required DLLs next to the executable
    add_custom_command( TARGET TestHarness-VK POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DXC_BINARY} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vulkan/$<CONFIG>
    )

    # Copy logo image to the working directories
    add_custom_command( TARGET TestHarness-VK POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ROOT_DIR}/docs/nvidia.jpg ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vulkan/$<CONFIG>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ROOT_DIR}/docs/nvidia.jpg ${CMAKE_BINARY_DIR}/samples/test-harness/
    )
endif()

if(UNIX AND RTXGI_API_VULKAN_ENABLE)
    if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64" OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
        # Copy required shared libraries next to the executable
        add_custom_command( TARGET TestHarness-VK POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ROOT_DIR}/dxc/lib/libdxcompiler.so ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vulkan/$<CONFIG>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ROOT_DIR}/dxc/lib/libdxcompiler.so ${CMAKE_BINARY_DIR}/samples/test-harness
        )

       # Copy logo image to the working directories
       add_custom_command( TARGET TestHarness-VK POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ROOT_DIR}/docs/nvidia.jpg ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/vulkan/$<CONFIG>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ROOT_DIR}/docs/nvidia.jpg ${CMAKE_BINARY_DIR}/samples/test-harness
       )
    endif()
endif()
