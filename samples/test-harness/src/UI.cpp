/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "UI.h"

#include <Windows.h>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include <sstream>

#include <rtxgi/Defines.h>

using namespace DirectX;

#if RTXGI_PERF_MARKERS
#define USE_PIX
#include <pix3.h>
#endif

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static bool s_initialized = false;

namespace UI
{

//----------------------------------------------------------------------------------------------------------
// Private Functions
//----------------------------------------------------------------------------------------------------------

// Helper to display a little (?) mark which shows a tooltip when hovered.
static void ShowHelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void CreateDebugWindow(
    D3D12Global &d3d,
    DXRGlobal &dxr,
    ConfigInfo &config,
    Camera &camera,
    LightInfo &lights,
    std::vector<rtxgi::DDGIVolume*> volumes,
    InputInfo &input,
    InputOptions &inputOptions,
    RTOptions &rtOptions,
    VizOptions &vizOptions,
    PostProcessOptions &postOptions)
{
    // Size the debug window based on the application height
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetWindowWidth(), d3d.height - 40.f));
    ImGui::Begin("Debug Options and Performance", NULL, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("Frame Time Average: %.3f ms/frame (%.1f FPS) ", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::Text("Resolution: %ix%i", d3d.width, d3d.height);
    ImGui::Text("Frame Number: %i", d3d.frameNumber);

    // Renderer
    {
        if (config.mode == ERenderMode::PathTrace)
        {
            ImGui::Text("Render Mode: Path Tracing");
            ImGui::SameLine(); ShowHelpMarker("Press 'M' to change mode");
        }
        else if (config.mode == ERenderMode::DDGI)
        {
            ImGui::Text("Render Mode: RTXGI");
            ImGui::SameLine(); ShowHelpMarker("Press 'M' to change mode");
        }
        ImGui::Separator();

        ImGui::Checkbox("Vsync", &d3d.vsync);
        ImGui::SameLine(); ShowHelpMarker("Enable or Disable Vertical Sync");
    }

    ImGui::Separator();

    // Camera
    {
        float pos = (ImGui::GetWindowWidth() / 2.f) - ImGui::CalcTextSize("Camera").x / 2.f;
        ImGui::SetCursorPosX(pos);
        ImGui::Text("Camera");
        ImGui::Text("Position: (%.2f, %.2f, %.2f)", camera.position.x, camera.position.y, camera.position.z);
        ImGui::Text("Pitch: %.2f, Yaw: %.2f", input.pitch, input.yaw);
        ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.85f);
        ImGui::DragFloat("##fov", &camera.fov, 0.1f, 25.f, 100.f, "FoV: %.1f degrees");
        ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.85f);
        ImGui::SameLine(); ShowHelpMarker("Adjust the slider to change the vertical field of view.");
        ImGui::DragFloat("##moveSpeed", &inputOptions.movementSpeed, 0.5f, 2.f, 200.f, "Movement speed: %.0f");
        ImGui::SameLine(); ShowHelpMarker("Adjust the slider to change the speed the camera moves.");

        // Update based on fov changes
        camera.tanHalfFovY = std::tanf(camera.fov * (XM_PI / 180.f) * 0.5f);
    }

    ImGui::Separator();

    // Ray Tracing
    {
        int numBounces = (int)rtOptions.numBounces;

        float pos = (ImGui::GetWindowWidth() / 2.f) - ImGui::CalcTextSize("Ray Tracing").x / 2.f;
        ImGui::SetCursorPosX(pos);
        ImGui::Text("Ray Tracing");
        ImGui::DragFloat("##rtViewBias", &rtOptions.viewBias, 0.0001f, 0.f, 10.f, "View Bias: %.4f");
        ImGui::SameLine(); ShowHelpMarker("Adjust the view bias applied to ray origins before tracing.");
        ImGui::DragFloat("##rtNormalBias", &rtOptions.normalBias, 0.0001f, 0.f, 10.f, "Normal Bias: %.4f");
        ImGui::SameLine(); ShowHelpMarker("Adjust the normal bias applied to ray origins before tracing.");
        ImGui::DragInt("##rtNumPaths", &camera.numPaths, 1, 1, 10, "Path Tracer SPP: %.i");
        ImGui::SameLine(); ShowHelpMarker("Adjust the number of paths per pixel the path tracer shoots each frame.");
        ImGui::DragInt("##rtNumBounces", &numBounces, 1, 1, 20, "Path Tracer Bounces: %.i");
        ImGui::SameLine(); ShowHelpMarker("Adjust the number of bounces each path is allowed during path tracing.");

        rtOptions.numBounces = (UINT)numBounces;
    }

    ImGui::Separator();

    // DDGIVolume
    {
        if (ImGui::BeginTabBar("DDGI Volumes"))
        {
            for (int i = 0; i < volumes.size(); i++)
            {
                DDGIVolume* volume = volumes[i];
                if (ImGui::BeginTabItem(volume->GetDesc().name.c_str()))
                {
                    inputOptions.volumeSelect = i;
                    rtxgi::DDGIVolumeDesc desc = volume->GetDesc();
                    float viewBias = volume->GetViewBias();
                    float normalBias = volume->GetNormalBias();

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT
                    ImGui::Text("Coordinate System: Left Hand, Y-Up");
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
                    ImGui::Text("Coordinate System: Right Hand, Y-Up");
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
                    ImGui::Text("Coordinate System: Left Hand, Z-Up (Unreal)");
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
                    ImGui::Text("Coordinate System: Right Hand, Z-Up");
#endif
                    ImGui::Text("Origin: (%.2f, %.2f, %.2f)", desc.origin.x, desc.origin.y, desc.origin.z);
                    ImGui::Text("Probe Grid Counts: (%i, %i, %i)", desc.probeGridCounts.x, desc.probeGridCounts.y, desc.probeGridCounts.z);
                    ImGui::Text("Probe Grid Spacing: (%.2f, %.2f, %.2f)", desc.probeGridSpacing.x, desc.probeGridSpacing.y, desc.probeGridSpacing.z);
                    ImGui::NewLine();

                    float probeMaxRayDistance = volume->GetProbeMaxRayDistance();
                    std::stringstream label;
                    label << "###probeMaxRayDistance" << i;

                    ImGui::DragFloat(label.str().c_str(), &probeMaxRayDistance, 0.1f, 0.f, 10000.f, "Probe Max Ray Distance: %.1f");
                    ImGui::SameLine(); ShowHelpMarker("Adjust the maximum distance a probe ray can travel.");
                    if (probeMaxRayDistance != volume->GetProbeMaxRayDistance())
                    {
                        volume->SetProbeMaxRayDistance(probeMaxRayDistance);
                    }

                    float hysteresis = volume->GetProbeHysteresis();
                    label.clear();
                    label.str("");
                    label << "###hysteresis" << i;
                    ImGui::DragFloat(label.str().c_str(), &hysteresis, 0.01f, 0.f, 1.f, "Probe Hysteresis: %.2f");
                    ImGui::SameLine(); ShowHelpMarker("Adjust the probe hysteresis.");
                    if (hysteresis != volume->GetProbeHysteresis())
                    {
                        volume->SetProbeHysteresis(hysteresis);
                    }

                    float changeThreshold = volume->GetProbeChangeThreshold();
                    label.clear();
                    label.str("");
                    label << "###changeThreshold" << i;
                    ImGui::DragFloat(label.str().c_str(), &changeThreshold, 0.01f, 0.f, 1.f, "Probe Change Threshold: %.2f");
                    ImGui::SameLine(); ShowHelpMarker("Adjust the probe change threshold.");
                    if (changeThreshold != volume->GetProbeChangeThreshold())
                    {
                        volume->SetProbeChangeThreshold(changeThreshold);
                    }

                    float brightnessThreshold = volume->GetProbeBrightnessThreshold();
                    label.clear();
                    label.str("");
                    label << "###brightnessThreshold" << i;
                    ImGui::DragFloat(label.str().c_str(), &brightnessThreshold, 0.0001f, 0.f, 1.f, "Probe Brightness Threshold: %.4f");
                    ImGui::SameLine(); ShowHelpMarker("Adjust the probe impulse threshold.");
                    if (brightnessThreshold != volume->GetProbeBrightnessThreshold())
                    {
                        volume->SetProbeBrightnessThreshold(brightnessThreshold);
                    }

                    float maxDistance = max(max(desc.probeGridSpacing.x, desc.probeGridSpacing.y), desc.probeGridSpacing.z);
                    maxDistance *= 0.5f;

                    label.clear();
                    label.str("");
                    label << "###viewBias" << i;
                    ImGui::DragFloat(label.str().c_str(), &viewBias, 0.01f, 0.f, maxDistance, "View Bias: %.2f");
                    ImGui::SameLine(); ShowHelpMarker("Adjust the view bias used when computing indirect lighting.");

                    label.clear();
                    label.str("");
                    label << "###normalBias" << i;
                    ImGui::DragFloat(label.str().c_str(), &normalBias, 0.01f, 0.f, maxDistance, "Normal Bias: %.2f");
                    ImGui::SameLine(); ShowHelpMarker("Adjust the normal bias used when computing indirect lighting.");

                    volume->SetViewBias(viewBias);
                    volume->SetNormalBias(normalBias);

#if RTXGI_DDGI_PROBE_SCROLL
                    bool scrollingMovement = static_cast<int>(desc.movementType);
                    ImGui::Checkbox("Use Scrolling Movement", &scrollingMovement);
                    ImGui::SameLine(); ShowHelpMarker("Enable or Disable Scrolling Volume Movement");
                    if (scrollingMovement)
                    {
                        ImGui::SetCursorPosX(40);
                        ImGui::Text("Probe Scroll Offset: (%i, %i, %i)", desc.probeScrollOffsets.x, desc.probeScrollOffsets.y, desc.probeScrollOffsets.z);
                        volume->SetMovementType(EDDGIVolumeMovementType::Scrolling);
                    }
                    else
                    {
                        volume->SetMovementType(EDDGIVolumeMovementType::Default);
                    }
#endif

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
                    ImGui::Checkbox("Use Probe State Classifier", &inputOptions.enableProbeClassification);
                    ImGui::SameLine(); ShowHelpMarker("Toggles dynamic probe classification");
                    ImGui::Checkbox("Activate All Probes", &inputOptions.activateAllProbes);
                    ImGui::SameLine(); ShowHelpMarker("Temporarily activates all probes");
#endif
#if RTXGI_DDGI_PROBE_RELOCATION
                    ImGui::Checkbox("Relocate Probes", &inputOptions.enableProbeRelocation);
                    ImGui::SameLine(); ShowHelpMarker("Runs probe relocation");
#endif
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }

        ImGui::Separator();

        ImGui::Checkbox("Use RTXGI Indirect Lighting", &postOptions.useDDGI);
        ImGui::SameLine(); ShowHelpMarker("Toggles whether indirect lighting is computed in RTXGI mode");

        ImGui::Checkbox("Show RTXGI Probes", &vizOptions.showDDGIVolumeProbes);
        ImGui::SameLine(); ShowHelpMarker("Visualize RTXGI irradiance probes. 'P' on the keyboard.");

        if (vizOptions.showDDGIVolumeProbes)
        {
            ImGui::DragFloat("###probeRadius", &vizOptions.probeRadius, 0.01f, 0.f, 1.f, "Probe Radius: %.2f");
            ImGui::SameLine(); ShowHelpMarker("Adjust the radius of the spheres used to visualize probes.");
        }

        ImGui::Checkbox("Show RTXGI Probe Textures", &vizOptions.showDDGIVolumeBuffers);
        ImGui::SameLine(); ShowHelpMarker("Visualize RTXGI probe textures. 'B' on the keyboard.");

        if (vizOptions.showDDGIVolumeBuffers)
        {
            ImGui::DragFloat("###irradianceScale", &vizOptions.irradianceScale, 0.01f, 0.f, 10.f, "Irradiance Scale: %.2f");
            ImGui::SameLine(); ShowHelpMarker("Adjust the size of irradiance texels when visualized on screen.");

            ImGui::DragFloat("###distanceScale", &vizOptions.distanceScale, 0.01f, 0.f, 10.f, "Distance Scale: %.2f");
            ImGui::SameLine(); ShowHelpMarker("Adjust the size of distance texels when visualized on screen.");

            ImGui::DragFloat("###distanceDivisor", &vizOptions.distanceDivisor, 0.10f, 0.f, 400.f, "Distance Divisor: %.1f");
            ImGui::SameLine(); ShowHelpMarker("Adjust the size of divisor used to normalize distance values when visualized on screen.");

            ImGui::DragFloat("###radianceScale", &vizOptions.radianceScale, 0.01f, 0.f, 10.f, "Radiance Scale: %.2f");
            ImGui::SameLine(); ShowHelpMarker("Adjust the size of radiance texels when visualized on screen.");

#if RTXGI_DDGI_PROBE_RELOCATION
            ImGui::DragFloat("###offsetScale", &vizOptions.offsetScale, 0.01f, 0.f, 10.f, "Offset Scale: %.2f");
            ImGui::SameLine(); ShowHelpMarker("Adjust the size of offset texels when visualized on screen.");
#endif

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
            ImGui::DragFloat("###stateScale", &vizOptions.stateScale, 0.01f, 0.f, 10.f, "State Scale: %.2f");
            ImGui::SameLine(); ShowHelpMarker("Adjust the size of state texels when visualized on screen.");
#endif
        }
    }

    // Directional Light
    if (lights.lightMask & 0x00000001)
    {
        ImGui::Separator();

        float pos = (ImGui::GetWindowWidth() / 2.f) - ImGui::CalcTextSize("Directional Light").x / 2.f;
        ImGui::SetCursorPosX(pos);
        ImGui::Text("Directional Light");

        ImGui::Text("Direction");
        static float direction[3] = { lights.directionalLight.direction.x, lights.directionalLight.direction.y, lights.directionalLight.direction.z };
        ImGui::SliderFloat3("##directionLightDirection", direction, -1.f, 1.f);
        ImGui::SameLine(); ShowHelpMarker("Adjust the direction of the directional light.");
        
        ImGui::Text("Color");
        static float color[3] = { lights.directionalLight.color.x, lights.directionalLight.color.y, lights.directionalLight.color.z };
        ImGui::ColorEdit4("###directionalLightColor", color, ImGuiColorEditFlags__OptionsDefault | ImGuiColorEditFlags_NoAlpha);
        ImGui::SameLine(); ShowHelpMarker("Adjust the color of the directional light.");

        ImGui::DragFloat("###directionalLightPower", &lights.directionalLight.power, 0.01f, 0.f, 4.f, "Power: %.2f");
        ImGui::SameLine(); ShowHelpMarker("Adjust the power of the directional light.");

        lights.directionalLight.direction = { direction[0], direction[1], direction[2] };
        lights.directionalLight.color = { color[0], color[1], color[2] };
    }

    // Point Light
    if (lights.lightMask & 0x00000010)
    {
        ImGui::Separator();

        float pos = (ImGui::GetWindowWidth() / 2.f) - ImGui::CalcTextSize("Point Light").x / 2.f;
        ImGui::SetCursorPosX(pos);
        ImGui::Text("Point Light");

        ImGui::Text("Position");
        static float position[3] = { lights.pointLight.position.x, lights.pointLight.position.y, lights.pointLight.position.z };        
        ImGui::SliderFloat3("##pointLightLocation", position, -400.f, 400.f);
        ImGui::SameLine(); ShowHelpMarker("Adjust the position of the point light.");
        
        ImGui::Text("Color");
        static float color[3] = { lights.pointLight.color.x, lights.pointLight.color.y, lights.pointLight.color.z };
        ImGui::ColorEdit4("###pointLightColor", color, ImGuiColorEditFlags__OptionsDefault | ImGuiColorEditFlags_NoAlpha);
        ImGui::SameLine(); ShowHelpMarker("Adjust the color of the point light.");

        ImGui::DragFloat("###pointLightPower", &lights.pointLight.power, 0.1f, 0.f, 1000.f, "Power: %.2f");
        ImGui::SameLine(); ShowHelpMarker("Adjust the power of the point light.");

        lights.pointLight.position = { position[0], position[1], position[2] };
        lights.pointLight.color = { color[0], color[1], color[2] };
    }

    // Spot Light
    if (lights.lightMask & 0x00000100)
    {
        ImGui::Separator();

        float pos = (ImGui::GetWindowWidth() / 2.f) - ImGui::CalcTextSize("Spot Light").x / 2.f;
        ImGui::SetCursorPosX(pos);
        ImGui::Text("Spot Light");

        static float position[3] = { lights.spotLight.position.x, lights.spotLight.position.y, lights.spotLight.position.z };
        ImGui::Text("Position");
        ImGui::SliderFloat3("##spotLightPosition", position, -1.f, 1.f);
        ImGui::SameLine(); ShowHelpMarker("Adjust the position of the spot light.");

        static float direction[3] = { lights.spotLight.direction.x, lights.spotLight.direction.y, lights.spotLight.direction.z };
        ImGui::Text("Direction");
        ImGui::SliderFloat3("##spotLightDirection", direction, -1.f, 1.f);
        ImGui::SameLine(); ShowHelpMarker("Adjust the position of the spot light.");

        ImGui::Text("Color");
        static float color[3] = { lights.spotLight.color.x, lights.spotLight.color.y, lights.spotLight.color.z };
        ImGui::ColorEdit4("###spotLightColor", color, ImGuiColorEditFlags__OptionsDefault | ImGuiColorEditFlags_NoAlpha);
        ImGui::SameLine(); ShowHelpMarker("Adjust the color of the spot light.");

        static float umbraAngle = lights.spotLight.umbraAngle * (180.f / XM_PI);
        ImGui::DragFloat("###spotLightUmbraAngle", &umbraAngle, 0.1f, 0.f, 180.f, "Umbra Angle: %.2f");
        ImGui::SameLine(); ShowHelpMarker("Adjust the umbra angle of the spot light.");

        static float penumbraAngle = lights.spotLight.penumbraAngle * (180.f / XM_PI);
        ImGui::DragFloat("###spotLightPenumbraAngle", &penumbraAngle, 0.1f, 0.f, 180.f, "Penumbra Angle: %.2f");
        ImGui::SameLine(); ShowHelpMarker("Adjust the penumbra angle of the spot light.");

        ImGui::DragFloat("###spotLightPower", &lights.spotLight.power, 1.f, 0.f, 1000000.f, "Power: %.2f");
        ImGui::SameLine(); ShowHelpMarker("Adjust the power of the spot light.");
       
        ImGui::DragFloat("###spotLightMaxDistance", &lights.spotLight.maxDistance, 1.f, 0.f, 5000.f, "Maximum Distance: %.2f");
        ImGui::SameLine(); ShowHelpMarker("Adjust the max distance of the spot light.");

        lights.spotLight.position = { position[0], position[1], position[2] };
        lights.spotLight.direction = { direction[0], direction[1], direction[2] };
        lights.spotLight.color = { color[0], color[1], color[2] };
        lights.spotLight.umbraAngle = umbraAngle * (XM_PI / 180.f);
        lights.spotLight.penumbraAngle = penumbraAngle * (XM_PI / 180.f);
    }

    ImGui::Separator();

    // Ambient Occlusion
    {
        float pos = (ImGui::GetWindowWidth() / 2.f) - ImGui::CalcTextSize("Ambient Occlusion").x / 2.f;
        ImGui::SetCursorPosX(pos);
        ImGui::Text("Ambient Occlusion");

        ImGui::Checkbox("Enable", &postOptions.useRTAO);
        ImGui::SameLine(); ShowHelpMarker("Enable or disable ray traced ambient occlusion");

        ImGui::Checkbox("Visualize AO", &postOptions.viewAO);
        ImGui::SameLine(); ShowHelpMarker("View the ambient occlusion buffer.");

        ImGui::DragFloat("###RayLength", &postOptions.AORadius, 0.01f, 0.0f, 50.f, "Ray Length: %.2f");
        ImGui::SameLine(); ShowHelpMarker("The length of the ray used for AO sampling");

        ImGui::DragFloat("###RayBias", &postOptions.AOBias, 0.0001f, 0.f, 5.f, "Ray Bias: %.4f");
        ImGui::SameLine(); ShowHelpMarker("The minimum time T to accept a ray hit. This is to avoid intersection with the surface the ray is shot from.");

        ImGui::DragFloat("###AOPower", &postOptions.AOPowerLog, 0.01f, -10.f, 10.f, "AO Power (log2): %.2f");
        ImGui::SameLine(); ShowHelpMarker("When an AO ray hits something, the intersection length percentage is raised to this power to come up with an actual AO value.");

        ImGui::DragFloat("###FilterDistanceSigma", &postOptions.AOFilterDistanceSigma, 0.1f, 0.f, 20.f, "Filter Distance Sigma: %.1f");
        ImGui::SameLine(); ShowHelpMarker("The sigma for the gaussian weight for distance differences, for the bilateral filter.");

        ImGui::DragFloat("###FilterDepthSigma", &postOptions.AOFilterDepthSigma, 0.1f, 0.f, 20.f, "Filter Depth Sigma: %.1f");
        ImGui::SameLine(); ShowHelpMarker("The sigma for the gaussian weight for color differences, for the bilateral filter.");
    }

    ImGui::Separator();

    // Post Processing
    {
        float pos = (ImGui::GetWindowWidth() / 2.f) - ImGui::CalcTextSize("Post Processing").x / 2.f;
        ImGui::SetCursorPosX(pos);
        ImGui::Text("Post Processing");

        bool toggle = false;
        if (ImGui::Checkbox("Toggle All Post Processing Effects", &toggle))
        {
            postOptions.useTonemapping = !postOptions.useTonemapping;
            postOptions.useDithering = !postOptions.useDithering;
            postOptions.useExposure = !postOptions.useExposure;
        }
        ImGui::SameLine(); ShowHelpMarker("Enable or disable all post processing effects");

        ImGui::Checkbox("Tonemapping", &postOptions.useTonemapping);
        ImGui::SameLine(); ShowHelpMarker("Enable or disable tonemapping");

        ImGui::Checkbox("Color Banding Reduction", &postOptions.useDithering);
        ImGui::SameLine(); ShowHelpMarker("Enable or disable blue noise dithering to reduce color banding");

        ImGui::Checkbox("Exposure", &postOptions.useExposure);
        ImGui::SameLine(); ShowHelpMarker("Enable or disable exposure");

        if (postOptions.useExposure)
        {
            ImGui::DragFloat("##exposure", &postOptions.exposureFStops, 0.01f, -8.f, 8.f, "Exposure (FStops): %.1f");
            ImGui::SameLine(); ShowHelpMarker("Adjust the slider to change the camera exposure.");
        }
    }

    ImGui::SetWindowPos("Debug Options and Performance", ImVec2((d3d.width - ImGui::GetWindowWidth() - 20.f), 20));
    ImGui::End();
}

//----------------------------------------------------------------------------------------------------------
// Public Functions
//----------------------------------------------------------------------------------------------------------

void Initialize(D3D12Global &d3d, D3D12Resources &resources, HWND window)
{
    // Setup Dear ImGui context
    const unsigned int NUM_FRAMES_IN_FLIGHT = 2;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    UINT handleIncrement = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle = resources.cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
    CPUHandle.ptr += (handleIncrement * DescriptorHeapConstants::IMGUI_OFFSET);        // IMGui font SRV is 41st on the descriptor heap

    D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle = resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
    GPUHandle.ptr += (handleIncrement * DescriptorHeapConstants::IMGUI_OFFSET);

    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX12_Init(d3d.device, NUM_FRAMES_IN_FLIGHT,
        DXGI_FORMAT_R8G8B8A8_UNORM, resources.cbvSrvUavHeap,
        CPUHandle,
        GPUHandle);

    s_initialized = true;
}

void Cleanup()
{
    s_initialized = false;

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void OnNewFrame(
    D3D12Global &d3d,
    DXRGlobal &dxr,
    ConfigInfo &config,
    Camera &camera,
    LightInfo &lights,
    std::vector<rtxgi::DDGIVolume*> volumes,
    InputInfo &input,
    InputOptions &inputOptions,
    RTOptions &rtOptions,
    VizOptions &vizOptions,
    PostProcessOptions &postOptions)
{
    // Start the Dear ImGui frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    CreateDebugWindow(d3d, dxr, config, camera, lights, volumes, input, inputOptions, rtOptions, vizOptions, postOptions);
}

void OnRender(D3D12Global &d3d, D3D12Resources &resources)
{
#if RTXGI_PERF_MARKERS
    PIXScopedEvent(d3d.cmdList, PIX_COLOR(190, 190, 190), "ImGui");
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
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = resources.rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += (resources.rtvDescSize * d3d.frameIndex);
    d3d.cmdList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);

    // Set the CBV/SRV/UAV descriptor heap
    ID3D12DescriptorHeap* ppHeaps[] = { resources.cbvSrvUavHeap };
    d3d.cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // Render
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), d3d.cmdList);

    // Transition the back buffer back to present
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

    // Wait for the transition to complete
    d3d.cmdList->ResourceBarrier(1, &barrier);
}

bool WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (s_initialized) return ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam);
    return false;
}

bool WantsMouseCapture()
{
    if (s_initialized) return ImGui::GetIO().WantCaptureMouse;
    return false;
}

}
