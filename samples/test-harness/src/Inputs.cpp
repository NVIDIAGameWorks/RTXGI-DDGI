/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "Inputs.h"
#include "UI.h"

#include <rtxgi/Math.h>

using namespace DirectX;

// Data structure references
Inputs::Input* inputPtr = nullptr;
Configs::Config* configPtr = nullptr;
Scenes::Scene* scenePtr = nullptr;

//----------------------------------------------------------------------------------------------------------
// Private Functions
//----------------------------------------------------------------------------------------------------------

bool IsKeyPressed(GLFWwindow* window, int key)
{
    int state = glfwGetKey(window, key);
    return (state == GLFW_PRESS || state == GLFW_REPEAT);
}

bool IsKeyReleased(int key, int action, int keycode)
{
    return ((key == keycode) && (action == GLFW_RELEASE));
}

//----------------------------------------------------------------------------------------------------------
// Input Event Handlers
//----------------------------------------------------------------------------------------------------------

/**
 * Handle keyboard inputs.
 */
void KeyHandler(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // ImGui captured the keyboard input, don't forward it
    if (Graphics::UI::CapturedKeyboard()) return;

    // Escape to quit
    if(IsKeyReleased(key, action, GLFW_KEY_ESCAPE))
    {
        inputPtr->event = Inputs::EInputEvent::QUIT;
        return;
    }

    // ALT + ENTER combo for fullscreen transitions
    if(IsKeyReleased(key, action, GLFW_KEY_ENTER) && mods == GLFW_MOD_ALT)
    {
        inputPtr->event = Inputs::EInputEvent::FULLSCREEN_CHANGE;
        return;
    }

    // Save a screenshot
    if(IsKeyReleased(key, action, GLFW_KEY_F1))
    {
        inputPtr->event = Inputs::EInputEvent::SCREENSHOT;
        return;
    }

    // Save debug images
    if (IsKeyReleased(key, action, GLFW_KEY_F2))
    {
        inputPtr->event = Inputs::EInputEvent::SAVE_IMAGES;
        return;
    }

    // Run benchmark
    if (IsKeyReleased(key, action, GLFW_KEY_F4))
    {
        inputPtr->event = Inputs::EInputEvent::RUN_BENCHMARK;
        return;
    }

    // Toggle pan inversion
    if(IsKeyReleased(key, action, GLFW_KEY_I))
    {
        configPtr->input.invertPan = !configPtr->input.invertPan;
        return;
    }

    // Toggle DDGI
    if(IsKeyReleased(key, action, GLFW_KEY_1))
    {
        configPtr->ddgi.enabled = !configPtr->ddgi.enabled;
        inputPtr->event = Inputs::EInputEvent::NONE;
        return;
    }

    // Toggle visualization of indirect lighting
    if (IsKeyReleased(key, action, GLFW_KEY_2))
    {
        configPtr->ddgi.showIndirect = !configPtr->ddgi.showIndirect;
        inputPtr->event = Inputs::EInputEvent::NONE;
        return;
    }

    // Toggle DDGIVolume Texture Visualization
    if(IsKeyReleased(key, action, GLFW_KEY_T))
    {
        configPtr->ddgi.showTextures = !configPtr->ddgi.showTextures;
        inputPtr->event = Inputs::EInputEvent::NONE;
        return;
    }

    // Toggle DDGIVolume Probe Visualization
    if(IsKeyReleased(key, action, GLFW_KEY_P))
    {
        configPtr->ddgi.showProbes = !configPtr->ddgi.showProbes;
        inputPtr->event = Inputs::EInputEvent::NONE;
        return;
    }

    // Reload
    if(IsKeyReleased(key, action, GLFW_KEY_R))
    {
        inputPtr->event = Inputs::EInputEvent::RELOAD;
        return;
    }

    // Toggle UI
    if(IsKeyReleased(key, action, GLFW_KEY_U))
    {
        configPtr->app.showUI = !configPtr->app.showUI;
        inputPtr->event = Inputs::EInputEvent::NONE;
        return;
    }

    // Toggle Perf UI
    if (IsKeyReleased(key, action, GLFW_KEY_J))
    {
        configPtr->app.showPerf = !configPtr->app.showPerf;
        inputPtr->event = Inputs::EInputEvent::NONE;
        return;
    }

    // Toggle Render Mode
    if(IsKeyReleased(key, action, GLFW_KEY_M))
    {
        if (configPtr->app.renderMode == ERenderMode::DDGI)
        {
            configPtr->pathTrace.enabled = true;
            configPtr->ddgi.enabled = false;
            configPtr->rtao.enabled = false;
            configPtr->app.renderMode = ERenderMode::PATH_TRACE;
            inputPtr->event = Inputs::EInputEvent::CAMERA_MOVEMENT;
            return;
        }else if (configPtr->app.renderMode == ERenderMode::PATH_TRACE)
        {
            configPtr->pathTrace.enabled = false;
            configPtr->ddgi.enabled = true;
            configPtr->rtao.enabled = true;
            configPtr->app.renderMode = ERenderMode::DDGI;
        }
        inputPtr->event = Inputs::EInputEvent::NONE;
        return;
    }

}

/**
 * Handle mouse movement inputs.
 */
void MousePositionHandler(GLFWwindow* window, double x, double y)
{
    // ImGui captured the mouse input, don't forward it
    if (Graphics::UI::CapturedMouse()) return;

    inputPtr->mousePos = { (int)x, (int)y };

    if(inputPtr->mouseLeftBtnDown)
    {
        // Compute relative change in mouse position, multiplying by the degrees of change per pixel
        Scenes::Camera& camera = scenePtr->cameras[scenePtr->activeCamera];
        float degreesPerPixelX = (camera.data.fov / (float)configPtr->app.width) * camera.data.aspect;
        float degreesPerPixelY = (camera.data.fov / (float)configPtr->app.height);

        camera.yaw += (float)(inputPtr->mousePos.x - inputPtr->prevMousePos.x) * degreesPerPixelX * configPtr->input.rotationSpeed;
        camera.pitch += (float)(inputPtr->mousePos.y - inputPtr->prevMousePos.y) * degreesPerPixelY * configPtr->input.rotationSpeed;

        if (rtxgi::abs(camera.yaw) >= 360) camera.yaw = 0.f;
        if (rtxgi::abs(camera.pitch) >= 360) camera.pitch = 0.f;

        //char buffer[100];
        //sprintf_s(buffer, "mouse delta: (%i, %i)\n", (mouse.x - input.lastMouseXY.x), (mouse.y - input.lastMouseXY.y));
        //OutputDebugStringA(buffer);

        // Store current mouse position
        inputPtr->prevMousePos = inputPtr->mousePos;

        // Compute and apply the rotation
        Scenes::UpdateCamera(camera);

        return;
    }

    if(inputPtr->mouseRightBtnDown)
    {
        float speed = configPtr->input.movementSpeed / 100.f;
        if (configPtr->input.invertPan) speed *= -1.f;

        float speedX = (float)(inputPtr->mousePos.x - inputPtr->prevMousePos.x) * speed;
        float speedY = (float)(inputPtr->mousePos.y - inputPtr->prevMousePos.y) * -speed;

        // Store current mouse position
        inputPtr->prevMousePos = inputPtr->mousePos;

        // Compute new camera position from pan
        Scenes::Camera& camera = scenePtr->cameras[scenePtr->activeCamera];
        camera.data.position -= (camera.data.right * speedX);
        camera.data.position -= (camera.data.up * speedY);

        return;
    }

}

/**
 * Handle mouse button inputs.
 */
void MouseButtonHandler(GLFWwindow* window, int button, int action, int mods)
{
    // ImGui captured the mouse input, don't forward it
    if (Graphics::UI::CapturedMouse()) return;

    if(button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if(action == GLFW_PRESS)
        {
            inputPtr->mouseLeftBtnDown = true;
            inputPtr->prevMousePos = inputPtr->mousePos;
            return;
        }
        else if(action == GLFW_RELEASE)
        {
            inputPtr->mouseLeftBtnDown = false;
        }
    }

    if(button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if(action == GLFW_PRESS)
        {
            inputPtr->mouseRightBtnDown = true;
            inputPtr->prevMousePos = inputPtr->mousePos;
            return;
        }
        else if(action == GLFW_RELEASE)
        {
            inputPtr->mouseRightBtnDown = false;
        }
    }

    inputPtr->prevMousePos = { INT_MAX, INT_MAX };
}

/**
 * Handle mouse scroll wheel inputs.
 */
void MouseScrollHandler(GLFWwindow* window, double xoffset, double yoffset)
{
    // ImGui captured the mouse input, don't forward it
    if (Graphics::UI::CapturedMouse()) return;

    // Compute new camera position from zoom
    float speed = (float)-yoffset * configPtr->input.movementSpeed / 10.f;

    Scenes::Camera& camera = scenePtr->cameras[scenePtr->activeCamera];
    camera.data.position -= (camera.data.forward * speed);

    inputPtr->event = Inputs::EInputEvent::CAMERA_MOVEMENT;
}

namespace Inputs
{
    //----------------------------------------------------------------------------------------------------------
    // Public Functions
    //----------------------------------------------------------------------------------------------------------

    /**
     *  Bind the input event callbacks and set references to global data.
     */
    bool Initialize(GLFWwindow* window, Input& input, Configs::Config& config, Scenes::Scene& scene)
    {
        glfwSetKeyCallback(window, KeyHandler);
        glfwSetCursorPosCallback(window, MousePositionHandler);
        glfwSetMouseButtonCallback(window, MouseButtonHandler);
        glfwSetScrollCallback(window, MouseScrollHandler);
        inputPtr = &input;
        configPtr = &config;
        scenePtr = &scene;
        return true;
    }

    /**
     * Poll the mouse and keyboard to update the camera.
     */
    void PollInputs(GLFWwindow* window)
    {
        bool movement = false;
        float speed = configPtr->input.movementSpeed / 100.f;

        if (!Graphics::UI::CapturedMouse())
        {
            // Mouse drag
            if (inputPtr->mouseLeftBtnDown || inputPtr->mouseRightBtnDown) movement = true;
        }

        // Keyboard is being captured by ImGui
        if (Graphics::UI::CapturedKeyboard())
        {
            return;
        }

        // If other key events have happened, allow them to complete
        if (inputPtr->event != Inputs::EInputEvent::NONE && 
            inputPtr->event != Inputs::EInputEvent::CAMERA_MOVEMENT)
        {
            return;
        }

        // Get the active camera
        Scenes::Camera& camera = scenePtr->cameras[scenePtr->activeCamera];

        // Modifiers
        int LSHIFT = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT);
        int RSHIFT = glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT);
        int LCNTRL = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL);
        int RCNTRL = glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL);
        int LALT = glfwGetKey(window, GLFW_KEY_LEFT_ALT);
        int RALT = glfwGetKey(window, GLFW_KEY_LEFT_ALT);

        if(LSHIFT == GLFW_PRESS || RSHIFT == GLFW_PRESS)
        {
            speed *= 2.f;
        }

        if(LCNTRL == GLFW_PRESS || RCNTRL == GLFW_PRESS)
        {
            speed *= 0.1f;
        }

        if(LALT == GLFW_PRESS || RALT == GLFW_PRESS)
        {
            speed *= 0.01f;
        }

        // Move left
        if(IsKeyPressed(window, GLFW_KEY_A))
        {
            camera.data.position = camera.data.position - (camera.data.right * speed);
            movement = true;
        }

        // Move right
        if(IsKeyPressed(window, GLFW_KEY_D))
        {
            camera.data.position = camera.data.position + (camera.data.right * speed);
            movement = true;
        }

        // Move backward
        if(IsKeyPressed(window, GLFW_KEY_S))
        {
            camera.data.position = camera.data.position - (camera.data.forward * speed);
            movement = true;
        }

        // Move forward
        if(IsKeyPressed(window, GLFW_KEY_W))
        {
            camera.data.position = camera.data.position + (camera.data.forward * speed);
            movement = true;
        }

        // Move up
        if(IsKeyPressed(window, GLFW_KEY_E))
        {
    #if COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT || COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT
            camera.data.position.y += speed;
    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT_Z_UP || COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT_Z_UP
            camera.data.position.z += speed;
    #endif
            movement = true;
        }

        // Move down
        if(IsKeyPressed(window, GLFW_KEY_Q))
        {
    #if COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT || COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT
            camera.data.position.y -= speed;
    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT_Z_UP || COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT_Z_UP
            camera.data.position.z -= speed;
    #endif
            movement = true;
        }

        if (movement) inputPtr->event = Inputs::EInputEvent::CAMERA_MOVEMENT;
    }

}
