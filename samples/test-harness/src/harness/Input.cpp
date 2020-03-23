/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

#include "Input.h"

#include <DirectXMath.h>

#include <rtxgi/Defines.h>

using namespace DirectX;

namespace Input
{

//----------------------------------------------------------------------------------------------------------
// Private Functions
//----------------------------------------------------------------------------------------------------------

void Rotate(InputInfo &input, CameraInfo &camera)
{
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT
    XMFLOAT3 up = XMFLOAT3(0.f, 1.f, 0.f);
    XMFLOAT3 forward = XMFLOAT3(0.f, 0.f, 1.f);
    XMMATRIX rotation = XMMatrixRotationRollPitchYaw(input.pitch * (XM_PI / 180.f), input.yaw * (XM_PI / 180.f), 0.f);
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    XMFLOAT3 up = XMFLOAT3(0.f, 1.f, 0.f);
    XMFLOAT3 forward = XMFLOAT3(0.f, 0.f, -1.f);
    XMMATRIX rotation = XMMatrixRotationRollPitchYaw(-input.pitch * (XM_PI / 180.f), -input.yaw * (XM_PI / 180.f), 0.f);
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_UNREAL
    XMFLOAT3 up = XMFLOAT3(0.f, 0.f, 1.f);
    XMFLOAT3 forward = XMFLOAT3(1.f, 0.f, 0.f);
    XMMATRIX rotationY = XMMatrixRotationY(input.pitch * (XM_PI / 180.F));
    XMMATRIX rotationZ = XMMatrixRotationZ(input.yaw * (XM_PI / 180.F));
    XMMATRIX rotation = XMMatrixMultiply(rotationY, rotationZ);
#endif

    XMStoreFloat3(&camera.forward, XMVector3Normalize(XMVector3Transform(XMLoadFloat3(&forward), rotation)));

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
    XMStoreFloat3(&camera.right, XMVector3Normalize(XMVector3Cross(XMLoadFloat3(&camera.forward), XMLoadFloat3(&up))));
    XMStoreFloat3(&camera.up, XMVector3Cross(-XMLoadFloat3(&camera.forward), XMLoadFloat3(&camera.right)));
#else
    XMStoreFloat3(&camera.right, XMVector3Normalize(-XMVector3Cross(XMLoadFloat3(&camera.forward), XMLoadFloat3(&up))));
    XMStoreFloat3(&camera.up, XMVector3Cross(XMLoadFloat3(&camera.forward), XMLoadFloat3(&camera.right)));
#endif
}

//----------------------------------------------------------------------------------------------------------
// Public Functions
//----------------------------------------------------------------------------------------------------------

/**
* Handle keyboard inputs.
*/
bool KeyHandler(
    InputInfo &input,
    ConfigInfo &config,
    InputOptions &inputOptions,
    VizOptions &vizOptions,
    CameraInfo &camera,
    float3 &translation,
    bool &useDDGI,
    bool &hotReload)
{
    Keyboard::State kb = input.keyboard.GetState();
    hotReload = false;

    if (input.kbTracker.IsKeyReleased(Keyboard::Escape))
    {
        PostQuitMessage(0);
        return false;
    }
    
    if (input.kbTracker.IsKeyReleased(Keyboard::I))
    {
        inputOptions.invertPan = !inputOptions.invertPan;
        input.kbTracker.Update(kb);
        return false;
    }

    if (input.kbTracker.IsKeyReleased(Keyboard::B))
    {
        vizOptions.showDDGIVolumeBuffers = !vizOptions.showDDGIVolumeBuffers;
        input.kbTracker.Update(kb);
        return false;
    }

    if (input.kbTracker.IsKeyReleased(Keyboard::P))
    {
        vizOptions.showDDGIVolumeProbes = !vizOptions.showDDGIVolumeProbes;
        input.kbTracker.Update(kb);
        return false;
    }

    if (input.kbTracker.IsKeyReleased(Keyboard::T))
    {
        useDDGI = !useDDGI;
        input.kbTracker.Update(kb);
        return false;
    }

    if (input.kbTracker.IsKeyReleased(Keyboard::R))
    {
        hotReload = true;
        input.kbTracker.Update(kb);
        return false;
    }

    if (input.kbTracker.IsKeyReleased(Keyboard::U))
    {
        config.ui = !config.ui;
        input.kbTracker.Update(kb);
        return false;
    }

    if (input.kbTracker.IsKeyReleased(Keyboard::M))
    {
        if (config.mode == ERenderMode::PathTrace) config.mode = ERenderMode::DDGI;
        else if (config.mode == ERenderMode::DDGI) config.mode = ERenderMode::PathTrace;
        input.kbTracker.Update(kb);
        return true;
    }

#if RTXGI_DDGI_PROBE_RELOCATION
    if (input.kbTracker.IsKeyReleased(Keyboard::G))
    {
        // Activate probe relocation
        inputOptions.runProbeRelocation = true;
        input.kbTracker.Update(kb);
        return false;
    }
#endif

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
    if (input.kbTracker.IsKeyReleased(Keyboard::K))
    {
        // Activate all probes in the volume
        inputOptions.activateAllProbes = true;
        input.kbTracker.Update(kb);
        return false;
    }

    if (input.kbTracker.IsKeyReleased(Keyboard::L))
    {
        // Toggle probe classification
        inputOptions.enableProbeClassification = !inputOptions.enableProbeClassification;
        input.kbTracker.Update(kb);
        return false;
    }
#endif

    float speed = inputOptions.movementSpeed / 100.f;
    bool result = false;

    if (kb.IsKeyDown(Keyboard::LeftShift) || kb.IsKeyDown(Keyboard::RightShift))
    {
        speed *= 2.f;
    }

    if (kb.IsKeyDown(Keyboard::LeftControl) || kb.IsKeyDown(Keyboard::RightControl))
    {
        speed *= 0.1f;
    }

    if (kb.IsKeyDown(Keyboard::LeftAlt) || kb.IsKeyDown(Keyboard::RightAlt))
    {
        speed *= 0.01f;
    }

    if (kb.IsKeyDown(Keyboard::A))
    {
        camera.origin = { camera.origin.x - (camera.right.x * speed), camera.origin.y - (camera.right.y * speed), camera.origin.z - (camera.right.z * speed) };
        result = true;
    }

    if (kb.IsKeyDown(Keyboard::D))
    {
        camera.origin = { camera.origin.x + (camera.right.x * speed), camera.origin.y + (camera.right.y * speed), camera.origin.z + (camera.right.z * speed) };
        result = true;
    }

    if (kb.IsKeyDown(Keyboard::S))
    {
        camera.origin = { camera.origin.x - (camera.forward.x * speed), camera.origin.y - (camera.forward.y * speed), camera.origin.z - (camera.forward.z * speed) };
        result = true;
    }

    if (kb.IsKeyDown(Keyboard::W))
    {
        camera.origin = { camera.origin.x + (camera.forward.x * speed), camera.origin.y + (camera.forward.y * speed), camera.origin.z + (camera.forward.z * speed) };
        result = true;
    }

    if (kb.IsKeyDown(Keyboard::E))
    {
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
        camera.origin.y += speed;
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_UNREAL
        camera.origin.z += speed;
#endif
        result = true;
    }

    if (kb.IsKeyDown(Keyboard::Q))
    {
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
        camera.origin.y -= speed;
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_UNREAL
        camera.origin.z -= speed;
#endif
        result = true;
    }

    if (kb.IsKeyDown(Keyboard::Left))
    {
        translation.x -= (camera.right.x * speed);
        translation.y -= (camera.right.y * speed);
        translation.z -= (camera.right.z * speed);
        result = true;
    }

    if (kb.IsKeyDown(Keyboard::Right))
    {
        translation.x += (camera.right.x * speed);
        translation.y += (camera.right.y * speed);
        translation.z += (camera.right.z * speed);
        result = true;
    }

    if (kb.IsKeyDown(Keyboard::Down))
    {
        translation.x -= (camera.forward.x * speed);
        translation.y -= (camera.forward.y * speed);
        translation.z -= (camera.forward.z * speed);
        result = true;
    }

    if (kb.IsKeyDown(Keyboard::Up))
    {
        translation.x += (camera.forward.x * speed);
        translation.y += (camera.forward.y * speed);
        translation.z += (camera.forward.z * speed);
        result = true;
    }

    if (kb.IsKeyDown(Keyboard::PageDown))
    {
        translation.x -= (camera.up.x * speed);
        translation.y -= (camera.up.y * speed);
        translation.z -= (camera.up.z * speed);
        result = true;
    }

    if (kb.IsKeyDown(Keyboard::PageUp))
    {
        translation.x += (camera.up.x * speed);
        translation.y += (camera.up.y * speed);
        translation.z += (camera.up.z * speed);
        result = true;
    }

    input.kbTracker.Update(kb);
    return result;
}

/**
* Handle mouse inputs.
*/
bool MouseHandler(InputInfo &input, CameraInfo &camera, InputOptions &inputOptions)
{
    Mouse::State mouse = input.mouse.GetState();
    if (mouse.leftButton)
    {
        // Just pressed the left mouse button
        if (input.lastMouseXY.x == INT_MAX && input.lastMouseXY.y == INT_MAX)
        {
            input.lastMouseXY = { mouse.x, mouse.y };
            return false;
        }

        // Compute relative change in mouse position, and multiply by the degrees of change per pixel
        float degreesPerPixelX = (camera.fov / (float)input.width) * camera.aspect;
        float degreesPerPixelY = (camera.fov / (float)input.height);

        input.yaw += (float)(mouse.x - input.lastMouseXY.x) * degreesPerPixelX * inputOptions.rotationSpeed;
        input.pitch += (float)(mouse.y - input.lastMouseXY.y) * degreesPerPixelY * inputOptions.rotationSpeed;

        if (abs(input.yaw) >= 360) input.yaw = 0.f;
        if (abs(input.pitch) >= 360) input.pitch = 0.f;

        /*char buffer[100];
        sprintf_s(buffer, "mouse delta: (%i, %i)\n", (mouse.x - input.lastMouseXY.x), (mouse.y - input.lastMouseXY.y));
        OutputDebugStringA(buffer);*/

        // Store current mouse position
        input.lastMouseXY = { mouse.x, mouse.y };

        // Compute and apply the rotation
        Rotate(input, camera);

        return true;
    }

    if (mouse.rightButton)
    {
        // Just pressed the right mouse button
        if (input.lastMouseXY.x == INT_MAX && input.lastMouseXY.y == INT_MAX)
        {
            input.lastMouseXY = { mouse.x, mouse.y };
            return false;
        }

        float speed = inputOptions.movementSpeed / 100.f;
        if (inputOptions.invertPan) speed *= -1.f;

        float speedX = (float)(mouse.x - input.lastMouseXY.x) * speed;
        float speedY = (float)(mouse.y - input.lastMouseXY.y) * -speed;

        // Store current mouse position
        input.lastMouseXY = { mouse.x, mouse.y };

        // Compute new camera origin
        camera.origin = { camera.origin.x - (camera.right.x * speedX), camera.origin.y - (camera.right.y * speedX), camera.origin.z - (camera.right.z * speedX) };
        camera.origin = { camera.origin.x - (camera.up.x * speedY), camera.origin.y - (camera.up.y * speedY), camera.origin.z - (camera.up.z * speedY) };

        return true;
    }

    if (mouse.scrollWheelValue != input.scrollWheelValue)
    {
        if (input.scrollWheelValue == INT_MAX)
        {
            input.scrollWheelValue = mouse.scrollWheelValue;
            return false;
        }

        float speed = (input.scrollWheelValue - mouse.scrollWheelValue) * inputOptions.movementSpeed / 100.f;
        camera.origin = { camera.origin.x - (camera.forward.x * speed), camera.origin.y - (camera.forward.y * speed), camera.origin.z - (camera.forward.z * speed) };

        input.scrollWheelValue = mouse.scrollWheelValue;
        return true;
    }

    if (input.initialized)
    {
        if (abs(input.yaw) >= 360) input.yaw = 0.f;
        if (abs(input.pitch) >= 360) input.pitch = 0.f;

        Rotate(input, camera);

        input.initialized = false;
    }

    input.lastMouseXY = { INT_MAX, INT_MAX };
    return false;
}

}
