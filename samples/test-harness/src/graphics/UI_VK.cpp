/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "graphics/UI.h"

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_glfw.h>

namespace Graphics
{
    namespace Vulkan
    {
        namespace UI
        {

            //----------------------------------------------------------------------------------------------------------
            // Public Functions
            //----------------------------------------------------------------------------------------------------------

            bool Initialize(Graphics::Globals& vk, Graphics::GlobalResources& vkResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log)
            {
                // Setup the ImGui context
                const unsigned int NUM_FRAMES_IN_FLIGHT = 2;
                IMGUI_CHECKVERSION();
                ImGui::CreateContext();
                ImGui::StyleColorsDark();

                // Initialize ImGui
                CHECK(ImGui_ImplGlfw_InitForVulkan(vk.window, true), "initialize ImGui for Linux/Vulkan!\n", log);

                // Describe the Vulkan usage
                ImGui_ImplVulkan_InitInfo initInfo = {};
                initInfo.Device = vk.device;
                initInfo.Instance = vk.instance;
                initInfo.PhysicalDevice = vk.physicalDevice;
                initInfo.QueueFamily = vk.queueFamilyIndex;
                initInfo.Queue = vk.queue;
                initInfo.PipelineCache = VK_NULL_HANDLE;
                initInfo.DescriptorPool = vkResources.descriptorPool;
                initInfo.ImageCount = NUM_FRAMES_IN_FLIGHT;
                initInfo.MinImageCount = NUM_FRAMES_IN_FLIGHT;
                initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

                // Initialize ImGui Vulkan
                CHECK(ImGui_ImplVulkan_Init(&initInfo, vk.renderPass),"initialize ImGui Vulkan!\n", log);

                // Setup the fonts texture
                VKCHECK(vkResetCommandPool(vk.device, vk.commandPool, 0));

                VkCommandBufferBeginInfo beginInfo = {};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                VKCHECK(vkBeginCommandBuffer(vk.cmdBuffer[vk.frameIndex], &beginInfo));

                ImGui_ImplVulkan_CreateFontsTexture(vk.cmdBuffer[vk.frameIndex]);

                VkSubmitInfo endInfo = {};
                endInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                endInfo.commandBufferCount = 1;
                endInfo.pCommandBuffers = &vk.cmdBuffer[vk.frameIndex];

                VKCHECK(vkEndCommandBuffer(vk.cmdBuffer[vk.frameIndex]));
                VKCHECK(vkQueueSubmit(vk.queue, 1, &endInfo, VK_NULL_HANDLE));
                VKCHECK(vkDeviceWaitIdle(vk.device));

                ImGui_ImplVulkan_DestroyFontUploadObjects();

                Graphics::ResetCmdList(vk);

                Graphics::UI::s_initialized = true;

                perf.AddStat("UI", resources.cpuStat, resources.gpuStat);

                return true;
            }

            void Update(
                Graphics::Globals& vk,
                Resources& resources,
                Configs::Config& config,
                Inputs::Input& input,
                Scenes::Scene& scene,
                std::vector<DDGIVolumeBase*>& volumes,
                const Instrumentation::Performance& perf)
            {
                CPU_TIMESTAMP_BEGIN(resources.cpuStat);

                if (config.app.showUI)
                {
                    // Start the ImGui frame
                    ImGui_ImplVulkan_NewFrame();
                    ImGui_ImplGlfw_NewFrame();
                    ImGui::NewFrame();

                    Graphics::UI::CreateDebugWindow(vk, config, input, scene, volumes);
                    Graphics::UI::CreatePerfWindow(vk, config, perf);
                }

                CPU_TIMESTAMP_END(resources.cpuStat);
            }

            void Execute(Graphics::Globals& vk, Graphics::GlobalResources& vkResources, Resources& resources, const Configs::Config& config)
            {
                if (config.app.showUI)
                {
                #ifdef GFX_PERF_MARKERS
                    AddPerfMarker(vk, GFX_PERF_MARKER_GREY, "ImGui" );
                #endif
                    CPU_TIMESTAMP_BEGIN(resources.cpuStat);

                    // Note: Clear is ignored since vk.renderPass attachment load op is VK_ATTACHMENT_LOAD_OP_DONT_CARE
                    VkClearValue clearValue;
                    clearValue.color = { 0.f, 0.f, 0.f, 0.f };

                    // Describe the render pass
                    VkRenderPassBeginInfo renderPassBeginInfo = {};
                    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                    renderPassBeginInfo.framebuffer = vk.frameBuffer[vk.frameIndex];
                    renderPassBeginInfo.renderArea.extent.width = vk.width;
                    renderPassBeginInfo.renderArea.extent.height = vk.height;
                    renderPassBeginInfo.renderPass = vk.renderPass;
                    renderPassBeginInfo.pClearValues = &clearValue;
                    renderPassBeginInfo.clearValueCount = 1;

                    // Start the render pass
                    GPU_TIMESTAMP_BEGIN(resources.gpuStat->GetGPUQueryBeginIndex());
                    vkCmdBeginRenderPass(vk.cmdBuffer[vk.frameIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

                    ImGui::Render();
                    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vk.cmdBuffer[vk.frameIndex]);

                    // End the render pass
                    vkCmdEndRenderPass(vk.cmdBuffer[vk.frameIndex]);
                    GPU_TIMESTAMP_END(resources.gpuStat->GetGPUQueryEndIndex());

                #ifdef GFX_PERF_MARKERS
                    vkCmdEndDebugUtilsLabelEXT(vk.cmdBuffer[vk.frameIndex]);
                #endif

                    CPU_TIMESTAMP_ENDANDRESOLVE(resources.cpuStat);
                }
            }

            void Cleanup()
            {
                Graphics::UI::s_initialized = false;

                ImGui_ImplVulkan_Shutdown();
                ImGui_ImplGlfw_Shutdown();
                ImGui::DestroyContext();
            }

        } // namespace Graphics::Vulkan::UI

    } // namespace Graphics::Vulkan

    namespace UI
    {

        bool Initialize(Globals& vk, GlobalResources& vkResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log)
        {
            return Graphics::Vulkan::UI::Initialize(vk, vkResources, resources, perf, log);
        }

        void Update(Globals& vk, Resources& resources, Configs::Config& config, Inputs::Input& input, Scenes::Scene& scene, std::vector<DDGIVolumeBase*>& volumes, const Instrumentation::Performance& perf)
        {
            return Graphics::Vulkan::UI::Update(vk, resources, config, input, scene, volumes, perf);
        }

        void Execute(Globals& vk, GlobalResources& vkResources, Resources& resources, const Configs::Config& config)
        {
            return Graphics::Vulkan::UI::Execute(vk, vkResources, resources, config);
        }

        void Cleanup()
        {
            Graphics::Vulkan::UI::Cleanup();
        }

    } // namespace Graphics::UI

} //namespace Graphics
