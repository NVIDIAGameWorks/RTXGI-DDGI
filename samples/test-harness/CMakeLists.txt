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

# --------------------------------------
# RTXGI Test Harness Project
# --------------------------------------

# Setup the project
project(TestHarness)

file(GLOB TEST_HARNESS_CONFIG
	"config/cornell.ini"
	"config/two-rooms.ini"
)

file(GLOB COMMON_SOURCE
	"src/common/D3D12.cpp"
	"src/common/DXR.cpp"
	"src/common/Shaders.cpp"
)

file(GLOB TEST_HARNESS_SOURCE
	"include/Common.h"
	"include/Config.h"
	"include/Deserialize.h"
	"include/Geometry.h"
	"include/Harness.h"
	"include/UI.h"
	"include/Input.h"
	"include/Lights.h"
	"include/Shaders.h"
	"include/Textures.h"
	"include/Visualization.h"
	"include/Window.h"
	"src/harness/Config.cpp"
	"src/harness/Deserialize.cpp"
	"src/harness/Geometry.cpp"
	"src/harness/Harness.cpp"
	"src/harness/UI.cpp"
	"src/harness/Input.cpp"
	"src/harness/main.cpp"
	"src/harness/Textures.cpp"
	"src/harness/Visualization.cpp"
	"src/harness/Window.cpp"
)

file(GLOB THIRD_PARTY_INCLUDE
	"include/thirdparty/stb_image.h"
)

file(GLOB THIRD_PARTY_DXC_INCLUDE
	"include/thirdparty/dxc/dxcapi.h"
	"include/thirdparty/dxc/dxcapi.use.h"
)

file(GLOB THIRD_PARTY_DIRECTXTK_INCLUDE
	"include/thirdparty/directxtk/Keyboard.h"
	"include/thirdparty/directxtk/Mouse.h"
)

file(GLOB THIRD_PARTY_DIRECTXTK_SOURCE
	"src/thirdparty/directxtk/Keyboard.cpp"
	"src/thirdparty/directxtk/Mouse.cpp"
)

file(GLOB THIRD_PARTY_IMGUI_INCLUDE
	"include/thirdparty/imgui/imconfig.h"
	"include/thirdparty/imgui/imgui.h"
	"include/thirdparty/imgui/imgui_impl_dx12.h"
	"include/thirdparty/imgui/imgui_impl_win32.h"
	"include/thirdparty/imgui/imgui_internal.h"
	"include/thirdparty/imgui/imstb_rectpack.h"
	"include/thirdparty/imgui/imstb_textedit.h"
	"include/thirdparty/imgui/imstb_truetype.h"
)

file(GLOB THIRD_PARTY_IMGUI_SOURCE
	"src/thirdparty/imgui/imgui.cpp"
	"src/thirdparty/imgui/imgui_demo.cpp"
	"src/thirdparty/imgui/imgui_draw.cpp"
	"src/thirdparty/imgui/imgui_impl_dx12.cpp"
	"src/thirdparty/imgui/imgui_impl_win32.cpp"
	"src/thirdparty/imgui/imgui_widgets.cpp"
)

file(GLOB TEST_HARNESS_SHADER_INCLUDE
	"shaders/include/Common.hlsl"
	"shaders/include/ComputeRS.hlsl"
	"shaders/include/LightingCommon.hlsl"
	"shaders/include/Random.hlsl"
	"shaders/include/RasterRS.hlsl"
	"shaders/include/RTCommon.hlsl"
	"shaders/include/RTGlobalRS.hlsl"
	"shaders/include/RTLocalRS.hlsl"
)

file(GLOB TEST_HARNESS_SHADER_SOURCE
	"shaders/AOFilterCS.hlsl"
	"shaders/AOTraceRGS.hlsl"
	"shaders/ClosestHit.hlsl"
	"shaders/Indirect.hlsl"	
	"shaders/Miss.hlsl"
	"shaders/PathTraceRGS.hlsl"
	"shaders/ProbeTraceRGS.hlsl"
	"shaders/PrimaryTraceRGS.hlsl"
	"shaders/VisDDGIBuffers.hlsl"
	"shaders/VisDDGIProbes.hlsl"
	"shaders/VisUpdateTLASCS.hlsl"
)

# Shaders do not participate in the build
set_source_files_properties(${TEST_HARNESS_SHADER_INCLUDE} PROPERTIES VS_TOOL_OVERRIDE "None")
set_source_files_properties(${TEST_HARNESS_SHADER_SOURCE} PROPERTIES VS_TOOL_OVERRIDE "None")

# Add the executable
add_executable(TestHarness WIN32 
${TEST_HARNESS_CONFIG}
${COMMON_SOURCE}
${TEST_HARNESS_SOURCE}
${TEST_HARNESS_SHADER_INCLUDE}
${TEST_HARNESS_SHADER_SOURCE}
${THIRD_PARTY_INCLUDE}
${THIRD_PARTY_DXC_INCLUDE}
${THIRD_PARTY_DIRECTXTK_INCLUDE}
${THIRD_PARTY_DIRECTXTK_SOURCE}
${THIRD_PARTY_IMGUI_INCLUDE}
${THIRD_PARTY_IMGUI_SOURCE}
)

# Add the include directories
target_include_directories(TestHarness PRIVATE 
"include"
"include/thirdparty/"
"include/thirdparty/dxc"
"include/thirdparty/directxtk"
"include/thirdparty/imgui"
)

# Add VS filters
source_group("Config" FILES ${TEST_HARNESS_CONFIG})

source_group("Header Files\\Thirdparty" FILES ${THIRD_PARTY_INCLUDE})
source_group("Header Files\\Thirdparty\\DXC" FILES ${THIRD_PARTY_DXC_INCLUDE})
source_group("Header Files\\Thirdparty\\DirectXTK" FILES ${THIRD_PARTY_DIRECTXTK_INCLUDE})
source_group("Header Files\\Thirdparty\\ImGui" FILES ${THIRD_PARTY_IMGUI_INCLUDE})

source_group("Source Files\\Common" FILES ${COMMON_SOURCE})
source_group("Source Files\\Thirdparty\\DirectXTK" FILES ${THIRD_PARTY_DIRECTXTK_SOURCE})
source_group("Source Files\\Thirdparty\\ImGui" FILES ${THIRD_PARTY_IMGUI_SOURCE})

source_group("Shaders" FILES ${TEST_HARNESS_SHADER_SOURCE})
source_group("Shaders\\Include" FILES ${TEST_HARNESS_SHADER_INCLUDE})

# Add statically linked libs
target_link_libraries(TestHarness d3d12 dxgi RTXGI ..\\..\\..\\rtxgi-sdk\\lib\\WinPixEventRuntime)

# Copy some files into the bin directory
add_custom_command( TARGET TestHarness POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_if_different ${WIN_SDK_BIN_DIR}/dxcompiler.dll ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>
COMMAND ${CMAKE_COMMAND} -E copy_if_different ${WIN_SDK_BIN_DIR}/dxil.dll ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>
COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}../../rtxgi-sdk/bin/WinPixEventRuntime.dll ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>
COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}../../docs/nvidia.ico ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>
)

# Set the default project in VS
set_target_properties(TestHarness PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "../../test-harness/config/two-rooms.ini")
