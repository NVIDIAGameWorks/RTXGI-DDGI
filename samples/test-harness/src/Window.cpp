/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "Window.h"

#include <stb_image.h>

using namespace DirectX;

namespace Windows
{
    static EWindowEvent state = EWindowEvent::NONE;
    const EWindowEvent GetWindowEvent() { return state; }
    void ResetWindowEvent() { state = EWindowEvent::NONE; }

    //----------------------------------------------------------------------------------------------------------
    // Event Handlers
    //----------------------------------------------------------------------------------------------------------

    /**
     * Handle frame buffer resize events.
     */
    void onFramebufferResize(GLFWwindow* window, int width, int height)
    {
        state = EWindowEvent::RESIZE;
    }

    /**
     * Create a window.
     */
    bool Create(Configs::Config& config, GLFWwindow*& window)
    {
        config.app.title.append(", ");
        config.app.title.append(config.scene.name);
    #if defined(API_D3D12)
        config.app.title.append(" (D3D12)");
    #elif defined(API_VULKAN)
        config.app.title.append(" (Vulkan)");
    #endif

        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);   // don't need an OpenGL context with D3D12/Vulkan
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        // Create the window with GLFW
        window = glfwCreateWindow(config.app.width, config.app.height, config.app.title.c_str(), nullptr, nullptr);
        if (!window) return false;

        // Add a GLFW hook to handle resize events
        glfwSetFramebufferSizeCallback(window, onFramebufferResize);

        // Load the nvidia logo and set it as the window icon
        GLFWimage icon;
        icon.pixels = stbi_load("nvidia.jpg", (int*)&(icon.width), (int*)&icon.height, nullptr, STBI_rgb_alpha);
        if (icon.pixels) glfwSetWindowIcon(window, 1, &icon);
        delete[] icon.pixels;
        icon.pixels = nullptr;

        return true;
    }

    /**
     * Close and destroy a window.
     */
    bool Close(GLFWwindow*& window)
    {
        glfwDestroyWindow(window);
        glfwTerminate();
        return true;
    }
}
