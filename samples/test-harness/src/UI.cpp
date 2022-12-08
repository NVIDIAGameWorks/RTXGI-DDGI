/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "UI.h"

#include <math.h>
#include <sstream>
#include <stdio.h>
#include <locale.h>

#include <imgui.h>
#if __linux__
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>
#endif

namespace Graphics
{
    namespace UI
    {
        bool s_initialized = false;

        static int prevRes = 0;
        static int curRes = 0;
        static bool resChanged = false;
        static float debugWindowWidth = 500.f;
        static float perfWindowWidth = 300.f;
        static float indent = 150.f;
        struct ResolutionOption
        {
            int width = 0;
            int height = 0;
            std::string label = "";
        };

        static std::vector<ResolutionOption> resolutions =
        {
            { 0, 0, "Config File"},
            { 1280, 720, "720p" },
            { 1920, 1080, "FHD, 1080p" },
            { 2560, 1440, "QHD, 1440p" },
            { 3840, 2160, "UHD, 4K" },
        };

        //----------------------------------------------------------------------------------------------------------
        // Private Functions
        //----------------------------------------------------------------------------------------------------------

        /**
         * Initializes the style template.
         */
        void SetupStyle()
        {
            ImGuiStyle& style = ImGui::GetStyle();
            style.Colors[ImGuiCol_TitleBg] = (ImVec4)ImColor(70, 70, 70, 120);
            style.Colors[ImGuiCol_TitleBgActive] = (ImVec4)ImColor(70, 70, 70, 255);
            style.Colors[ImGuiCol_TitleBgCollapsed] = (ImVec4)ImColor(70, 70, 70, 255);
            style.Colors[ImGuiCol_FrameBg] = (ImVec4)ImColor(70, 70, 70, 120);
            style.Colors[ImGuiCol_FrameBgHovered] = (ImVec4)ImColor(70, 70, 70, 200);
            style.Colors[ImGuiCol_FrameBgActive] = (ImVec4)ImColor(70, 70, 70, 255);
            style.Colors[ImGuiCol_Header] = (ImVec4)ImColor(70, 70, 70, 120);
            style.Colors[ImGuiCol_HeaderHovered] = (ImVec4)ImColor(70, 70, 70, 200);
            style.Colors[ImGuiCol_HeaderActive] = (ImVec4)ImColor(70, 70, 70, 255);
            style.Colors[ImGuiCol_Button] = (ImVec4)ImColor(70, 70, 70, 120);
            style.Colors[ImGuiCol_ButtonHovered] = (ImVec4)ImColor(70, 70, 70, 200);
            style.Colors[ImGuiCol_ButtonActive] = (ImVec4)ImColor(70, 70, 70, 255);
            style.Colors[ImGuiCol_Tab] = (ImVec4)ImColor(70, 70, 70, 120);
            style.Colors[ImGuiCol_TabHovered] = (ImVec4)ImColor(70, 70, 70, 200);
            style.Colors[ImGuiCol_TabActive] = (ImVec4)ImColor(70, 70, 70, 255);
            style.Colors[ImGuiCol_CheckMark] = ImVec4(1.f, 1.f, 1.f, 1.f);
        }

        /*
        * Adds a question mark (?) tool-tip.
        */
        void AddQuestionMark(const char* desc)
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

        /**
        * Adds a text tool-tip on hover.
        */
        void AddHoverToolTip(std::string message)
        {
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                ImGui::TextUnformatted(message.c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
        }

        /**
         * Add a float value slider.
         */
        bool AddSlider(float& value, float min, float max, float step, std::string id, std::string label, std::string hoverText = "")
        {
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
            std::string format = label.append(": %.3f");
            bool result = ImGui::DragFloat(id.c_str(), &value, step, min, max, format.c_str());
            AddHoverToolTip(hoverText.c_str());

            return result;
        }

        /**
        * Helper to add a float3 slider.
        */
        bool AddFloat3Slider(float3& value, float speed, float min, float max, std::string id, std::string label, std::string hoverText = "")
        {
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - indent);

            float data[3] = { value.x, value.y, value.z };
            bool result = ImGui::DragFloat3(id.c_str(), data, speed, min, max, "%.3f");
            value = { data[0], data[1], data[2] };
            AddHoverToolTip(hoverText.c_str());

            ImGui::SameLine(); ImGui::PopItemWidth(); ImGui::Text("%s", label.c_str());
            return result;
        }

        /**
         * Helper to add a color slider.
         */
        bool AddColorSlider(float3& color, std::string id, std::string hoverText = "")
        {
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - indent);

            float data[3] = { color.x, color.y, color.z };
            bool result = ImGui::ColorEdit4(id.c_str(), data, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Float);
            AddHoverToolTip(hoverText.c_str());
            color = { data[0], data[1], data[2] };

            ImGui::SameLine(); ImGui::PopItemWidth(); ImGui::Text("Color");

            return result;
        }

        void AddHRule()
        {
            if (ImGui::BeginTable("##hrule", 1, ImGuiTableFlags_BordersH))
            {
                ImGui::EndTable();
            }
        }

        void AddTextSeparator()
        {
            ImGui::Text("------------------------------------------------------------------");
        }

        void GetSceneScaleSliderValues(Scenes::Scene scene, float& max, float& step)
        {
            // Find scene scale
            float d = rtxgi::Distance(scene.boundingBox.max, scene.boundingBox.min);
            max = roundf(d);

            // Find slider step, for every 10 pixels of mouse movement
            step = (max / 100.f);
            step /= 10.f;
        }

        /**
        * Push our color style onto the ImGui stack.
        */
        void PushColorStyle(int r, int g, int b)
        {
            ImGui::PushStyleColor(ImGuiCol_Header, (ImVec4)ImColor(r, g, b, 120));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, (ImVec4)ImColor(r, g, b, 200));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, (ImVec4)ImColor(r, g, b, 255));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, (ImVec4)ImColor(r, g, b, 120));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, (ImVec4)ImColor(r, g, b, 200));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, (ImVec4)ImColor(r, g, b, 255));
            ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(r, g, b, 120));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor(r, g, b, 200));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor(r, g, b, 255));
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, (ImVec4)ImColor(r, g, b, 200));
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, (ImVec4)ImColor(r, g, b, 255));
        }

        /**
        * Pop our color style from the ImGui stack.
        */
        void PopColorStyle()
        {
            ImGui::PopStyleColor(11);
            ImGui::Separator();
        }

        /**
         * Converts a number to formatted text.
         */
        void AddIntQuantityText(int value, std::string message)
        {
            // Format text
            std::string number = std::to_string(value);
            if (value > 999) number.insert(number.end() - 3, ',');
            if (value > 999999) number.insert(number.end() - 7, ',');
            if (value > 999999999) number.insert(number.end() - 11, ',');

            ImGui::Text("%s", std::string(number).c_str());
            ImGui::SameLine();
            ImGui::Text("%s", message.c_str());
        }

        void AddFloatQuantityText(float value, std::string message)
        {
            // Format text
            std::string number = std::to_string(value);

            ImGui::Text("%s", std::string(number).c_str());
            ImGui::SameLine();
            ImGui::Text("%s", message.c_str());
        }

        /**
         * Creates the main debug window.
         */
        void CreateDebugWindow(Graphics::Globals& gfx, Configs::Config& config, Inputs::Input& input, Scenes::Scene& scene, std::vector<DDGIVolumeBase*>& volumes)
        {
            SetupStyle();

            bool skyChanged = false;
            bool anyLightChanged = false;
            std::vector<bool> volumeChanged;
            volumeChanged.resize(volumes.size());

            // Size the debug window based on the application height
            ImGui::SetNextWindowSize(ImVec2(debugWindowWidth, gfx.height - 40.f));
            ImGui::Begin("Settings (Press 'u' to hide)", NULL, ImGuiWindowFlags_AlwaysAutoResize);

            ImGui::BeginDisabled(config.app.benchmarkRunning);

            // Application
            if (ImGui::CollapsingHeader("Application", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_CollapsingHeader))
            {
                ImGui::Text("RTXGI SDK Version: %s", RTXGI_VERSION::getVersionString());
                ImGui::Text("API: %s", config.app.api.c_str());

                // Rendering mode
                if (config.app.renderMode == ERenderMode::PATH_TRACE)
                {
                    ImGui::Text("Render Mode: Path Tracing");
                }
                else if (config.app.renderMode == ERenderMode::DDGI)
                {
                    ImGui::Text("Render Mode: DDGI");

                #if RTXGI_DDGI_RESOURCE_MANAGEMENT
                    ImGui::Text("RTXGI Resource Mode: Managed");
                #else
                    ImGui::Text("RTXGI Resource Mode: Unmanaged");
                #endif

                #if RTXGI_DDGI_BINDLESS_RESOURCES
                    #if RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS
                        ImGui::Text("RTXGI Resource Access: Bindless, Resource Arrays");
                    #elif RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_DESCRIPTOR_HEAP
                        ImGui::Text("RTXGI Resource Access: Bindless, Descriptor Heap (SM6.6+)");
                    #endif
                #else
                    ImGui::Text("RTXGI Resource Access: Bound");
                #endif
                }

            #if COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT
                ImGui::Text("Coordinate System: Left Hand, Y-Up");
            #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT
                ImGui::Text("Coordinate System: Right Hand, Y-Up");
            #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT_Z_UP
                ImGui::Text("Coordinate System: Left Hand, Z-Up");
            #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT_Z_UP
                ImGui::Text("Coordinate System: Right Hand, Z-Up");
            #endif

                if (ImGui::Button("Write Intermediate Images to Disk"))
                {
                    input.event = Inputs::EInputEvent::SAVE_IMAGES;
                }
                ImGui::SameLine(); AddQuestionMark("Save images of GBuffer slices, RTAO intermediate buffers, and DDGIVolume resources. Press 'F2' on the keyboard for a shortcut.");

                if (ImGui::Button("Screenshot"))
                {
                    input.event = Inputs::EInputEvent::SCREENSHOT;
                }
                ImGui::SameLine(); AddQuestionMark("Save a screenshot. Press 'F1' on the keyboard for a shortcut.");
            }
            ImGui::Separator();

            // Performance
            if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_CollapsingHeader))
            {
                ImGui::Text("Frame Number: %i", gfx.frameNumber);
                ImGui::Text("Frame Time Average: %.3f ms/frame (%.1f FPS) ", 1000.f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
                ImGui::Text("Device: %s", config.app.gpuName.c_str());
                ImGui::Checkbox("Detailed Performance", &config.app.showPerf);
                ImGui::SameLine(); AddQuestionMark("Shows detailed performance information. Press 'J' on the keyboard for a shortcut.");

                if(ImGui::Button("Run Benchmark"))
                {
                    input.event = Inputs::EInputEvent::RUN_BENCHMARK;
                }
                if(config.app.benchmarkRunning)
                {
                    ImGui::SameLine();
                    ImGui::Text("Running...%d%%", config.app.benchmarkProgress);
                }
                ImGui::SameLine(); AddQuestionMark("Runs a benchmark that captures performance information for 1,000 frames. Press 'F4' on the keyboard for a shortcut.");
            }
            ImGui::Separator();

            // Display
            if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_CollapsingHeader))
            {
                // Screen Resolution
                {
                    ImGui::Text("Resolution: %ix%i", gfx.width, gfx.height);

                    if (!gfx.fullscreen)
                    {
                        float pos = ImGui::GetCursorPosY();
                        ImGui::SetCursorPosY(pos + 3);
                        ImGui::Text("Resolution Preset:");
                        ImGui::SameLine();
                        ImGui::SetCursorPosY(pos);

                        // Setup the drop down box
                        ResolutionOption res = resolutions[curRes];
                        const char* label = res.label.c_str();
                        resolutions[0].width = config.app.width;
                        resolutions[0].height = config.app.height;
                        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                        if (ImGui::BeginCombo("##resolutionSelect", label, ImGuiComboFlags_None))
                        {
                            for (int n = 0; n < static_cast<int>(resolutions.size()); n++)
                            {
                                const bool selected = (curRes == n);
                                if (ImGui::Selectable(resolutions[n].label.c_str(), selected))
                                {
                                    curRes = n;
                                    resChanged = true;
                                }
                                if (selected) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }

                        // Update to chosen resolution
                        res = resolutions[curRes];

                        // Resize if the resolution changed
                        if (resChanged && (res.width != gfx.width || res.height != gfx.height))
                        {
                            resChanged = false;
                            glfwSetWindowSize(gfx.window, res.width, res.height);
                        }
                    }
                }

                // Vsync
                {
                    if (ImGui::Checkbox("Vsync", &gfx.vsync)) gfx.vsyncChanged = true;
                    ImGui::SameLine(); AddQuestionMark("Enable or disable vertical sync");
                }

                // Fullscreen
                {
                    bool windowed = !static_cast<bool>(gfx.fullscreen);
                    if (ImGui::Checkbox("Windowed", &windowed)) gfx.fullscreenChanged = true;
                    ImGui::SameLine(); AddQuestionMark("Enable or disable windowed mode");
                }
            }
            ImGui::Separator();

            // Scene
            PushColorStyle(70, 70, 70);
            if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_CollapsingHeader))
            {
                ImGui::Text("%s", scene.name.c_str());
                AddIntQuantityText(static_cast<int>(scene.materials.size()), "Materials");
                AddIntQuantityText(static_cast<int>(scene.meshes.size()), "Mesh");
                AddIntQuantityText(static_cast<int>(scene.numMeshPrimitives), "Mesh Primitives");
                AddIntQuantityText(static_cast<int>(scene.instances.size()), "Mesh Instances");
                AddIntQuantityText(static_cast<int>(scene.numTriangles), "Triangles (unique, not instanced)");
            }
            PopColorStyle();

            // Cameras
            PushColorStyle(70, 70, 70);
            if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_CollapsingHeader))
            {
                // Add a drop down with all scene cameras
                static uint32_t currentItem = 0;                    // Here our selection data is an index.
                const char* label = scene.cameras[scene.activeCamera].name.c_str();
                ImGui::PushItemWidth(ImGui::GetWindowWidth() - 12);
                if (ImGui::BeginCombo("##cameraSelect", label, ImGuiComboFlags_None))
                {
                    for (uint32_t cameraIndex = 0; cameraIndex < static_cast<int>(scene.cameras.size()); cameraIndex++)
                    {
                        const bool selected = (currentItem == cameraIndex);
                        if (ImGui::Selectable(scene.cameras[cameraIndex].name.c_str(), selected)) currentItem = cameraIndex;

                        // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if(scene.activeCamera != currentItem)
                {
                    // Camera changed
                    scene.activeCamera = currentItem;
                    gfx.frameNumber = 1;
                }

                // Get the active camera
                Scenes::Camera& camera = scene.GetActiveCamera();
                ImGui::Text("Position: (%.2f, %.2f, %.2f)", camera.data.position.x, camera.data.position.y, camera.data.position.z);
                ImGui::Text("Yaw: %.2f", camera.yaw);
                ImGui::Text("Pitch: %.2f", camera.pitch);
                ImGui::PushItemWidth(ImGui::GetWindowWidth());
                ImGui::DragFloat("##fov", &camera.data.fov, 0.1f, 25.f, 100.f, "Field of View: %.1f degrees");
                AddHoverToolTip("Drag the slider to change the vertical field of view");
                ImGui::PushItemWidth(ImGui::GetWindowWidth());
                ImGui::DragFloat("##moveSpeed", &config.input.movementSpeed, 0.5f, 2.f, 200.f, "Movement Speed: %.0f");
                AddHoverToolTip("Drag the slider to change the speed the camera moves");

                // Update camera based on fov changes
                camera.data.tanHalfFovY = tanf(camera.data.fov * (DirectX::XM_PI / 180.f) * 0.5f);
            }
            PopColorStyle();

            // Sky
            PushColorStyle(70, 70, 70);
            if (ImGui::CollapsingHeader("Sky", ImGuiTreeNodeFlags_CollapsingHeader))
            {
                float3 skyColor = { config.scene.skyColor.x, config.scene.skyColor.y, config.scene.skyColor.z };
                if (AddColorSlider(skyColor, "##skyColor", "Adjust the color of the sky"))
                {
                    skyChanged = true;
                    config.scene.skyColor = { skyColor.x, skyColor.y, skyColor.z };
                }

                skyChanged |= AddSlider(config.scene.skyIntensity, 0.f, 8.f, 0.1f, "##sceneSkyIntensity", "Sky Intensity", "Adjust the intensity of the sky light spot light");
            }
            PopColorStyle();

            // Lights
            PushColorStyle(70, 70, 70);
            if (ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_CollapsingHeader))
            {
                // Adjust slider to the scene's scale
                float max, step;
                GetSceneScaleSliderValues(scene, max, step);

                ImGui::Dummy(ImVec2(0, 3.f));
                if (ImGui::BeginTabBar("Light Types"))
                {
                    // Directional Light
                    if(ImGui::BeginTabItem("Directional"))
                    {
                        if (scene.hasDirectionalLight)
                        {
                            // Get the light
                            Graphics::Light& light = scene.lights[0].data;

                            // Direction
                            if (AddFloat3Slider(light.direction, 0.1f, -1.f, 1.f, "##directionalLightDirection", "Direction", "Adjust the direction of the directional light"))
                            {
                                scene.lights[0].dirty = true;
                                scene.lights[0].data.direction = light.direction;
                            }

                            // Color
                            if (AddColorSlider(light.color, "##directionalLightColor", "Adjust the color of the directional light"))
                            {
                                scene.lights[0].dirty = true;
                                scene.lights[0].data.color = light.color;
                            }

                            // Power
                            if (AddSlider(light.power, 0.f, 8.f, 0.1f, "##directionalLightPower", "Power", "Adjust the power of the directional light"))
                            {
                                scene.lights[0].dirty = true;
                                scene.lights[0].data.power = light.power;
                            }
                            anyLightChanged = anyLightChanged || scene.lights[0].dirty;
                        }
                        else
                        {
                            ImGui::Text("No direction light");
                        }

                        ImGui::EndTabItem();
                    }

                    // Spot Light(s)
                    if (ImGui::BeginTabItem("Spot"))
                    {
                        if (scene.numSpotLights > 0)
                        {
                            // Add a drop down with all spot lights
                            static int currentItem = scene.firstSpotLight;
                            const char* label = scene.lights[currentItem].name.c_str();
                            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                            if (ImGui::BeginCombo("##spotLightSelect", label, ImGuiComboFlags_None))
                            {
                                for (int i = 0; i < static_cast<int>(scene.lights.size()); i++)
                                {
                                    if (scene.lights[i].type == ELightType::SPOT)
                                    {
                                        const bool selected = (currentItem == i);
                                        if (ImGui::Selectable(scene.lights[i].name.c_str(), selected)) currentItem = i;
                                        if (selected) ImGui::SetItemDefaultFocus();
                                    }
                                }
                                ImGui::EndCombo();
                            }

                            // Get the selected light
                            Graphics::Light light = scene.lights[currentItem].data;

                            // Position
                            if (AddFloat3Slider(light.position, step, -max, max, "##spotLightPosition", "Position", "Adjust the position of the spot light"))
                            {
                                scene.lights[currentItem].dirty = true;
                                scene.lights[currentItem].data.position = light.position;
                            }

                            // Direction
                            if (AddFloat3Slider(light.direction, 0.1f, -1.f, 1.f, "##spotLightDirection", "Direction", "Adjust the direction of the spot light"))
                            {
                                scene.lights[currentItem].dirty = true;
                                scene.lights[currentItem].data.direction = light.direction;
                            }

                            // Color
                            if (AddColorSlider(light.color, "##spotLightColor", "Adjust the color of the spot light"))
                            {
                                scene.lights[currentItem].dirty = true;
                                scene.lights[currentItem].data.color = light.color;
                            }

                            // Radius
                            if (AddSlider(light.radius, 0.f, 8.f, 0.1f, "##spotLightRadius", "Radius", "Adjust the radius of the spot light"))
                            {
                                scene.lights[currentItem].dirty = true;
                                scene.lights[currentItem].data.radius = light.radius;
                            }

                            // Power
                            if (AddSlider(light.power, 0.f, 8.f, 0.1f, "##spotLightPower", "Power", "Adjust the power of the spot light"))
                            {
                                scene.lights[currentItem].dirty = true;
                                scene.lights[currentItem].data.power = light.power;
                            }

                            // Umbra Angle
                            if (AddSlider(light.umbraAngle, 0.f, 360.f, 1.f, "##spotLightUmbraAngle", "Umbra Angle", "Adjust the umbra angle of the spot light"))
                            {
                                scene.lights[currentItem].dirty = true;
                                scene.lights[currentItem].data.umbraAngle = light.umbraAngle;
                            }

                            // Penumbra Angle
                            if (AddSlider(light.penumbraAngle, 0.f, 360.f, 1.f, "##spotLightPenumbraAngle", "Penumbra Angle", "Adjust the penumbra angle of the spot light"))
                            {
                                scene.lights[currentItem].dirty = true;
                                scene.lights[currentItem].data.penumbraAngle = light.penumbraAngle;
                            }
                            anyLightChanged = anyLightChanged || scene.lights[currentItem].dirty;
                        }
                        else
                        {
                            ImGui::Text("No spot lights");
                        }
                        ImGui::EndTabItem();
                    }

                    // Point Light(s)
                    if (ImGui::BeginTabItem("Point"))
                    {
                        if (scene.numPointLights > 0)
                        {
                            // Add a drop down with all point lights
                            static int currentItem = scene.firstPointLight;
                            const char* label = scene.lights[currentItem].name.c_str();
                            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                            if (ImGui::BeginCombo("##pointLightSelect", label, ImGuiComboFlags_None))
                            {
                                for (int i = 0; i < static_cast<int>(scene.lights.size()); i++)
                                {
                                    if (scene.lights[i].type == ELightType::POINT)
                                    {
                                        const bool selected = (currentItem == i);
                                        if (ImGui::Selectable(scene.lights[i].name.c_str(), selected)) currentItem = i;
                                        if (selected) ImGui::SetItemDefaultFocus();
                                    }
                                }
                                ImGui::EndCombo();
                            }

                            // Get the selected light
                            Graphics::Light light = scene.lights[currentItem].data;

                            // Position
                            if (AddFloat3Slider(light.position, step, -max, max, "##pointLightPosition", "Position", "Adjust the position of the point light"))
                            {
                                scene.lights[currentItem].dirty = true;
                                scene.lights[currentItem].data.position = light.position;
                            }

                            // Color
                            if (AddColorSlider(light.color, "##pointLightColor", "Adjust the color of the point light"))
                            {
                                scene.lights[currentItem].dirty = true;
                                scene.lights[currentItem].data.color = light.color;
                            }

                            // Radius
                            if (AddSlider(light.radius, 0.f, 8.f, 0.1f, "##pointLightRadius", "Radius", "Adjust the radius of the point light"))
                            {
                                scene.lights[currentItem].dirty = true;
                                scene.lights[currentItem].data.radius = light.radius;
                            }

                            // Power
                            if (AddSlider(light.power, 0.f, 8.f, 0.1f, "##pointLightPower", "Power", "Adjust the power of the point light"))
                            {
                                scene.lights[currentItem].dirty = true;
                                scene.lights[currentItem].data.power = light.power;
                            }

                            anyLightChanged = anyLightChanged || scene.lights[currentItem].dirty;
                        }
                        else
                        {
                            ImGui::Text("No point lights");
                        }
                        ImGui::EndTabItem();
                    }
                    ImGui::Dummy(ImVec2(0, 3.f));
                    ImGui::EndTabBar();
                }

            }
            PopColorStyle();

            // Path Tracer Options
            PushColorStyle(110, 110, 70);
            if (ImGui::CollapsingHeader("Path Tracer", ImGuiTreeNodeFlags_CollapsingHeader))
            {
                ImGui::Checkbox("Enable##pt", &config.pathTrace.enabled);
                ImGui::SameLine(); AddQuestionMark("Toggles the path tracing renderer");

                if (config.pathTrace.enabled)
                {
                    config.app.renderMode = ERenderMode::PATH_TRACE;
                    config.ddgi.enabled = false;
                    config.rtao.enabled = false;

                    int numBounces = static_cast<int>(config.pathTrace.numBounces);
                    int numPaths = static_cast<int>(config.pathTrace.samplesPerPixel);

                    ImGui::PushItemWidth(ImGui::GetWindowWidth());

                    ImGui::Checkbox("Antialiasing", &config.pathTrace.antialiasing);
                    ImGui::SameLine(); AddQuestionMark("Enable or disable antialiasing");

                    if (gfx.supportsShaderExecutionReordering)
                    {
                        ImGui::Checkbox("Shader Execution Reordering", &config.pathTrace.shaderExecutionReordering);
                        ImGui::SameLine(); AddQuestionMark("Enable or disable shader execution reordering (RTX 4000 series)");
                    }

                    ImGui::DragFloat("##ptNormalBias", &config.pathTrace.rayNormalBias, 0.0001f, 0.f, 10.f, "Ray Normal Bias: %.4f");
                    AddHoverToolTip("A world-space distance along the surface normal, used to avoid self intersection");

                    ImGui::DragFloat("##ptViewBias", &config.pathTrace.rayViewBias, 0.0001f, 0.f, 10.f, "Ray View Bias: %.4f");
                    AddHoverToolTip("A world-space distance along the camera's view ray to the surface, used to avoid self intersection");

                    ImGui::DragInt("##ptNumPaths", &numPaths, 1, 1, 10, "Paths per Pixel: %.i");
                    AddHoverToolTip("The number of paths per pixel traced each frame");

                    ImGui::DragInt("##ptNumBounces", &numBounces, 1, 1, 20, "Bounces Per Path: %.i");
                    AddHoverToolTip("The maximum number of bounces allowed per path");

                    config.pathTrace.numBounces = static_cast<uint32_t>(numBounces);
                    config.pathTrace.samplesPerPixel = static_cast<uint32_t>(numPaths);

                    if (ImGui::Button("Reload Shaders"))
                    {
                        config.pathTrace.reload = true;
                    }
                }
                else
                {
                    config.app.renderMode = ERenderMode::DDGI;
                    config.pathTrace.enabled = false;
                }
            }
            PopColorStyle();

            // DDGI Options
            PushColorStyle(65, 95, 70);
            if (ImGui::CollapsingHeader("RTXGI: DDGI", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_CollapsingHeader))
            {
                ImGui::Checkbox("Enable##ddgi", &config.ddgi.enabled);
                ImGui::SameLine(); AddQuestionMark("Toggles RTXGI dynamic diffuse global illumination (DDGI). Press '1' on the keyboard for a shortcut.");

                if (config.ddgi.enabled)
                {
                    config.app.renderMode = ERenderMode::DDGI;
                    config.pathTrace.enabled = false;
                }

                if (config.ddgi.enabled && volumes.size() > 0)
                {
                    if (ImGui::Checkbox("Insert Performance Markers##ddgi-perf-markers", &config.ddgi.insertPerfMarkers))
                    {
                        rtxgi::SetInsertPerfMarkers(config.ddgi.insertPerfMarkers);
                    }
                    ImGui::SameLine(); AddQuestionMark("Toggle inserting DDGI performance markers in the graphics command list.");

                    if (gfx.supportsShaderExecutionReordering)
                    {
                        ImGui::Checkbox("Shader Execution Reordering", &config.ddgi.shaderExecutionReordering);
                        ImGui::SameLine(); AddQuestionMark("Enable or disable shader execution reordering (RTX 4000 series)");
                    }

                    ImGui::Checkbox("Show Indirect Lighting", &config.ddgi.showIndirect);
                    ImGui::SameLine(); AddQuestionMark("Show only the indirect lighting contribution. Press '2' on the keyboard for a shortcut.");

                    ImGui::Checkbox("Probe Visualization", &config.ddgi.showProbes);
                    ImGui::SameLine(); AddQuestionMark("Toggles a visualization of DDGI probes for all volumes that have the \"Show Probes\" option selected. Press 'P' on the keyboard for a shortcut.");

                    ImGui::Checkbox("Texture Visualization", &config.ddgi.showTextures);
                    ImGui::SameLine(); AddQuestionMark("Visualize the volume's probe textures. Press 'T' on the keyboard for a shortcut.");

                    // Adjust slider to the scene's scale
                    float max, step;
                    GetSceneScaleSliderValues(scene, max, step);

                    if (config.ddgi.showTextures)
                    {
                        Configs::DDGIVolume& selectedVolumeConfig = config.ddgi.volumes[config.ddgi.selectedVolume];

                        AddSlider(selectedVolumeConfig.probeIrradianceScale, 0.f, 10.f, 0.1f, "##irradianceTextureScale", "Irradiance Texture Scale", "Adjust the display size of the volume's irradiance texture data");
                        AddSlider(selectedVolumeConfig.probeDistanceScale, 0.f, 10.f, 0.1f, "##distanceTextureScale", "Distance Texture Scale", "Adjust the display size of the volume's distance texture data");
                        AddSlider(selectedVolumeConfig.probeVariabilityScale, 0.f, 10.f, 0.1f, "##variTextureScale", "Variability Texture Scale", "Adjust the display size of the volume's variability texture data");

                        if (selectedVolumeConfig.probeClassificationEnabled)
                        {
                            AddSlider(selectedVolumeConfig.probeDataScale, 0.f, 10.f, 0.1f, "##probeDataTextureScale", "Probe Data Texture Scale", "Adjust the display size of the volume's probe classification state texture data");
                        }
                        AddSlider(selectedVolumeConfig.probeRayDataScale, 0.f, 10.f, 0.1f, "##rayDataTextureScale", "Ray Data Texture Scale", "Adjust the display size of the volume's ray data texture data");

                        ImGui::NewLine();
                        AddSlider(selectedVolumeConfig.probeDistanceDivisor, 0.f, max, step, "##probeDistanceDivisor", "Distance Normalization", "Adjust the size of the divisor used to normalize probe distance values before display");
                    }

                    AddHRule();

                    // Add a drop down with all DDGIVolumes
                    const char* label = config.ddgi.volumes[config.ddgi.selectedVolume].name.c_str();
                    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                    if (ImGui::BeginCombo("##ddgiVolumeSelect", label, ImGuiComboFlags_None))
                    {
                        for (int i = 0; i < static_cast<int>(config.ddgi.volumes.size()); i++)
                        {
                            const bool selected = (config.ddgi.selectedVolume == i);
                            if (ImGui::Selectable(config.ddgi.volumes[i].name.c_str(), selected)) config.ddgi.selectedVolume = i;
                            if (selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    AddHoverToolTip("Select a DDGIVolume");
                    ImGui::Indent(5.f);

                    // Get the selected volume
                    rtxgi::DDGIVolumeBase* volume = volumes[config.ddgi.selectedVolume];
                    rtxgi::DDGIVolumeDesc desc = volume->GetDesc();

                    ImGui::Text("Volume Index: %i", volume->GetIndex());

                    std::stringstream msg;
                    msg << "Probes (" << std::to_string(desc.probeCounts.x) << ", " << std::to_string(desc.probeCounts.y) << ", " << std::to_string(desc.probeCounts.z) << ")";
                    AddIntQuantityText(volume->GetNumProbes(), msg.str());
                    AddIntQuantityText(desc.probeNumRays, "Rays Per Probe");
                    AddIntQuantityText(desc.probeNumRays * volume->GetNumProbes(), "Probe Rays Per Frame (max)");
                    AddIntQuantityText(desc.probeNumRays * volume->GetNumProbes() * 2, "Rays Per Frame (max) - includes shadow rays");
                    if (volume->GetProbeVariabilityEnabled())
                    {
                        AddFloatQuantityText(volume->GetVolumeAverageVariability(), "Probe Variability Average");
                    }

                    int memory = (int)ceil((float)volume->GetGPUMemoryUsedInBytes() / 1024.f);
                    AddIntQuantityText(memory, "KiB of GPU memory used");

                    // Clear probes button
                    if (ImGui::Button("Clear Probes"))
                    {
                        config.ddgi.volumes[config.ddgi.selectedVolume].clearProbes = 1;
                        volumeChanged[config.ddgi.selectedVolume] = true;
                    }

                    if (ImGui::Checkbox("Insert Performance Markers##ddgivolume-perf-markers", &config.ddgi.volumes[config.ddgi.selectedVolume].insertPerfMarkers))
                    {
                        volume->SetInsertPerfMarkers(config.ddgi.volumes[config.ddgi.selectedVolume].insertPerfMarkers);
                    }
                    ImGui::SameLine(); AddQuestionMark("Toggle inserting volume-specific performance markers in the graphics command list.");

                    if (ImGui::Checkbox("Show Probes", &config.ddgi.volumes[config.ddgi.selectedVolume].showProbes))
                    {
                        volume->SetShowProbes(config.ddgi.volumes[config.ddgi.selectedVolume].showProbes);
                    }
                    ImGui::SameLine(); AddQuestionMark("Toggle visualizing volume's probes.");

                    if (config.ddgi.volumes[config.ddgi.selectedVolume].showProbes)
                    {
                        ImGui::Indent(20.f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
                        ImGui::Text(" Mode: ");
                        ImGui::SameLine();

                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 8);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2);

                        Configs::DDGIVolume& selectedVolumeConfig = config.ddgi.volumes[config.ddgi.selectedVolume];

                        static int probeVisType = static_cast<int>(selectedVolumeConfig.probeVisType);
                        ImGui::RadioButton("Default", &probeVisType, 0);
                        ImGui::SameLine();
                        ImGui::RadioButton("Hide Inactive Probes", &probeVisType, 1);
                        selectedVolumeConfig.probeVisType = static_cast<EDDGIVolumeProbeVisType>(probeVisType);
                        volume->SetProbeVisType(selectedVolumeConfig.probeVisType);

                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
                        ImGui::Text(" Data: ");
                        ImGui::SameLine();
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 8);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2);

                        static int probeType = 0;
                        ImGui::RadioButton("Irradiance", &probeType, 0);
                        ImGui::SameLine();
                        ImGui::RadioButton("Distance", &probeType, 1);
                        config.ddgi.volumes[config.ddgi.selectedVolume].probeType = probeType;

                        AddSlider(config.ddgi.volumes[config.ddgi.selectedVolume].probeDistanceDivisor, 0.f, max, step, "##probeDistanceDivisor2", "Distance Normalization", "Adjust the size of the divisor used to normalize probe distance values before display");
                        AddSlider(config.ddgi.volumes[config.ddgi.selectedVolume].probeRadius, 0.f, 10.f, 0.01f, "##volumeProbeRadius", "Probe Radius", "Adjust the radius of the spheres used to visualize probes");
                        //AddSlider(config.ddgi.volumes[config.ddgi.selectedVolume].probeAlpha, 0.f, 1.f, 0.01f, "##volumeProbeAlpha", "Probe Alpha", "Adjust how opaque or transparent the visualized probes appear");

                        ImGui::Unindent(20.f);
                    }

                    AddTextSeparator();

                    // Probe Relocation options
                    {
                        if (ImGui::Checkbox("Probe Relocation", &config.ddgi.volumes[config.ddgi.selectedVolume].probeRelocationEnabled))
                        {
                            volume->SetProbeRelocationEnabled(config.ddgi.volumes[config.ddgi.selectedVolume].probeRelocationEnabled);

                            // Reset all probes offsets if relocation is disabled
                            if (!config.ddgi.volumes[config.ddgi.selectedVolume].probeRelocationEnabled)
                            {
                                volume->SetProbeRelocationNeedsReset(true);
                            }
                        }
                        ImGui::SameLine(); AddQuestionMark("Probe relocation attempts to move probes (within their voxel) if they are near or inside geometry");

                        if (config.ddgi.volumes[config.ddgi.selectedVolume].probeRelocationEnabled)
                        {
                            ImGui::Indent(20.f);

                            // Reset button
                            if (ImGui::Button("Reset Probe Relocation"))
                            {
                                volume->SetProbeRelocationNeedsReset(true);
                            }

                            // MinFrontfaceDistance Slider
                            if (AddSlider(config.ddgi.volumes[config.ddgi.selectedVolume].probeMinFrontfaceDistance, 0.f, max, step, "##relocationMinFrontfaceDistance", "Minimum Distance to Front-faces", "Adjust the minimum allowed distance to front-facing geometry used during relocation"))
                            {
                                volume->SetMinFrontFaceDistance(config.ddgi.volumes[config.ddgi.selectedVolume].probeMinFrontfaceDistance);
                            }

                            ImGui::Unindent(20.f);
                        }
                    }

                    // Probe Classification options
                    {
                        if (ImGui::Checkbox("Probe Classification", &config.ddgi.volumes[config.ddgi.selectedVolume].probeClassificationEnabled))
                        {
                            volume->SetProbeClassificationEnabled(config.ddgi.volumes[config.ddgi.selectedVolume].probeClassificationEnabled);

                            // (Re)activate all probes if classification is disabled
                            if (!config.ddgi.volumes[config.ddgi.selectedVolume].probeClassificationEnabled)
                            {
                                volume->SetProbeClassificationNeedsReset(true);
                            }
                        }
                        ImGui::SameLine(); AddQuestionMark("Probe classification determines if probes should be enabled or disabled. When disabled, probes do not trace rays and are not updated with new irradiance and distance data.");

                        if (config.ddgi.volumes[config.ddgi.selectedVolume].probeClassificationEnabled)
                        {
                            // Reset button
                            ImGui::Indent(20.f);
                            if (ImGui::Button("Reset Probe Classification"))
                            {
                                volume->SetProbeClassificationNeedsReset(true);
                            }
                            ImGui::Unindent(20.f);
                        }
                    }

                    // Probe Fixed Ray Backface Threshold (blending)
                    if(config.ddgi.volumes[config.ddgi.selectedVolume].probeRelocationEnabled || config.ddgi.volumes[config.ddgi.selectedVolume].probeClassificationEnabled)
                    {
                        ImGui::Indent(20.f);

                        if (AddSlider(desc.probeFixedRayBackfaceThreshold, 0.f, 1.f, 0.01f, "##volumeProbeFixedRayBackfaceThreshold", "Probe Fixed Ray Backface Threshold", "Adjust the ratio of backfaces a probe's fixed rays can hit before the probe is considered inside geometry"))
                        {
                            volume->SetProbeFixedRayBackfaceThreshold(desc.probeFixedRayBackfaceThreshold);
                        }

                        ImGui::Unindent(20.f);
                    }

                    // Probe Variability options
                    {
                        if (ImGui::Checkbox("Probe Variability", &config.ddgi.volumes[config.ddgi.selectedVolume].probeVariabilityEnabled))
                        {
                            volume->SetProbeVariabilityEnabled(config.ddgi.volumes[config.ddgi.selectedVolume].probeVariabilityEnabled);
                        }
                        ImGui::SameLine(); AddQuestionMark("Probe variability tracks the coefficient of variation of the volume over time. Users can use this value as a proxy for how converged a volume is.");
                        
                        if (config.ddgi.volumes[config.ddgi.selectedVolume].probeVariabilityEnabled)
                        {
                            AddSlider(config.ddgi.volumes[config.ddgi.selectedVolume].probeVariabilityThreshold, 0.f, 1.f, 0.001f, "##variTextureThreshold", "Variability Threshold", "Variability threshold below which the volume is considered converged and will stop updates.");
                        }
                    }

                    // Infinite Scrolling options
                    {
                        bool type = (volume->GetMovementType() == EDDGIVolumeMovementType::Scrolling);
                        if (ImGui::Checkbox("Infinite Scrolling Volume", &type))
                        {
                            if (type) volume->SetMovementType(EDDGIVolumeMovementType::Scrolling);
                            else volume->SetMovementType(EDDGIVolumeMovementType::Default);
                        }
                        ImGui::SameLine(); AddQuestionMark("Infinite scrolling volumes reuse probes as the volume moves by moving \"planes\" of probes from one side of the volume to the other, in the direction of movement. This method keeps all probes stationary for as long as possible to maximize irradiance stability.");

                        if (volume->GetMovementType() == EDDGIVolumeMovementType::Scrolling)
                        {
                            float3 effectiveOrigin = volume->GetOrigin();
                            int3 scrollOffsets = volume->GetScrollOffsets();

                            ImGui::Indent(25.f);
                            ImGui::Text("Effective Origin: (%.2f, %.2f, %.2f)", effectiveOrigin.x, effectiveOrigin.y, effectiveOrigin.z);
                            ImGui::Text("Scroll Offsets: (%i, %i, %i)", scrollOffsets.x, scrollOffsets.y, scrollOffsets.z);
                            ImGui::Unindent(25.f);
                        }
                    }

                    AddTextSeparator();

                    if (volume->GetMovementType() == EDDGIVolumeMovementType::Default)
                    {
                        // Origin
                        if (AddFloat3Slider(desc.origin, step, -max, max, "##volumeOrigin", "Origin", "Adjust the world-space position of the DDGIVolume's origin"))
                        {
                            volume->SetOrigin(desc.origin);
                            volumeChanged[config.ddgi.selectedVolume] = true;
                        }

                        // Rotation
                        float3 degrees { rtxgi::RadiansToDegrees(desc.eulerAngles.x), rtxgi::RadiansToDegrees(desc.eulerAngles.y), rtxgi::RadiansToDegrees(desc.eulerAngles.z)};
                        if (AddFloat3Slider(degrees, 0.1f, -360.f, 360.f, "##volumeRotation", "Rotation (degrees)", "Adjust the rotation of the DDGIVolume (in degrees)"))
                        {
                            desc.eulerAngles.x = rtxgi::DegreesToRadians(degrees.x);
                            desc.eulerAngles.y = rtxgi::DegreesToRadians(degrees.y);
                            desc.eulerAngles.z = rtxgi::DegreesToRadians(degrees.z);
                            volume->SetEulerAngles(desc.eulerAngles);
                            volumeChanged[config.ddgi.selectedVolume] = true;
                        }
                    }
                    else if(volume->GetMovementType() == EDDGIVolumeMovementType::Scrolling)
                    {
                        // Scrolling Anchor
                        float3 anchor = volume->GetScrollAnchor();
                        if (AddFloat3Slider(anchor, step, -10000.f, 10000.f, "##volumeScrollingAnchor", "Scrolling Anchor", "The world-space position of the infinite scrolling volume tries to move its origin to"))
                        {
                            volume->SetScrollAnchor({ anchor.x, anchor.y, anchor.z });
                            volumeChanged[config.ddgi.selectedVolume] = true;
                        }
                    }

                    // Probe Spacing
                    if (AddFloat3Slider(desc.probeSpacing, step, 0.f, max, "##volumeProbeSpacing", "Probe Spacing", "Adjust the world-space distance between probes"))
                    {
                        volume->SetProbeSpacing(desc.probeSpacing);
                        volumeChanged[config.ddgi.selectedVolume] = true;
                    }

                    AddTextSeparator();

                    // Probe Maximum Ray Distance
                    if (AddSlider(desc.probeMaxRayDistance, 0.f, max * 2.f, 0.1f, "##volumeProbeMaxRayDistance", "Probe Max Ray Distance", "Adjust the maximum distance rays traced from probes may travel"))
                    {
                        volume->SetProbeMaxRayDistance(desc.probeMaxRayDistance);
                    }

                    // Probe Hysteresis
                    if (AddSlider(desc.probeHysteresis, 0.f, 1.f, 0.01f, "###volumeProbeHysteresis", "Probe Hysteresis", "Adjust the probe hysteresis. Probe hysteresis controls the influence of new data when updating probes. A hysteresis value close to 1.0 slowly changes probe data, improving stability but increasing lighting latency. Values of 0.9 or lower will rapidly react to scene changes, but may cause flickering or other instabilities in the lighting."))
                    {
                        volume->SetProbeHysteresis(desc.probeHysteresis);
                    }

                    // Probe View Bias
                    float maxViewBias = fmax(fmax(desc.probeSpacing.x, desc.probeSpacing.y), desc.probeSpacing.z);
                    float viewBiasStep = maxViewBias / 10.f;
                    if (AddSlider(desc.probeViewBias, 0.f, maxViewBias, viewBiasStep, "###volumeProbeViewBias", "Probe View Bias", "Adjust the probe view bias. View bias is a small offset along the camera view ray applied to the shaded surface point to avoid numerical instabilities when determining visibility."))
                    {
                        volume->SetProbeViewBias(desc.probeViewBias);
                    }

                    // Probe Normal Bias
                    if (AddSlider(desc.probeNormalBias, 0.f, 1.f, 0.01f, "###volumeProbeNormalBias", "Probe Normal Bias", "Adjust the probe normal bias. Normal bias is a small offset along the surface normal applied to the shaded surface point to avoid numerical instabilities when determining visibility."))
                    {
                        volume->SetProbeNormalBias(desc.probeNormalBias);
                    }

                    // Probe Distance Exponent
                    if (AddSlider(desc.probeDistanceExponent, 0.f, 100.f, 0.1f, "###volumeProbeDistanceExponent", "Probe Distance Exponent", "Adjust the probe distance exponent. The distance exponent is used during visibility testing. A high value rapidly reacts to depth discontinuities but may cause banding."))
                    {
                        volume->SetProbeDistanceExponent(desc.probeDistanceExponent);
                    }

                    // Probe Irradiance Encoding Gamma
                    if (AddSlider(desc.probeIrradianceEncodingGamma, 0.f, 10.f, 0.1f, "###volumeProbeIrradianceEncodingGamma", "Probe Irradiance Encoding Gamma", "Adjust the probe irradiance encoding gamma. Irradiance encoding gamma is an exponent that perceptually encodes irradiance for faster light-to-dark convergence."))
                    {
                        volume->SetIrradianceEncodingGamma(desc.probeIrradianceEncodingGamma);
                    }

                    // Probe Irradiance Threshold
                    if (AddSlider(desc.probeIrradianceThreshold, 0.f, 1.f, 0.01f, "###volumeProbeIrradianceThreshold", "Probe Irradiance Threshold", "Adjust the probe irradiance threshold. This specifies a threshold to identify when large lighting changes occur. When the difference in the maximum color components between the previous and current irradiance is larger than this threshold, the hysteresis will be reduced."))
                    {
                        volume->SetProbeIrradianceThreshold(desc.probeIrradianceThreshold);
                    }

                    // Probe Brightness Threshold
                    if (AddSlider(desc.probeBrightnessThreshold, 0.f, 1.f, 0.01f, "###volumeProbeBrightnessThreshold", "Probe Brightness Threshold", "Adjust the probe brightness threshold. This specifies a threshold value used during probe radiance blending that determines the maximum allowed difference in brightness between the previous and current irradiance values. This prevents impulses from drastically changing a texel�s irradiance in a single update cycle."))
                    {
                        volume->SetProbeBrightnessThreshold(desc.probeBrightnessThreshold);
                    }

                    // Probe Random Ray Backface Threshold (blending)
                    if (AddSlider(desc.probeRandomRayBackfaceThreshold, 0.f, 1.f, 0.01f, "##volumeProbeRandomRayBackfaceThreshold", "Probe Random Ray Backface Threshold", "Adjust the ratio of backfaces a probe's random rays can hit before the probe is considered inside geometry"))
                    {
                        volume->SetProbeRandomRayBackfaceThreshold(desc.probeRandomRayBackfaceThreshold);
                    }

                    AddTextSeparator();

                    if (ImGui::Button("Reload Shaders"))
                    {
                        config.ddgi.reload = true;
                    }
                }
            }
            PopColorStyle();


            // If the sky changed, any light changed, or the volume changed, probe variability data is no longer valid and should be cleared
            for (uint32_t volumeIndex = 0; volumeIndex < static_cast<uint32_t>(config.ddgi.volumes.size()); volumeIndex++)
            {
                Configs::DDGIVolume& volume = config.ddgi.volumes[volumeIndex];
                if (volume.probeVariabilityEnabled)
                {
                    volume.clearProbeVariability = (skyChanged || anyLightChanged || volumeChanged[volumeIndex]);
                }
            }


            // RTAO Options
            PushColorStyle(90, 60, 70);
            if (ImGui::CollapsingHeader("Ray Traced Ambient Occlusion", ImGuiTreeNodeFlags_CollapsingHeader))
            {
                ImGui::Checkbox("Enable##rtao", &config.rtao.enabled);
                ImGui::SameLine(); AddQuestionMark("Toggles ray traced ambient occlusion");

                if (config.rtao.enabled)
                {
                    ImGui::Checkbox("Visualize", &config.rtao.visualize);
                    ImGui::SameLine(); AddQuestionMark("Toggles a visualization of the ray traced ambient occlusion result");

                    ImGui::PushItemWidth(ImGui::GetWindowWidth());

                    ImGui::DragFloat("###RTAORayLength", &config.rtao.rayLength, 0.01f, 0.0f, 50.f, "Ray Length: %.2f");
                    AddHoverToolTip("The maximum distance a ray may travel when determining ambient occlusion");

                    ImGui::DragFloat("###RTAORayNormalBias", &config.rtao.rayNormalBias, 0.0001f, 0.f, 5.f, "Ray Normal Bias: %.4f");
                    AddHoverToolTip("A world-space distance along the surface normal, used to avoid self intersection");

                    ImGui::DragFloat("###RTAORayViewBias", &config.rtao.rayViewBias, 0.0001f, 0.f, 5.f, "Ray View Bias: %.4f");
                    AddHoverToolTip("A world-space distance along the camera's view ray to the surface, used to avoid self intersection");

                    ImGui::DragFloat("###RTAOPower", &config.rtao.powerLog, 0.01f, -10.f, 10.f, "AO Power (log2): %.2f");
                    AddHoverToolTip("When a ray hits a nearby surface the ratio of hit distance to max ray length is raised to this power to derive an occlusion value");

                    ImGui::DragFloat("###RTAOFilterDistanceSigma", &config.rtao.filterDistanceSigma, 0.1f, 0.f, 20.f, "Filter Distance Sigma: %.1f");
                    AddHoverToolTip("The sigma for the Gaussian weight for distance differences, for the bilateral filter");

                    ImGui::DragFloat("###RTAOFilterDepthSigma", &config.rtao.filterDepthSigma, 0.1f, 0.f, 20.f, "Filter Depth Sigma: %.1f");
                    AddHoverToolTip("The sigma for the Gaussian weight for color differences, for the bilateral filter");

                    if (ImGui::Button("Reload Shaders"))
                    {
                        config.rtao.reload = true;
                    }
                }
            }
            PopColorStyle();

            // Post Process Options
            PushColorStyle(70, 65, 100);
            if (ImGui::CollapsingHeader("Post Process", ImGuiTreeNodeFlags_CollapsingHeader))
            {
                ImGui::Checkbox("Enable##post", &config.postProcess.enabled);
                ImGui::SameLine(); AddQuestionMark("Toggles post processing effects");

                if (config.postProcess.enabled)
                {
                    ImGui::Checkbox("Exposure", &config.postProcess.exposure.enabled);
                    ImGui::SameLine(); AddQuestionMark("Enable or disable exposure");

                    if (config.postProcess.exposure.enabled)
                    {
                        ImGui::PushItemWidth(ImGui::GetWindowWidth());

                        ImGui::DragFloat("##PostProcessExposure", &config.postProcess.exposure.fstops, 0.01f, -8.f, 8.f, "Exposure (f-stops): %.1f");
                        AddHoverToolTip("The camera's exposure in f-stops");
                    }

                    ImGui::Checkbox("Tonemapping", &config.postProcess.tonemap.enabled);
                    ImGui::SameLine(); AddQuestionMark("Enable or disable tonemapping");

                    ImGui::Checkbox("Color Banding Reduction", &config.postProcess.dither.enabled);
                    ImGui::SameLine(); AddQuestionMark("Enable or disable blue noise dithering to reduce color banding");

                    ImGui::Checkbox("Gamma Correction", &config.postProcess.gamma.enabled);
                    ImGui::SameLine(); AddQuestionMark("Enable or disable gamma correction");

                    if (ImGui::Button("Reload Shaders"))
                    {
                        config.postProcess.reload = true;
                    }
                }
            }
            PopColorStyle();

            ImGui::EndDisabled();

            ImGui::SetWindowPos("Settings (Press 'u' to hide)", ImVec2((gfx.width - ImGui::GetWindowWidth() - 20.f), 20));
            ImGui::End();
        }

        uint32_t GetPerfColor(double average)
        {
            if (average > 15 && average < 17) return 1;
            if (average > 17) return 2;
            return 0;
        }

        /**
         * Creates the detailed performance debug window.
         */
        void CreatePerfWindow(Graphics::Globals& gfx, const Configs::Config& config, const Instrumentation::Performance& performance)
        {
            // Early out if the window shouldn't be open
            if (!config.app.showPerf) return;

            SetupStyle();

            // Size the debug window based on the application height
            ImGui::SetNextWindowSize(ImVec2(perfWindowWidth, 550.f));
            ImGui::Begin("Detailed Performance", NULL, ImGuiWindowFlags_AlwaysAutoResize);

            ImVec4 colors[3] =
            {
                ImVec4(1.f, 1.f, 1.f, 1.f),
                ImVec4(0.851f, 0.807f, 0.365f, 1.f),
                ImVec4(1.f, 0.f, 0.f, 1.f),
            };

            // GPU Performance
            ImGui::TextColored(ImVec4(0.f, 0.71f, 0.071f, 1.f), "GPU");

            // Display GPU times
            uint32_t cIdx = GetPerfColor(performance.gpuTimes[0]->average);
            ImGui::TextColored(colors[cIdx], "%s: %.3lf ms (avg, last %u frames)", performance.gpuTimes[0]->name.c_str(), performance.gpuTimes[0]->average, performance.gpuTimes[0]->sampleSize);
            ImGui::Separator();

            if (config.app.renderMode == ERenderMode::PATH_TRACE)
            {
                cIdx = GetPerfColor(performance.gpuTimes[1]->average);
                ImGui::TextColored(colors[cIdx], "%s: %.3lf ms", performance.gpuTimes[1]->name.c_str(), performance.gpuTimes[1]->average);
            }
            else if(config.app.renderMode == ERenderMode::DDGI)
            {
                for (size_t index = 2; index < (performance.gpuTimes.size() - 1); index++)
                {
                    cIdx = GetPerfColor(performance.gpuTimes[index]->average);
                    ImGui::TextColored(colors[cIdx], "%s: %.3lf ms", performance.gpuTimes[index]->name.c_str(), performance.gpuTimes[index]->average);
                }
            }

            // UI
            cIdx = GetPerfColor(performance.gpuTimes.back()->average);
            ImGui::TextColored(colors[cIdx], "%s: %.3lf ms", performance.gpuTimes.back()->name.c_str(), performance.gpuTimes.back()->average);

            ImGui::Separator();
            ImGui::NewLine();

            // CPU Performance
            double gfxTotal = 0;
            ImGui::TextColored(ImVec4(0.259f, 0.529f, 0.961f, 1.f), "CPU");

            cIdx = GetPerfColor(performance.cpuTimes[0]->average);
            ImGui::TextColored(colors[cIdx], "%s: %.3lf ms (avg, last %u frames)", performance.cpuTimes[0]->name.c_str(), performance.cpuTimes[0]->average, performance.cpuTimes[0]->sampleSize);
            ImGui::Separator();
            ImGui::Text("%s: %.3lf ms", performance.cpuTimes[1]->name.c_str(), performance.cpuTimes[1]->average); // input
            ImGui::Text("%s: %.3lf ms", performance.cpuTimes[2]->name.c_str(), performance.cpuTimes[2]->average); // update

            ImGui::Separator();

            if (config.app.renderMode == ERenderMode::PATH_TRACE)
            {
                gfxTotal += performance.cpuTimes[3]->average;
                cIdx = GetPerfColor(performance.cpuTimes[3]->average);
                ImGui::TextColored(colors[cIdx], "%s: %.3lf ms", performance.cpuTimes[3]->name.c_str(), performance.cpuTimes[3]->average);
            }
            else if (config.app.renderMode == ERenderMode::DDGI)
            {
                for (size_t index = 4; index < performance.cpuTimes.size() - 2; index++)
                {
                    gfxTotal += performance.cpuTimes[index]->average;
                    cIdx = GetPerfColor(performance.cpuTimes[index]->average);
                    ImGui::TextColored(colors[cIdx], "%s: %.3lf ms", performance.cpuTimes[index]->name.c_str(), performance.cpuTimes[index]->average);
                }
            }

            // UI
            gfxTotal += performance.cpuTimes[performance.cpuTimes.size() - 2]->average;
            cIdx = GetPerfColor(performance.cpuTimes[performance.cpuTimes.size() - 2]->average);
            ImGui::TextColored(colors[cIdx], "%s: %.3lf ms", performance.cpuTimes[performance.cpuTimes.size() - 2]->name.c_str(), performance.cpuTimes[performance.cpuTimes.size() - 2]->average);

            // Submit / Present
            gfxTotal += performance.cpuTimes.back()->average;
            cIdx = GetPerfColor(performance.cpuTimes.back()->average);
            ImGui::TextColored(colors[cIdx], "%s: %.3lf ms", performance.cpuTimes.back()->name.c_str(), performance.cpuTimes.back()->average);

            // Idle
            double idle = performance.cpuTimes[0]->average - (performance.cpuTimes[1]->average + performance.cpuTimes[2]->average + gfxTotal);
            ImGui::Text("GFX Total: %.3lf ms", gfxTotal);
            ImGui::Separator();
            ImGui::Text("Idle: %.3lf ms", idle);

            ImGui::Separator();

            ImGui::SetWindowPos("Detailed Performance", ImVec2((gfx.width - ImGui::GetWindowWidth() - debugWindowWidth - 20.f), 20));
            ImGui::End();
        }

    #if __linux__
        struct X11WindowData
        {
            Display* display;
            Window window;
            int screen;
            size_t width;
            size_t height;
            GC bgContext;
            GC textContext;
            XFontSet fontSet;
        };

        bool IntersectBox2D(int x, int y, RECT rect)
        {
            if(x < rect.left || x > rect.right || y < rect.top || y > rect.bottom) return false;
            return true;
        }

        bool InitX11Window(X11WindowData& data)
        {
            // Create an X11 display
            data.display = XOpenDisplay(0);
            if(!data.display) return false;

            // Get the screen
            data.screen = DefaultScreen(data.display);

            // Get the window
            data.window = XCreateSimpleWindow(
                data.display,
                RootWindow(data.display, data.screen),
                0, 0,
                data.width, data.height, 1,
                BlackPixel(data.display, data.screen),
                WhitePixel(data.display, data.screen));

            XSetStandardProperties(data.display, data.window, "Error!", "Error!", None, nullptr, 0, nullptr);
            XSelectInput(data.display, data.window, ExposureMask | PointerMotionMask | ButtonPressMask | ButtonReleaseMask);

            XMapWindow(data.display, data.window);
            Atom WM_DELETE_WINDOW = XInternAtom(data.display, "WM_DELETE_WINDOW", False);
            XSetWMProtocols(data.display, data.window, &WM_DELETE_WINDOW, 1);
            XUnmapWindow(data.display, data.window);

            // Create the graphics context
            data.bgContext = XCreateGC(data.display, data.window, 0, nullptr);
            XSetBackground(data.display, data.bgContext, WhitePixel(data.display, data.screen));
            XSetForeground(data.display, data.bgContext, BlackPixel(data.display, data.screen));

            // Create the text graphics context
            XGCValues textValues;
            textValues.font = XLoadFont(data.display, "7x13");
            textValues.foreground = BlackPixel(data.display, 0);
            data.textContext = XCreateGC(data.display, data.window, GCFont + GCForeground, &textValues);

            // Create the fontset
            char **charsetList = NULL;
            int charsetCount = 0;
            data.fontSet = XCreateFontSet(data.display, "-*-*-medium-r-*-*-*-140-75-75-*-*-*-*", &charsetList, &charsetCount, NULL);

            return true;
        }

        /**
         * Split a string by the delimiter into an vector of string tokens.
         */
        std::vector<std::string> Split(const std::string& line, const char delimiter = '.')
        {
            std::vector<std::string> tokens;
            std::istringstream stream(line);
            std::string token;

            while (getline(stream, token, delimiter))
            {
                tokens.push_back(token);
            }
            return tokens;
        }
    #endif

        //----------------------------------------------------------------------------------------------------------
        // Public Functions
        //----------------------------------------------------------------------------------------------------------

        bool CapturedMouse()
        {
            if (s_initialized) return ImGui::GetIO().WantCaptureMouse;
            return false;
        }

        bool CapturedKeyboard()
        {
            if (s_initialized) return ImGui::GetIO().WantCaptureKeyboard;
            return false;
        }

        bool MessageBox(std::string message)
        {
        #if defined(_WIN32) || defined(WIN32)
            std::wstring wstr = std::wstring(message.begin(), message.end());
            MessageBoxW(NULL, wstr.c_str(), L"Error!", MB_OK);
        #elif __linux__
            size_t charWidth = 9;
            size_t textWidth = strlen(message.c_str()) * charWidth;
            size_t textHeight = 13;
            size_t windowWidth = textWidth + 40;
            size_t windowHeight = 100;
            size_t btnWidth = 80;
            size_t btnHeight = 30;
            size_t btnPosX = ((windowWidth - btnWidth) / 2.f) + (btnWidth / 2.f);
            size_t btnPosY = 70;

            RECT btnExtent;
            btnExtent.left = btnPosX - (btnWidth / 2.f);
            btnExtent.right = btnPosX + (btnWidth / 2.f);
            btnExtent.top = btnPosY - (btnHeight / 2.f);
            btnExtent.bottom = btnPosY + (btnHeight / 2.f);

            // Initialize an X11 Window
            X11WindowData w;
            w.width = windowWidth;
            w.height = windowHeight;
            if(!InitX11Window(w)) return false;

            Atom WM_DELETE_WINDOW = XInternAtom(w.display, "WM_DELETE_WINDOW", False);

            // Create the inactive background color for the Okay button
            GC btnInactiveContext = XCreateGC(w.display, w.window, 0, 0);
            XColor btnInactiveColor;
            XParseColor(w.display, DefaultColormap(w.display, w.screen), "Gainsboro", &btnInactiveColor);
            XAllocColor(w.display, DefaultColormap(w.display, w.screen), &btnInactiveColor);
            XSetForeground(w.display, btnInactiveContext, btnInactiveColor.pixel);

            // Create the active background color for the Okay button
            GC btnActiveContext = XCreateGC(w.display, w.window, 0, 0);
            XColor btnActiveColor;
            XParseColor(w.display, DefaultColormap(w.display, w.screen), "Gray", &btnActiveColor);
            XAllocColor(w.display, DefaultColormap(w.display, w.screen), &btnActiveColor);
            XSetForeground(w.display, btnActiveContext, btnActiveColor.pixel);

            XClearWindow(w.display, w.window);
            XMapRaised(w.display, w.window);
            XFlush(w.display);

            bool close = false;
            while(!close)
            {
                XEvent event;
                XNextEvent(w.display, &event);

                // Message text
                XmbDrawString(w.display, w.window, w.fontSet, w.textContext, 20, 20 + textHeight, message.c_str(), strlen(message.c_str()));

                // Button
                if(IntersectBox2D(event.xmotion.x, event.xmotion.y, btnExtent))
                {
                    // Mouse is over the button
                    XFillRectangle(w.display, w.window, btnActiveContext, btnExtent.left, btnExtent.top, btnWidth, btnHeight);
                }
                else
                {
                    // Mouse is outside of the button
                    XFillRectangle(w.display, w.window, btnInactiveContext, btnExtent.left, btnExtent.top, btnWidth, btnHeight);
                }
                XmbDrawString(w.display, w.window, w.fontSet, w.textContext, btnPosX - (4 * charWidth) / 2.f, btnPosY + 5.5, "Okay", 4);

                switch(event.type)
                {
                    case ButtonRelease:
                        if(IntersectBox2D(event.xmotion.x, event.xmotion.y, btnExtent)) close = true;
                        break;
                    case ClientMessage:
                        if(event.xclient.data.l[0] == WM_DELETE_WINDOW) close = true;
                        break;
                    default:
                        break;
                }

                XFlush(w.display);
            }

            XDestroyWindow(w.display, w.window);
            XFreeFontSet(w.display, w.fontSet);
            XFreeGC(w.display, w.textContext);
            XFreeGC(w.display, btnActiveContext);
            XFreeGC(w.display, btnInactiveContext);
            XFreeGC(w.display, w.bgContext);
            XCloseDisplay(w.display);
        #endif

            return true;
        }

        bool MessageRetryBox(std::string message)
        {
            bool retry = false;

        #if defined(_WIN32) || defined(WIN32)
            std::wstring wstr = std::wstring(message.begin(), message.end());
            retry = (MessageBoxW(NULL, wstr.c_str(), L"Error!", MB_RETRYCANCEL) == IDRETRY);
        #elif __linux__
            // Split up the error message
            std::vector<std::string> lines = Split(message.c_str(), '\n');

            size_t charWidth = 9;
            size_t windowWidth = 0;
            for(size_t lineIndex = 0; lineIndex < lines.size(); lineIndex++)
            {
                windowWidth = std::max(windowWidth , std::strlen(lines[lineIndex].c_str()));
            }

            windowWidth *= charWidth;
            windowWidth += 60;
            windowWidth = std::min(windowWidth, (size_t)1920);

            size_t textHeight = 13;
            size_t btnWidth = 110;
            size_t btnHeight = 30;
            size_t btnPosX = ((windowWidth - btnWidth) / 2.f) + (btnWidth / 2.f);
            size_t btnPosY = 30 + (lines.size() * (textHeight + 2)) + 30;

            size_t windowHeight = btnPosY + btnHeight;

            RECT btnRetryExtent;
            btnRetryExtent.left = btnPosX - btnWidth - 2;
            btnRetryExtent.right = btnPosX - 2;
            btnRetryExtent.top = btnPosY - (btnHeight / 2.f);
            btnRetryExtent.bottom = btnPosY + (btnHeight / 2.f);

            RECT btnCancelExtent;
            btnCancelExtent.left = btnPosX + 2;
            btnCancelExtent.right = btnPosX + btnWidth + 2;
            btnCancelExtent.top = btnPosY - (btnHeight / 2.f);
            btnCancelExtent.bottom = btnPosY + (btnHeight / 2.f);

            // Initialize an X11 Window
            X11WindowData w;
            w.width = windowWidth;
            w.height = windowHeight;
            if(!InitX11Window(w)) return false;

            Atom WM_DELETE_WINDOW = XInternAtom(w.display, "WM_DELETE_WINDOW", False);

            // Create the inactive background color for the buttons
            GC btnInactiveContext = XCreateGC(w.display, w.window, 0, 0);
            XColor btnInactiveColor;
            XParseColor(w.display, DefaultColormap(w.display, w.screen), "Gainsboro", &btnInactiveColor);
            XAllocColor(w.display, DefaultColormap(w.display, w.screen), &btnInactiveColor);
            XSetForeground(w.display, btnInactiveContext, btnInactiveColor.pixel);

            // Create the active background color for the buttons
            GC btnActiveContext = XCreateGC(w.display, w.window, 0, 0);
            XColor btnActiveColor;
            XParseColor(w.display, DefaultColormap(w.display, w.screen), "Gray", &btnActiveColor);
            XAllocColor(w.display, DefaultColormap(w.display, w.screen), &btnActiveColor);
            XSetForeground(w.display, btnActiveContext, btnActiveColor.pixel);

            XClearWindow(w.display, w.window);
            XMapRaised(w.display, w.window);
            XFlush(w.display);

            bool close = false;
            while(!close)
            {
                XEvent event;
                XNextEvent(w.display, &event);

                // Message text
                for(size_t lineIndex = 0; lineIndex < lines.size(); lineIndex++)
                {
                    XmbDrawString(w.display, w.window, w.fontSet, w.textContext, 30, 30 + ((textHeight + 5) * lineIndex), lines[lineIndex].c_str(), strlen(lines[lineIndex].c_str()));
                }

                // Buttons
                if(IntersectBox2D(event.xmotion.x, event.xmotion.y, btnRetryExtent))
                {
                    // Mouse is over the retry button
                    XFillRectangle(w.display, w.window, btnActiveContext, btnRetryExtent.left, btnRetryExtent.top, btnWidth, btnHeight);
                }
                else
                {
                    // Mouse is outside of the retry button
                    XFillRectangle(w.display, w.window, btnInactiveContext, btnRetryExtent.left, btnRetryExtent.top, btnWidth, btnHeight);
                }
                XmbDrawString(w.display, w.window, w.fontSet, w.textContext, btnRetryExtent.left + 32, btnPosY + 5.5, "Retry", 5);

                if(IntersectBox2D(event.xmotion.x, event.xmotion.y, btnCancelExtent))
                {
                    XFillRectangle(w.display, w.window, btnActiveContext, btnCancelExtent.left, btnCancelExtent.top, btnWidth, btnHeight);
                }
                else
                {
                    XFillRectangle(w.display, w.window, btnInactiveContext, btnCancelExtent.left, btnCancelExtent.top, btnWidth, btnHeight);
                }
                XmbDrawString(w.display, w.window, w.fontSet, w.textContext, btnCancelExtent.left + 30, btnPosY + 5.5, "Cancel", 6);

                switch(event.type)
                {
                    case ButtonRelease:
                        if(IntersectBox2D(event.xmotion.x, event.xmotion.y, btnRetryExtent)) { close = true; retry = true; }
                        if(IntersectBox2D(event.xmotion.x, event.xmotion.y, btnCancelExtent)) { close = true; retry = false; }
                        break;
                    case ClientMessage:
                        if(event.xclient.data.l[0] == WM_DELETE_WINDOW) close = true;
                        break;
                    default:
                        break;
                }

                XFlush(w.display);
            }

            XDestroyWindow(w.display, w.window);
            XFreeFontSet(w.display, w.fontSet);
            XFreeGC(w.display, w.textContext);
            XFreeGC(w.display, btnActiveContext);
            XFreeGC(w.display, btnInactiveContext);
            XFreeGC(w.display, w.bgContext);
            XCloseDisplay(w.display);
        #endif

            return retry;
        }

    } // namespace Graphics::UI

} // namespace UI
