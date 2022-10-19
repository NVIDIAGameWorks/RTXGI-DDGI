/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "graphics/Composite.h"

namespace Graphics
{
    namespace Vulkan
    {
        namespace Composite
        {

            //----------------------------------------------------------------------------------------------------------
            // Private Functions
            //----------------------------------------------------------------------------------------------------------

            bool LoadAndCompileShaders(Globals& vk, Resources& resources, std::ofstream& log)
            {
                // Release existing shaders
                resources.shaders.Release();

                std::wstring root = std::wstring(vk.shaderCompiler.root.begin(), vk.shaderCompiler.root.end());

                // Load and compile the vertex shader
                resources.shaders.vs.filepath = root + L"shaders/Composite.hlsl";
                resources.shaders.vs.entryPoint = L"VS";
                resources.shaders.vs.targetProfile = L"vs_6_6";
                resources.shaders.vs.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };
                Shaders::AddDefine(resources.shaders.vs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
                CHECK(Shaders::Compile(vk.shaderCompiler, resources.shaders.vs, true), "compile composition vertex shader!\n", log);

                // Load and compile the pixel shader
                resources.shaders.ps.filepath = root + L"shaders/Composite.hlsl";
                resources.shaders.ps.entryPoint = L"PS";
                resources.shaders.ps.targetProfile = L"ps_6_6";
                resources.shaders.ps.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };
                Shaders::AddDefine(resources.shaders.ps, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
                CHECK(Shaders::Compile(vk.shaderCompiler, resources.shaders.ps, true), "compile composition pixel shader!\n", log);

                return true;
            }

            bool CreateDescriptorSets(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                // Describe the descriptor set allocation
                VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
                descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                descriptorSetAllocateInfo.descriptorPool = vkResources.descriptorPool;
                descriptorSetAllocateInfo.descriptorSetCount = 1;
                descriptorSetAllocateInfo.pSetLayouts = &vkResources.descriptorSetLayout;

                // Allocate the descriptor set
                VKCHECK(vkAllocateDescriptorSets(vk.device, &descriptorSetAllocateInfo, &resources.descriptorSet));
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.descriptorSet), "Composition Descriptor Set", VK_OBJECT_TYPE_DESCRIPTOR_SET);
            #endif

                return true;
            }

            bool CreatePipelines(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                // Release existing shader modules and pipeline
                resources.modules.Release(vk.device);
                vkDestroyPipeline(vk.device, resources.pipeline, nullptr);

                // Create the pipeline shader modules
                CHECK(CreateRasterShaderModules(vk.device, resources.shaders, resources.modules), "create Composition shader modules!\n", log);

                // Describe the rasterizer properties
                RasterDesc desc;
                {
                    // Input Assembly
                    desc.inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
                    desc.dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(desc.states.size());
                    desc.dynamicStateCreateInfo.pDynamicStates = desc.states.data();
                    desc.viewportStateCreateInfo.viewportCount = 1;
                    desc.viewportStateCreateInfo.scissorCount = 1;

                    // Blending
                    desc.colorBlendAttachmentState.colorWriteMask = 0xF;
                    desc.colorBlendAttachmentState.blendEnable = VK_FALSE;
                    desc.colorBlendStateCreateInfo.attachmentCount = 1;

                    // Modes
                    desc.rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
                    desc.rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
                    desc.rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
                    desc.rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
                    desc.rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
                    desc.rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
                    desc.rasterizationStateCreateInfo.lineWidth = 1.f;

                    // Depth / Stencil
                    desc.depthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
                    desc.depthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
                    desc.depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
                    desc.depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
                    desc.depthStencilStateCreateInfo.back.failOp = VK_STENCIL_OP_KEEP;
                    desc.depthStencilStateCreateInfo.back.passOp = VK_STENCIL_OP_KEEP;
                    desc.depthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
                    desc.depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;

                    // Multisampling
                    desc.multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
                }

                // Create the pipeline
                CHECK(CreateRasterPipeline(
                    vk.device,
                    vkResources.pipelineLayout,
                    vk.renderPass, resources.shaders,
                    resources.modules,
                    desc,
                    &resources.pipeline), "create Composition pipeline!\n", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.pipeline), "Composition Pipeline", VK_OBJECT_TYPE_PIPELINE);
            #endif

                return true;
            }

            bool UpdateDescriptorSets(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                // Store the data to be written to the descriptor set
                VkWriteDescriptorSet* descriptor = nullptr;
                std::vector<VkWriteDescriptorSet> descriptors;

                // 8: Texture2D UAVs
                VkDescriptorImageInfo rwTex2D[] =
                {
                    { VK_NULL_HANDLE, vkResources.rt.GBufferAView, VK_IMAGE_LAYOUT_GENERAL },
                    { VK_NULL_HANDLE, vkResources.rt.GBufferBView, VK_IMAGE_LAYOUT_GENERAL },
                    { VK_NULL_HANDLE, vkResources.rt.GBufferCView, VK_IMAGE_LAYOUT_GENERAL },
                    { VK_NULL_HANDLE, vkResources.rt.GBufferDView, VK_IMAGE_LAYOUT_GENERAL },
                    { VK_NULL_HANDLE, vkResources.rt.RTAOOutputView, VK_IMAGE_LAYOUT_GENERAL },
                    { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL }, // RTAORaw (skipped)
                    { VK_NULL_HANDLE, vkResources.rt.DDGIOutputView, VK_IMAGE_LAYOUT_GENERAL }
                };

                descriptor = &descriptors.emplace_back();
                descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptor->dstSet = resources.descriptorSet;
                descriptor->dstBinding = DescriptorLayoutBindings::UAV_TEX2D;
                descriptor->dstArrayElement = RWTex2DIndices::GBUFFERA;
                descriptor->descriptorCount = _countof(rwTex2D);
                descriptor->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                descriptor->pImageInfo = rwTex2D;

                // 11: Texture2D SRVs
                VkDescriptorImageInfo tex2D[] =
                {
                    { VK_NULL_HANDLE, vkResources.textureViews[0], VK_IMAGE_LAYOUT_GENERAL } // blue noise
                };

                descriptor = &descriptors.emplace_back();
                descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptor->dstSet = resources.descriptorSet;
                descriptor->dstBinding = DescriptorLayoutBindings::SRV_TEX2D;
                descriptor->dstArrayElement = Tex2DIndices::BLUE_NOISE;
                descriptor->descriptorCount = _countof(tex2D);
                descriptor->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                descriptor->pImageInfo = tex2D;

                // Update the descriptor set
                vkUpdateDescriptorSets(vk.device, static_cast<uint32_t>(descriptors.size()), descriptors.data(), 0, nullptr);

                return true;
            }

            //----------------------------------------------------------------------------------------------------------
            // Public Functions
            //----------------------------------------------------------------------------------------------------------

            /**
             * Create resources used by the ray traced ambient occlusion pass.
             */
            bool Initialize(Globals& vk, GlobalResources& vkResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log)
            {
                if (!LoadAndCompileShaders(vk, resources, log)) return false;
                if (!CreateDescriptorSets(vk, vkResources, resources, log)) return false;
                if (!CreatePipelines(vk, vkResources, resources, log)) return false;

                if (!UpdateDescriptorSets(vk, vkResources, resources, log)) return false;

                perf.AddStat("Composite", resources.cpuStat, resources.gpuStat);

                return true;
            }

            /**
             * Reload and compile shaders, recreate PSOs.
             */
            bool Reload(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                log << "Reloading Composition shaders...";
                if (!LoadAndCompileShaders(vk, resources, log)) return false;
                if (!CreatePipelines(vk, vkResources, resources, log)) return false;
                log << "done.\n";
                log << std::flush;

                return true;
            }

            /**
            * Resize, update descriptor sets. Screen-space textures are resized elsewhere.
            */
            bool Resize(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                log << "Updating Composition descriptor sets...";
                if (!UpdateDescriptorSets(vk, vkResources, resources, log)) return false;
                log << "done.\n";
                log << std::flush;

                return true;
            }

            /**
             * Update data before execute.
             */
            void Update(Globals& vk, GlobalResources& vkResources, Resources& resources, const Configs::Config& config)
            {
                CPU_TIMESTAMP_BEGIN(resources.cpuStat);

                // Composite constants
                vkResources.constants.composite.useFlags = COMPOSITE_FLAG_USE_NONE;
                if (config.rtao.enabled) vkResources.constants.composite.useFlags |= COMPOSITE_FLAG_USE_RTAO;
                if (config.ddgi.enabled) vkResources.constants.composite.useFlags |= COMPOSITE_FLAG_USE_DDGI;

                vkResources.constants.composite.showFlags = COMPOSITE_FLAG_SHOW_NONE;
                if (config.rtao.visualize) vkResources.constants.composite.showFlags |= COMPOSITE_FLAG_SHOW_RTAO;
                if (config.ddgi.showIndirect) vkResources.constants.composite.showFlags |= COMPOSITE_FLAG_SHOW_DDGI_INDIRECT;

                // Post Process constants
                vkResources.constants.post.useFlags = POSTPROCESS_FLAG_USE_NONE;
                if (config.postProcess.enabled)
                {
                    if (config.postProcess.exposure.enabled) vkResources.constants.post.useFlags |= POSTPROCESS_FLAG_USE_EXPOSURE;
                    if (config.postProcess.tonemap.enabled) vkResources.constants.post.useFlags |= POSTPROCESS_FLAG_USE_TONEMAPPING;
                    if (config.postProcess.dither.enabled) vkResources.constants.post.useFlags |= POSTPROCESS_FLAG_USE_DITHER;
                    if (config.postProcess.gamma.enabled) vkResources.constants.post.useFlags |= POSTPROCESS_FLAG_USE_GAMMA;
                    vkResources.constants.post.exposure = pow(2.f, config.postProcess.exposure.fstops);
                }

                CPU_TIMESTAMP_END(resources.cpuStat);
            }

            /**
             * Record the workload to the global command list.
             */
            void Execute(Globals& vk, GlobalResources& vkResources, Resources& resources)
            {
            #ifdef GFX_PERF_MARKERS
                AddPerfMarker(vk, GFX_PERF_MARKER_BLUE, "Composite");
            #endif
                CPU_TIMESTAMP_BEGIN(resources.cpuStat);

                // Set the push constants
                uint32_t offset = 0;
                GlobalConstants consts = vkResources.constants;
                vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, AppConsts::GetSizeInBytes(), consts.app.GetData());
                offset += AppConsts::GetAlignedSizeInBytes();
                offset += PathTraceConsts::GetAlignedSizeInBytes();
                offset += LightingConsts::GetAlignedSizeInBytes();
                offset += RTAOConsts::GetAlignedSizeInBytes();
                vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, CompositeConsts::GetSizeInBytes(), consts.composite.GetData());
                offset += CompositeConsts::GetAlignedSizeInBytes();
                vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, PostProcessConsts::GetSizeInBytes(), consts.post.GetData());

                // Set the pipeline
                vkCmdBindPipeline(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, resources.pipeline);

                // Set the descriptor set
                vkCmdBindDescriptorSets(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, vkResources.pipelineLayout, 0, 1, &resources.descriptorSet, 0, nullptr);

                // Set raster state
                vkCmdSetViewport(vk.cmdBuffer[vk.frameIndex], 0, 1, &vk.viewport);
                vkCmdSetScissor(vk.cmdBuffer[vk.frameIndex], 0, 1, &vk.scissor);

                // Transition the back buffer to a render target (start render pass)
                GPU_TIMESTAMP_BEGIN(resources.gpuStat->GetGPUQueryBeginIndex());
                BeginRenderPass(vk);

                // Draw
                vkCmdDraw(vk.cmdBuffer[vk.frameIndex], 3, 1, 0, 0);

                // Transition the back buffer to present (end render pass)
                vkCmdEndRenderPass(vk.cmdBuffer[vk.frameIndex]);
                GPU_TIMESTAMP_END(resources.gpuStat->GetGPUQueryEndIndex());

            #ifdef GFX_PERF_MARKERS
                vkCmdEndDebugUtilsLabelEXT(vk.cmdBuffer[vk.frameIndex]);
            #endif

                CPU_TIMESTAMP_ENDANDRESOLVE(resources.cpuStat);
            }

            /**
             * Release resources.
             */
            void Cleanup(VkDevice& device, Resources& resources)
            {
                resources.modules.Release(device);
                resources.shaders.Release();
                vkDestroyPipeline(device, resources.pipeline, nullptr);
            }

        } // namespace Graphics::Vulkan::Composite

    } // namespace Graphics::Vulkan

    namespace Composite
    {

        bool Initialize(Globals& vk, GlobalResources& vkResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log)
        {
            return Graphics::Vulkan::Composite::Initialize(vk, vkResources, resources, perf, log);
        }

        bool Reload(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
        {
            return Graphics::Vulkan::Composite::Reload(vk, vkResources, resources, log);
        }

        bool Resize(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
        {
            return Graphics::Vulkan::Composite::Resize(vk, vkResources, resources, log);
        }

        void Update(Globals& vk, GlobalResources& vkResources, Resources& resources, const Configs::Config& config)
        {
            return Graphics::Vulkan::Composite::Update(vk, vkResources, resources, config);
        }

        void Execute(Globals& vk, GlobalResources& vkResources, Resources& resources)
        {
            return Graphics::Vulkan::Composite::Execute(vk, vkResources, resources);
        }

        void Cleanup(Globals& vk, Resources& resources)
        {
            Graphics::Vulkan::Composite::Cleanup(vk.device, resources);
        }

    } // namespace Graphics::Composite
}
