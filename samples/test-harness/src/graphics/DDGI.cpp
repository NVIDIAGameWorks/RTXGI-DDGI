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
            Shaders::AddDefine(shader, L"RTXGI_COORDINATE_SYSTEM", std::to_wstring(RTXGI_COORDINATE_SYSTEM));
            Shaders::AddDefine(shader, L"RTXGI_DDGI_SHADER_REFLECTION", std::to_wstring(RTXGI_DDGI_SHADER_REFLECTION));
            Shaders::AddDefine(shader, L"RTXGI_DDGI_BINDLESS_RESOURCES", std::to_wstring(RTXGI_DDGI_BINDLESS_RESOURCES));

            #if !RTXGI_DDGI_RESOURCE_MANAGEMENT
                // Specify resource and spaces when the SDK is not managing resources (and when *not* using shader reflection)
                // Shader resource registers and spaces
                if (spirv)
                {
                #if RTXGI_DDGI_BINDLESS_RESOURCES                                    // Defines when using application's global root signature (bindless)
                    Shaders::AddDefine(shader, L"VOLUME_CONSTS_REGISTER", L"5");
                    Shaders::AddDefine(shader, L"VOLUME_CONSTS_SPACE", L"0");
                    Shaders::AddDefine(shader, L"RWTEX2D_REGISTER", L"6");
                    Shaders::AddDefine(shader, L"RWTEX2D_SPACE", L"0");
                #else                                                                // Defines when using the RTXGI SDK's root signature (not bindless)
                    Shaders::AddDefine(shader, L"VOLUME_CONSTS_REGISTER", L"0");
                    Shaders::AddDefine(shader, L"VOLUME_CONSTS_SPACE", L"0");
                    Shaders::AddDefine(shader, L"RAY_DATA_REGISTER", L"1");
                    Shaders::AddDefine(shader, L"RAY_DATA_SPACE", L"0");
                    //                           OUTPUT_REGISTER                     // Note: this register differs for irradiance vs. distance and should be defined per-shader
                    Shaders::AddDefine(shader, L"OUTPUT_SPACE", L"0");
                    Shaders::AddDefine(shader, L"PROBE_DATA_REGISTER", L"4");
                    Shaders::AddDefine(shader, L"PROBE_DATA_SPACE", L"0");
                #endif
                }
                else // DXIL
                {
                    Shaders::AddDefine(shader, L"CONSTS_REGISTER", L"b0");
                    Shaders::AddDefine(shader, L"CONSTS_SPACE", L"space1");
                #if RTXGI_DDGI_BINDLESS_RESOURCES                                    // Defines when using application's global root signature (bindless)
                    Shaders::AddDefine(shader, L"VOLUME_CONSTS_REGISTER", L"t5");
                    Shaders::AddDefine(shader, L"VOLUME_CONSTS_SPACE", L"space0");
                    Shaders::AddDefine(shader, L"RWTEX2D_REGISTER", L"u6");
                    Shaders::AddDefine(shader, L"RWTEX2D_SPACE", L"space0");
                #else                                                                // Defines when using the RTXGI SDK's root signature (not bindless)
                    Shaders::AddDefine(shader, L"VOLUME_CONSTS_REGISTER", L"t0");
                    Shaders::AddDefine(shader, L"VOLUME_CONSTS_SPACE", L"space1");
                    Shaders::AddDefine(shader, L"RAY_DATA_REGISTER", L"u0");
                    Shaders::AddDefine(shader, L"RAY_DATA_SPACE", L"space1");
                    //                           OUTPUT_REGISTER                     // Note: this register differs for irradiance vs. distance and should be defined per-shader
                    Shaders::AddDefine(shader, L"OUTPUT_SPACE", L"space1");
                    Shaders::AddDefine(shader, L"PROBE_DATA_REGISTER", L"u3");
                    Shaders::AddDefine(shader, L"PROBE_DATA_SPACE", L"space1");
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
            std::wstring numDistanceTexels = std::to_wstring(volumeDesc.probeNumDistanceTexels);

            std::wstring root = std::wstring(gfx.shaderCompiler.rtxgi.begin(), gfx.shaderCompiler.rtxgi.end());

            // Probe blending (irradiance)
            {
                Shaders::ShaderProgram& shader = volumeShaders.emplace_back();
                shader.filepath = root + L"shaders/ddgi/ProbeBlendingCS.hlsl";
                shader.entryPoint = L"DDGIProbeBlendingCS";
                shader.targetProfile = L"cs_6_0";
                if(spirv) shader.arguments = { L"-spirv", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };

                // Add common shader defines
                AddCommonShaderDefines(shader, volumeDesc, spirv);

                // Add shader specific defines
                Shaders::AddDefine(shader, L"RTXGI_DDGI_BLEND_RADIANCE", L"1");
                Shaders::AddDefine(shader, L"RTXGI_DDGI_PROBE_NUM_TEXELS", numIrradianceTexels.c_str());
                Shaders::AddDefine(shader, L"RTXGI_DDGI_BLEND_SHARED_MEMORY", std::to_wstring(RTXGI_DDGI_BLEND_SHARED_MEMORY));
            #if RTXGI_DDGI_BLEND_SHARED_MEMORY
                Shaders::AddDefine(shader, L"RTXGI_DDGI_BLEND_RAYS_PER_PROBE", numRays.c_str());
            #endif

            #if !RTXGI_DDGI_RESOURCE_MANAGEMENT && !RTXGI_DDGI_USE_SHADER_CONFIG_FILE && !RTXGI_DDGI_BINDLESS_RESOURCES
                if (spirv) Shaders::AddDefine(shader, L"OUTPUT_REGISTER", L"2"); // Note: this register differs for irradiance vs. distance
                else Shaders::AddDefine(shader, L"OUTPUT_REGISTER", L"u1");
            #endif

                // Load and compile the shader
                CHECK(Shaders::Compile(gfx.shaderCompiler, shader, true), "compile the RTXGI probe irradiance blending compute shader!\n", log);
            }

            // Probe blending (distance)
            {
                Shaders::ShaderProgram& shader = volumeShaders.emplace_back();
                shader.filepath = root + L"shaders/ddgi/ProbeBlendingCS.hlsl";
                shader.entryPoint = L"DDGIProbeBlendingCS";
                shader.targetProfile = L"cs_6_0";
                if (spirv) shader.arguments = { L"-spirv", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };

                // Add common shader defines
                AddCommonShaderDefines(shader, volumeDesc, spirv);

                // Add shader specific defines
                Shaders::AddDefine(shader, L"RTXGI_DDGI_BLEND_RADIANCE", L"0");
                Shaders::AddDefine(shader, L"RTXGI_DDGI_PROBE_NUM_TEXELS", numDistanceTexels.c_str());
                Shaders::AddDefine(shader, L"RTXGI_DDGI_BLEND_SHARED_MEMORY", std::to_wstring(RTXGI_DDGI_BLEND_SHARED_MEMORY));
            #if RTXGI_DDGI_BLEND_SHARED_MEMORY
                Shaders::AddDefine(shader, L"RTXGI_DDGI_BLEND_RAYS_PER_PROBE", numRays.c_str());
            #endif

            #if !RTXGI_DDGI_RESOURCE_MANAGEMENT && !RTXGI_DDGI_USE_SHADER_CONFIG_FILE && !RTXGI_DDGI_BINDLESS_RESOURCES
                if (spirv) Shaders::AddDefine(shader, L"OUTPUT_REGISTER", L"3"); // Note: this register differs for irradiance vs. distance
                else Shaders::AddDefine(shader, L"OUTPUT_REGISTER", L"u2");
            #endif

                // Load and compile the shader
                CHECK(Shaders::Compile(gfx.shaderCompiler, shader, true), "load and compile the RTXGI probe distance blending compute shader!\n", log);
            }

            // Border row update (irradiance)
            {
                Shaders::ShaderProgram& shader = volumeShaders.emplace_back();
                shader.filepath = root + L"shaders/ddgi/ProbeBorderUpdateCS.hlsl";
                shader.entryPoint = L"DDGIProbeBorderRowUpdateCS";
                shader.targetProfile = L"cs_6_0";
                if (spirv) shader.arguments = { L"-spirv", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };

                // Add common shader defines
                AddCommonShaderDefines(shader, volumeDesc, spirv);

                // Add shader specific defines
                Shaders::AddDefine(shader, L"RTXGI_DDGI_BLEND_RADIANCE", L"1");
                Shaders::AddDefine(shader, L"RTXGI_DDGI_PROBE_NUM_TEXELS", numIrradianceTexels.c_str());

            #if !RTXGI_DDGI_RESOURCE_MANAGEMENT && !RTXGI_DDGI_USE_SHADER_CONFIG_FILE && !RTXGI_DDGI_BINDLESS_RESOURCES
                if (spirv) Shaders::AddDefine(shader, L"OUTPUT_REGISTER", L"2"); // Note: this register differs for irradiance vs. distance
                else Shaders::AddDefine(shader, L"OUTPUT_REGISTER", L"u1");
            #endif

                CHECK(Shaders::Compile(gfx.shaderCompiler, shader, true), "load and compile the RTXGI probe border row update (irradiance) compute shader!\n", log);
            }

            // Border column update (irradiance)
            {
                Shaders::ShaderProgram& shader = volumeShaders.emplace_back();
                shader.filepath = root + L"shaders/ddgi/ProbeBorderUpdateCS.hlsl";
                shader.entryPoint = L"DDGIProbeBorderColumnUpdateCS";
                shader.targetProfile = L"cs_6_0";
                if (spirv) shader.arguments = { L"-spirv", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };

                // Add common shader defines
                AddCommonShaderDefines(shader, volumeDesc, spirv);

                // Add shader specific defines
                Shaders::AddDefine(shader, L"RTXGI_DDGI_BLEND_RADIANCE", L"1");
                Shaders::AddDefine(shader, L"RTXGI_DDGI_PROBE_NUM_TEXELS", numIrradianceTexels.c_str());

            #if !RTXGI_DDGI_RESOURCE_MANAGEMENT && !RTXGI_DDGI_USE_SHADER_CONFIG_FILE && !RTXGI_DDGI_BINDLESS_RESOURCES
                if (spirv) Shaders::AddDefine(shader, L"OUTPUT_REGISTER", L"2"); // Note: this register differs for irradiance vs. distance
                else Shaders::AddDefine(shader, L"OUTPUT_REGISTER", L"u1");
            #endif

                CHECK(Shaders::Compile(gfx.shaderCompiler, shader, true), "load and compile the RTXGI probe border column update (irradiance) compute shader!\n", log);
            }

            // Border row update (distance)
            {
                Shaders::ShaderProgram& shader = volumeShaders.emplace_back();
                shader.filepath = root + L"shaders/ddgi/ProbeBorderUpdateCS.hlsl";
                shader.entryPoint = L"DDGIProbeBorderRowUpdateCS";
                shader.targetProfile = L"cs_6_0";
                if (spirv) shader.arguments = { L"-spirv", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };

                // Add common shader defines
                AddCommonShaderDefines(shader, volumeDesc, spirv);

                // Add shader specific defines
                Shaders::AddDefine(shader, L"RTXGI_DDGI_BLEND_RADIANCE", L"0");
                Shaders::AddDefine(shader, L"RTXGI_DDGI_PROBE_NUM_TEXELS", numDistanceTexels.c_str());

            #if !RTXGI_DDGI_RESOURCE_MANAGEMENT && !RTXGI_DDGI_USE_SHADER_CONFIG_FILE && !RTXGI_DDGI_BINDLESS_RESOURCES
                if (spirv) Shaders::AddDefine(shader, L"OUTPUT_REGISTER", L"3"); // Note: this register differs for irradiance vs. distance
                else Shaders::AddDefine(shader, L"OUTPUT_REGISTER", L"u2");
            #endif

                CHECK(Shaders::Compile(gfx.shaderCompiler, shader, true), "load and compile the RTXGI probe border row update (distance) compute shader!\n", log);
            }

            // Border column update (distance)
            {
                Shaders::ShaderProgram& shader = volumeShaders.emplace_back();
                shader.filepath = root + L"shaders/ddgi/ProbeBorderUpdateCS.hlsl";
                shader.entryPoint = L"DDGIProbeBorderColumnUpdateCS";
                shader.targetProfile = L"cs_6_0";
                if (spirv) shader.arguments = { L"-spirv", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };

                // Add common shader defines
                AddCommonShaderDefines(shader, volumeDesc, spirv);

                // Add shader specific defines
                Shaders::AddDefine(shader, L"RTXGI_DDGI_BLEND_RADIANCE", L"0");
                Shaders::AddDefine(shader, L"RTXGI_DDGI_PROBE_NUM_TEXELS", numDistanceTexels.c_str());

            #if !RTXGI_DDGI_RESOURCE_MANAGEMENT && !RTXGI_DDGI_USE_SHADER_CONFIG_FILE && !RTXGI_DDGI_BINDLESS_RESOURCES
                if (spirv) Shaders::AddDefine(shader, L"OUTPUT_REGISTER", L"3"); // Note: this register differs for irradiance vs. distance
                else Shaders::AddDefine(shader, L"OUTPUT_REGISTER", L"u2");
            #endif

                CHECK(Shaders::Compile(gfx.shaderCompiler, shader, true), "load and compile the RTXGI probe border column update (distance) compute shader!\n", log);
            }

            // Probe relocation
            {
                // Update shader
                Shaders::ShaderProgram& shader = volumeShaders.emplace_back();
                shader.filepath = root + L"shaders/ddgi/ProbeRelocationCS.hlsl";
                shader.entryPoint = L"DDGIProbeRelocationCS";
                shader.targetProfile = L"cs_6_0";
                if (spirv) shader.arguments = { L"-spirv", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };

                // Add common shader defines
                AddCommonShaderDefines(shader, volumeDesc, spirv);

                CHECK(Shaders::Compile(gfx.shaderCompiler, shader, true), "load and compile the RTXGI probe relocation compute shader!\n", log);

                // Reset shader
                Shaders::ShaderProgram& shader2 = volumeShaders.emplace_back();
                shader2.filepath = root + L"shaders/ddgi/ProbeRelocationCS.hlsl";
                shader2.entryPoint = L"DDGIProbeRelocationResetCS";
                shader2.targetProfile = L"cs_6_0";
                if (spirv) shader2.arguments = { L"-spirv", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };

                // Add common shader defines
                AddCommonShaderDefines(shader2, volumeDesc, spirv);

                CHECK(Shaders::Compile(gfx.shaderCompiler, shader2, true), "load and compile the RTXGI probe relocation reset compute shader!\n", log);
            }

            // Probe classification
            {
                // Update shader
                Shaders::ShaderProgram& shader = volumeShaders.emplace_back();
                shader.filepath = root + L"shaders/ddgi/ProbeClassificationCS.hlsl";
                shader.entryPoint = L"DDGIProbeClassificationCS";
                shader.targetProfile = L"cs_6_0";
                if (spirv) shader.arguments = { L"-spirv", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };

                // Add common shader defines
                AddCommonShaderDefines(shader, volumeDesc, spirv);

                CHECK(Shaders::Compile(gfx.shaderCompiler, shader, true), "load and compile the RTXGI probe classification compute shader!\n", log);

                // Reset shader
                Shaders::ShaderProgram& shader2 = volumeShaders.emplace_back();
                shader2.filepath = root + L"shaders/ddgi/ProbeClassificationCS.hlsl";
                shader2.entryPoint = L"DDGIProbeClassificationResetCS";
                shader2.targetProfile = L"cs_6_0";
                if (spirv) shader2.arguments = { L"-spirv", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };

                // Add common shader defines
                AddCommonShaderDefines(shader2, volumeDesc, spirv);

                CHECK(Shaders::Compile(gfx.shaderCompiler, shader2, true), "load and compile the RTXGI probe classification reset compute shader!\n", log);
            }

            log << "done.\n";
            std::flush(log);

            return true;
        }

    } // namespace Graphics::DDGI
}
