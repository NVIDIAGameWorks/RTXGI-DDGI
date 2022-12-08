/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "graphics/DDGI.h"

using namespace rtxgi;

namespace Graphics
{
    namespace DDGI
    {

        //----------------------------------------------------------------------------------------------------------
        // DDGIVolume Shader Compilation
        //----------------------------------------------------------------------------------------------------------

        void AddCommonShaderDefines(Shaders::ShaderProgram& shader, const DDGIVolumeDesc& volumeDesc, bool spirv)
        {
            Shaders::AddDefine(shader, L"RTXGI_DDGI_RESOURCE_MANAGEMENT", std::to_wstring(RTXGI_DDGI_RESOURCE_MANAGEMENT));

        #if RTXGI_DDGI_USE_SHADER_CONFIG_FILE
            // Shader defines are specified in a config file
            Shaders::AddDefine(shader, L"RTXGI_DDGI_USE_SHADER_CONFIG_FILE", L"1");

            // Specify the include path to the DDGIShaderConfig.h file
            shader.includePath = L"../../../samples/test-harness/include/graphics";
        #else
            // Configuration options
            if(spirv) Shaders::AddDefine(shader, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
            else Shaders::AddDefine(shader, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE));

            Shaders::AddDefine(shader, L"RTXGI_COORDINATE_SYSTEM", std::to_wstring(RTXGI_COORDINATE_SYSTEM));
            Shaders::AddDefine(shader, L"RTXGI_DDGI_SHADER_REFLECTION", std::to_wstring(RTXGI_DDGI_SHADER_REFLECTION));
            Shaders::AddDefine(shader, L"RTXGI_DDGI_BINDLESS_RESOURCES", std::to_wstring(RTXGI_DDGI_BINDLESS_RESOURCES));

            #if !RTXGI_DDGI_RESOURCE_MANAGEMENT
                // Specify resource registers and spaces when the SDK is not managing resources (and when *not* using shader reflection)
                if (spirv)
                {
                #if RTXGI_DDGI_BINDLESS_RESOURCES
                    // Using the application's pipeline layout (bindless resource arrays)
                    Shaders::AddDefine(shader, L"RTXGI_PUSH_CONSTS_TYPE", L"2");                                         // use the application's push constants layout
                    Shaders::AddDefine(shader, L"RTXGI_DECLARE_PUSH_CONSTS", L"1");                                      // declare the push constants struct (it is not already declared elsewhere)
                    Shaders::AddDefine(shader, L"RTXGI_PUSH_CONSTS_STRUCT_NAME", L"GlobalConstants");                    // specify the struct name of the application's push constants
                    Shaders::AddDefine(shader, L"RTXGI_PUSH_CONSTS_VARIABLE_NAME", L"GlobalConst");                      // specify the variable name of the application's push constants
                    Shaders::AddDefine(shader, L"RTXGI_PUSH_CONSTS_FIELD_DDGI_VOLUME_INDEX_NAME", L"ddgi_volumeIndex");  // specify the name of the DDGIVolume index field in the application's push constants struct
                    Shaders::AddDefine(shader, L"RTXGI_PUSH_CONSTS_FIELD_DDGI_REDUCTION_INPUT_SIZE_X_NAME", L"ddgi_reductionInputSizeX");  // specify the name of the DDGIVolume reduction pass input size fields the application's push constants struct
                    Shaders::AddDefine(shader, L"RTXGI_PUSH_CONSTS_FIELD_DDGI_REDUCTION_INPUT_SIZE_Y_NAME", L"ddgi_reductionInputSizeY");
                    Shaders::AddDefine(shader, L"RTXGI_PUSH_CONSTS_FIELD_DDGI_REDUCTION_INPUT_SIZE_Z_NAME", L"ddgi_reductionInputSizeZ");
                    Shaders::AddDefine(shader, L"VOLUME_CONSTS_REGISTER", L"5");
                    Shaders::AddDefine(shader, L"VOLUME_CONSTS_SPACE", L"0");
                    Shaders::AddDefine(shader, L"VOLUME_RESOURCES_REGISTER", L"6");
                    Shaders::AddDefine(shader, L"VOLUME_RESOURCES_SPACE", L"0");
                    Shaders::AddDefine(shader, L"RWTEX2DARRAY_REGISTER", L"9");
                    Shaders::AddDefine(shader, L"RWTEX2DARRAY_SPACE", L"0");
                #else
                    // Using the RTXGI SDK's pipeline layout (not bindless)
                    Shaders::AddDefine(shader, L"RTXGI_PUSH_CONSTS_TYPE", L"1");   // use the SDK's push constants layout
                    Shaders::AddDefine(shader, L"VOLUME_CONSTS_REGISTER", L"0");
                    Shaders::AddDefine(shader, L"VOLUME_CONSTS_SPACE", L"0");
                    Shaders::AddDefine(shader, L"RAY_DATA_REGISTER", L"1");
                    Shaders::AddDefine(shader, L"RAY_DATA_SPACE", L"0");
                    //                           OUTPUT_REGISTER    // Note: this register differs for irradiance vs. distance and should be defined per-shader
                    Shaders::AddDefine(shader, L"OUTPUT_SPACE", L"0");
                    Shaders::AddDefine(shader, L"PROBE_DATA_REGISTER", L"4");
                    Shaders::AddDefine(shader, L"PROBE_DATA_SPACE", L"0");
                    Shaders::AddDefine(shader, L"PROBE_VARIABILITY_SPACE", L"0");
                    Shaders::AddDefine(shader, L"PROBE_VARIABILITY_REGISTER", L"5");
                    Shaders::AddDefine(shader, L"PROBE_VARIABILITY_AVERAGE_REGISTER", L"6");
                #endif
                }
                else // DXIL
                {
                    Shaders::AddDefine(shader, L"CONSTS_REGISTER", L"b0");
                    Shaders::AddDefine(shader, L"CONSTS_SPACE", L"space1");
                #if RTXGI_DDGI_BINDLESS_RESOURCES && (RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS)
                    // Using the application's root signature (bindless resource arrays)
                    Shaders::AddDefine(shader, L"VOLUME_CONSTS_REGISTER", L"t5");
                    Shaders::AddDefine(shader, L"VOLUME_CONSTS_SPACE", L"space0");
                    Shaders::AddDefine(shader, L"VOLUME_RESOURCES_REGISTER", L"t6");
                    Shaders::AddDefine(shader, L"VOLUME_RESOURCES_SPACE", L"space0");
                    Shaders::AddDefine(shader, L"RWTEX2DARRAY_REGISTER", L"u6");
                    Shaders::AddDefine(shader, L"RWTEX2DARRAY_SPACE", L"space1");
                #else
                    // Using the RTXGI SDK's root signature (not bindless)
                    Shaders::AddDefine(shader, L"VOLUME_CONSTS_REGISTER", L"t0");
                    Shaders::AddDefine(shader, L"VOLUME_CONSTS_SPACE", L"space1");
                    Shaders::AddDefine(shader, L"RAY_DATA_REGISTER", L"u0");
                    Shaders::AddDefine(shader, L"RAY_DATA_SPACE", L"space1");
                    //                           OUTPUT_REGISTER    // Note: this register differs for irradiance vs. distance and should be defined per-shader
                    Shaders::AddDefine(shader, L"OUTPUT_SPACE", L"space1");
                    Shaders::AddDefine(shader, L"PROBE_DATA_REGISTER", L"u3");
                    Shaders::AddDefine(shader, L"PROBE_DATA_SPACE", L"space1");
                    Shaders::AddDefine(shader, L"PROBE_VARIABILITY_SPACE", L"space1");
                    Shaders::AddDefine(shader, L"PROBE_VARIABILITY_REGISTER", L"u4");
                    Shaders::AddDefine(shader, L"PROBE_VARIABILITY_AVERAGE_REGISTER", L"u5");
                #endif
                }
            #endif

            // Optional Defines (including since we are compiling with warnings as errors)
            Shaders::AddDefine(shader, L"RTXGI_DDGI_DEBUG_PROBE_INDEXING", std::to_wstring(RTXGI_DDGI_DEBUG_PROBE_INDEXING));
            Shaders::AddDefine(shader, L"RTXGI_DDGI_DEBUG_OCTAHEDRAL_INDEXING", std::to_wstring(RTXGI_DDGI_DEBUG_OCTAHEDRAL_INDEXING));
            Shaders::AddDefine(shader, L"RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING", std::to_wstring(RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING));
        #endif
        }

        /**
         * Loads and compiles RTXGI SDK shaders used by a DDGIVolume.
         */
        bool CompileDDGIVolumeShaders(
            Globals& gfx,
            const DDGIVolumeDesc& volumeDesc,
            std::vector<Shaders::ShaderProgram>& volumeShaders,
            bool spirv,
            std::ofstream& log)
        {
            log << "\n\tLoading and compiling shaders for DDGIVolume: \"" << volumeDesc.name << "\"...";
            std::flush(log);

            std::wstring numRays = std::to_wstring(volumeDesc.probeNumRays);
            std::wstring numIrradianceTexels = std::to_wstring(volumeDesc.probeNumIrradianceTexels);
            std::wstring numIrradianceInteriorTexels = std::to_wstring(volumeDesc.probeNumIrradianceInteriorTexels);
            std::wstring numDistanceTexels = std::to_wstring(volumeDesc.probeNumDistanceTexels);
            std::wstring numDistanceInteriorTexels = std::to_wstring(volumeDesc.probeNumDistanceInteriorTexels);
            std::wstring waveLaneCount = std::to_wstring(gfx.features.waveLaneCount);

            std::wstring root = std::wstring(gfx.shaderCompiler.rtxgi.begin(), gfx.shaderCompiler.rtxgi.end());

            // Probe Blending (irradiance)
            {
                Shaders::ShaderProgram& shader = volumeShaders.emplace_back();
                shader.filepath = root + L"shaders/ddgi/ProbeBlendingCS.hlsl";
                shader.entryPoint = L"DDGIProbeBlendingCS";
                shader.targetProfile = L"cs_6_6";
                if(spirv) shader.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };

                // Add common shader defines
                AddCommonShaderDefines(shader, volumeDesc, spirv);

                // Add shader specific defines
                Shaders::AddDefine(shader, L"RTXGI_DDGI_BLEND_RADIANCE", L"1");
                Shaders::AddDefine(shader, L"RTXGI_DDGI_PROBE_NUM_TEXELS", numIrradianceTexels.c_str());
                Shaders::AddDefine(shader, L"RTXGI_DDGI_PROBE_NUM_INTERIOR_TEXELS", numIrradianceInteriorTexels.c_str());
                Shaders::AddDefine(shader, L"RTXGI_DDGI_BLEND_SHARED_MEMORY", std::to_wstring(RTXGI_DDGI_BLEND_SHARED_MEMORY));
            #if RTXGI_DDGI_BLEND_SHARED_MEMORY
                Shaders::AddDefine(shader, L"RTXGI_DDGI_BLEND_RAYS_PER_PROBE", numRays.c_str());
            #endif
                Shaders::AddDefine(shader, L"RTXGI_DDGI_BLEND_SCROLL_SHARED_MEMORY", std::to_wstring(volumeDesc.probeBlendingUseScrollSharedMemory));

            #if !RTXGI_DDGI_RESOURCE_MANAGEMENT && !RTXGI_DDGI_USE_SHADER_CONFIG_FILE && !RTXGI_DDGI_BINDLESS_RESOURCES
                if (spirv) Shaders::AddDefine(shader, L"OUTPUT_REGISTER", L"2"); // Note: this register differs for irradiance vs. distance
                else Shaders::AddDefine(shader, L"OUTPUT_REGISTER", L"u1");
            #endif

                // Load and compile the shader
                CHECK(Shaders::Compile(gfx.shaderCompiler, shader, true), "compile the RTXGI probe irradiance blending compute shader!\n", log);
            }

            // Probe Blending (distance)
            {
                Shaders::ShaderProgram& shader = volumeShaders.emplace_back();
                shader.filepath = root + L"shaders/ddgi/ProbeBlendingCS.hlsl";
                shader.entryPoint = L"DDGIProbeBlendingCS";
                shader.targetProfile = L"cs_6_6";
                if (spirv) shader.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };

                // Add common shader defines
                AddCommonShaderDefines(shader, volumeDesc, spirv);

                // Add shader specific defines
                Shaders::AddDefine(shader, L"RTXGI_DDGI_BLEND_RADIANCE", L"0");
                Shaders::AddDefine(shader, L"RTXGI_DDGI_PROBE_NUM_TEXELS", numDistanceTexels.c_str());
                Shaders::AddDefine(shader, L"RTXGI_DDGI_PROBE_NUM_INTERIOR_TEXELS", numDistanceInteriorTexels.c_str());
                Shaders::AddDefine(shader, L"RTXGI_DDGI_BLEND_SHARED_MEMORY", std::to_wstring(RTXGI_DDGI_BLEND_SHARED_MEMORY));
            #if RTXGI_DDGI_BLEND_SHARED_MEMORY
                Shaders::AddDefine(shader, L"RTXGI_DDGI_BLEND_RAYS_PER_PROBE", numRays.c_str());
            #endif
                Shaders::AddDefine(shader, L"RTXGI_DDGI_BLEND_SCROLL_SHARED_MEMORY", std::to_wstring(volumeDesc.probeBlendingUseScrollSharedMemory));

            #if !RTXGI_DDGI_RESOURCE_MANAGEMENT && !RTXGI_DDGI_USE_SHADER_CONFIG_FILE && !RTXGI_DDGI_BINDLESS_RESOURCES
                if (spirv) Shaders::AddDefine(shader, L"OUTPUT_REGISTER", L"3"); // Note: this register differs for irradiance vs. distance
                else Shaders::AddDefine(shader, L"OUTPUT_REGISTER", L"u2");
            #endif

                // Load and compile the shader
                CHECK(Shaders::Compile(gfx.shaderCompiler, shader, true), "load and compile the RTXGI probe distance blending compute shader!\n", log);
            }

            // Probe Relocation
            {
                // Update shader
                Shaders::ShaderProgram& shader = volumeShaders.emplace_back();
                shader.filepath = root + L"shaders/ddgi/ProbeRelocationCS.hlsl";
                shader.entryPoint = L"DDGIProbeRelocationCS";
                shader.targetProfile = L"cs_6_6";
                if (spirv) shader.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };

                // Add common shader defines
                AddCommonShaderDefines(shader, volumeDesc, spirv);

                CHECK(Shaders::Compile(gfx.shaderCompiler, shader, true), "load and compile the RTXGI probe relocation compute shader!\n", log);

                // Reset shader
                Shaders::ShaderProgram& shader2 = volumeShaders.emplace_back();
                shader2.filepath = root + L"shaders/ddgi/ProbeRelocationCS.hlsl";
                shader2.entryPoint = L"DDGIProbeRelocationResetCS";
                shader2.targetProfile = L"cs_6_6";
                if (spirv) shader2.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };

                // Add common shader defines
                AddCommonShaderDefines(shader2, volumeDesc, spirv);

                CHECK(Shaders::Compile(gfx.shaderCompiler, shader2, true), "load and compile the RTXGI probe relocation reset compute shader!\n", log);
            }

            // Probe Classification
            {
                // Update shader
                Shaders::ShaderProgram& shader = volumeShaders.emplace_back();
                shader.filepath = root + L"shaders/ddgi/ProbeClassificationCS.hlsl";
                shader.entryPoint = L"DDGIProbeClassificationCS";
                shader.targetProfile = L"cs_6_6";
                if (spirv) shader.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };

                // Add common shader defines
                AddCommonShaderDefines(shader, volumeDesc, spirv);

                CHECK(Shaders::Compile(gfx.shaderCompiler, shader, true), "load and compile the RTXGI probe classification compute shader!\n", log);

                // Reset shader
                Shaders::ShaderProgram& shader2 = volumeShaders.emplace_back();
                shader2.filepath = root + L"shaders/ddgi/ProbeClassificationCS.hlsl";
                shader2.entryPoint = L"DDGIProbeClassificationResetCS";
                shader2.targetProfile = L"cs_6_6";
                if (spirv) shader2.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };

                // Add common shader defines
                AddCommonShaderDefines(shader2, volumeDesc, spirv);

                CHECK(Shaders::Compile(gfx.shaderCompiler, shader2, true), "load and compile the RTXGI probe classification reset compute shader!\n", log);
            }

            // Probe variability reduction
            {
                Shaders::ShaderProgram& shader = volumeShaders.emplace_back();
                shader.filepath = root + L"shaders/ddgi/ReductionCS.hlsl";
                shader.entryPoint = L"DDGIReductionCS";
                shader.targetProfile = L"cs_6_6";
                if (spirv) shader.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };

                // Add common shader defines
                AddCommonShaderDefines(shader, volumeDesc, spirv);

                // Add shader specific defines
                Shaders::AddDefine(shader, L"RTXGI_DDGI_PROBE_NUM_INTERIOR_TEXELS", numIrradianceInteriorTexels.c_str());
                Shaders::AddDefine(shader, L"RTXGI_DDGI_WAVE_LANE_COUNT", waveLaneCount);
                CHECK(Shaders::Compile(gfx.shaderCompiler, shader, true), "load and compile the RTXGI reduction compute shader!\n", log);
            }

            // Extra reduction passes
            {
                Shaders::ShaderProgram& shader = volumeShaders.emplace_back();
                shader.filepath = root + L"shaders/ddgi/ReductionCS.hlsl";
                shader.entryPoint = L"DDGIExtraReductionCS";
                shader.targetProfile = L"cs_6_6";
                if (spirv) shader.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };

                // Add common shader defines
                AddCommonShaderDefines(shader, volumeDesc, spirv);

                // Add shader specific defines
                Shaders::AddDefine(shader, L"RTXGI_DDGI_PROBE_NUM_INTERIOR_TEXELS", numIrradianceInteriorTexels.c_str());
                Shaders::AddDefine(shader, L"RTXGI_DDGI_WAVE_LANE_COUNT", waveLaneCount);
                CHECK(Shaders::Compile(gfx.shaderCompiler, shader, true), "load and compile the RTXGI extra reduction compute shader!\n", log);
            }

            log << "done.\n";
            std::flush(log);

            return true;
        }

    } // namespace Graphics::DDGI
}
