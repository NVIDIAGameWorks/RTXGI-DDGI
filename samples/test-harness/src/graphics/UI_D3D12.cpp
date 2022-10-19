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
#include <imgui_impl_dx12.h>
#include <imgui_impl_glfw.h>

namespace Graphics
{
    namespace D3D12
    {
        namespace UI
        {

            bool Initialize(Graphics::Globals& d3d, Graphics::GlobalResources& d3dResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log)
            {
                // Setup Dear ImGui context
                const unsigned int NUM_FRAMES_IN_FLIGHT = 2;
                IMGUI_CHECKVERSION();
                ImGui::CreateContext();
                ImGui::StyleColorsDark();

                D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle = {};
                CPUHandle.ptr += d3dResources.srvDescHeapStart.ptr + (Graphics::D3D12::DescriptorHeapOffsets::SRV_IMGUI_FONTS * d3dResources.srvDescHeapEntrySize);

                D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle = d3dResources.srvDescHeap->GetGPUDescriptorHandleForHeapStart();
                GPUHandle.ptr += (Graphics::D3D12::DescriptorHeapOffsets::SRV_IMGUI_FONTS * d3dResources.srvDescHeapEntrySize);

                // Initialize ImGui
                CHECK(ImGui_ImplGlfw_InitForOther(d3d.window, true), "initialize ImGui for GLFW", log);
                CHECK(ImGui_ImplDX12_Init(
                    d3d.device,
                    NUM_FRAMES_IN_FLIGHT,
                    DXGI_FORMAT_R8G8B8A8_UNORM,
                    d3dResources.srvDescHeap,
                    CPUHandle,
                    GPUHandle),
                    "initialize ImGui DX12!\n",
                    log);

                Graphics::UI::s_initialized = true;

                perf.AddStat("UI", resources.cpuStat, resources.gpuStat);

                return true;
            }

            void Update(
                Graphics::Globals& d3d,
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
                    ImGui_ImplDX12_NewFrame();
                    ImGui_ImplGlfw_NewFrame();
                    ImGui::NewFrame();

                    Graphics::UI::CreateDebugWindow(d3d, config, input, scene, volumes);
                    Graphics::UI::CreatePerfWindow(d3d, config, perf);
                }

                CPU_TIMESTAMP_END(resources.cpuStat);
            }

            void Execute(Graphics::Globals& d3d, Graphics::GlobalResources& d3dResources, Resources& resources, const Configs::Config& config)
            {
                CPU_TIMESTAMP_BEGIN(resources.cpuStat);

                if (config.app.showUI)
                {
                #ifdef GFX_PERF_MARKERS
                    PIXBeginEvent(d3d.cmdList, PIX_COLOR(GFX_PERF_MARKER_GREY), "ImGui");
                #endif
                    // Transition the back buffer to a render target
                    D3D12_RESOURCE_BARRIER barrier = {};
                    barrier.Transition.pResource = d3d.backBuffer[d3d.frameIndex];
                    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
                    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
                    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

                    // Wait for the transition to complete
                    d3d.cmdList->ResourceBarrier(1, &barrier);

                    // Set the render target
                    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
                    rtvHandle.ptr = d3dResources.rtvDescHeapStart.ptr + (d3d.frameIndex * d3dResources.rtvDescHeapEntrySize);
                    d3d.cmdList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);

                    // Set the resources descriptor heap
                    ID3D12DescriptorHeap* ppHeaps[] = { d3dResources.srvDescHeap };
                    d3d.cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

                    // Render
                    GPU_TIMESTAMP_BEGIN(resources.gpuStat->GetGPUQueryBeginIndex());
                    ImGui::Render();
                    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), d3d.cmdList);
                    GPU_TIMESTAMP_END(resources.gpuStat->GetGPUQueryEndIndex());

                    // Transition the back buffer back to present
                    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
                    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

                    // Wait for the transition to complete
                    d3d.cmdList->ResourceBarrier(1, &barrier);

                #ifdef GFX_PERF_MARKERS
                    PIXEndEvent(d3d.cmdList);
                #endif
                }

                CPU_TIMESTAMP_ENDANDRESOLVE(resources.cpuStat);
            }

            void Cleanup()
            {
                Graphics::UI::s_initialized = false;

                ImGui_ImplDX12_Shutdown();
                ImGui_ImplGlfw_Shutdown();
                ImGui::DestroyContext();
            }

        } // namespace Graphics::D3D12::UI

    } // namespace Graphics::D3D12

    namespace UI
    {

        bool Initialize(Globals& d3d, GlobalResources& d3dResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log)
        {
            return Graphics::D3D12::UI::Initialize(d3d, d3dResources, resources, perf, log);
        }

        void Update(Globals& d3d, Resources& resources, Configs::Config& config, Inputs::Input& input, Scenes::Scene& scene, std::vector<DDGIVolumeBase*>& volumes, const Instrumentation::Performance& perf)
        {
            return Graphics::D3D12::UI::Update(d3d, resources, config, input, scene, volumes, perf);
        }

        void Execute(Globals& d3d, GlobalResources& d3dResources, Resources& resources, const Configs::Config& config)
        {
            return Graphics::D3D12::UI::Execute(d3d, d3dResources, resources, config);
        }

        void Cleanup()
        {
            Graphics::D3D12::UI::Cleanup();
        }

    } // namespace Graphics::UI

} //namespace Graphics
