/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

#include "Common.h"

#include <rtxgi/ddgi/DDGIVolume.h>
#include <fstream>

using namespace DirectX;

namespace Configs
{

    // --- DDGI Configuration -------------------------

    struct DDGIVolumeTextures
    {
        rtxgi::EDDGIVolumeTextureFormat rayDataFormat;
        rtxgi::EDDGIVolumeTextureFormat irradianceFormat;
        rtxgi::EDDGIVolumeTextureFormat distanceFormat;
        rtxgi::EDDGIVolumeTextureFormat dataFormat;
        rtxgi::EDDGIVolumeTextureFormat variabilityFormat;
    };

    struct DDGIVolume
    {
        std::string        name = "";
        uint32_t           index = 0;
        uint32_t           rngSeed = 0;

        bool               insertPerfMarkers = false;
        bool               showProbes = false;
        bool               clearProbes = false;
        bool               probeRelocationEnabled = false;
        bool               probeClassificationEnabled = false;
        bool               probeVariabilityEnabled = false;
        bool               infiniteScrollingEnabled = false;
        bool               clearProbeVariability = false;

        DirectX::XMFLOAT3  origin = { 0.f, 0.f, 0.f };
        DirectX::XMFLOAT3  eulerAngles = { 0.f, 0.f, 0.f };
        DirectX::XMFLOAT3  probeSpacing = { 0.f, 0.f, 0.f };

        DirectX::XMINT3    probeCounts = { 0, 0, 0 };

        uint32_t           probeNumRays = 0;
        uint32_t           probeNumIrradianceTexels = 0;
        uint32_t           probeNumDistanceTexels = 0;

        float              probeHysteresis = 0.f;
        float              probeMaxRayDistance = 0.f;
        float              probeNormalBias = 0.f;
        float              probeViewBias = 0.f;
        float              probeIrradianceThreshold = 0.f;
        float              probeBrightnessThreshold = 0.f;
        float              probeVariabilityThreshold = 0.f;

        float              probeMinFrontfaceDistance = 0.f;

        DDGIVolumeTextures textureFormats;

        // Visualization
        uint32_t           probeType = 0;
        float              probeRadius = 1.f;
        float              probeDistanceDivisor = 1.f;
        float              probeRayDataScale = 1.f;
        float              probeIrradianceScale = 1.f;
        float              probeDistanceScale = 1.f;
        float              probeDataScale = 1.f;
        float              probeVariabilityScale = 1.f;

        rtxgi::EDDGIVolumeProbeVisType probeVisType = rtxgi::EDDGIVolumeProbeVisType::Default;
    };

    struct DDGI
    {
        bool enabled = true;
        bool reload = false;
        bool showProbes = false;
        bool showTextures = false;
        bool showIndirect = false;
        bool insertPerfMarkers = true;
        bool shaderExecutionReordering = false;
        uint32_t selectedVolume = 0;
        std::vector<DDGIVolume> volumes;
    };

    // ------------------------------------------------

    struct PostProcessTonemapping
    {
        bool enabled = false;
    };

    struct PostProcessDithering
    {
        bool enabled = false;
    };

    struct PostProcessExposure
    {
        bool  enabled = false;
        float fstops = 1.f;
    };

    struct PostProcessGamma
    {
        bool enabled = false;
    };

    struct PostProcess
    {
        bool enabled = true;
        bool reload = false;
        PostProcessTonemapping tonemap;
        PostProcessDithering dither;
        PostProcessExposure exposure;
        PostProcessGamma gamma;
    };

    struct RTAO
    {
        bool  enabled = true;
        bool  visualize = false;
        bool  reload = false;
        float rayLength = 1.f;
        float rayNormalBias = 0.0001f;
        float rayViewBias = 0.0001f;
        float powerLog = -1.f;
        float filterDistanceSigma = 10.f;
        float filterDepthSigma = 0.25f;
    };

    struct PathTrace
    {
        bool  enabled = false;
        bool  antialiasing = false;
        bool  shaderExecutionReordering = false;
        bool  reload = false;
        float rayNormalBias = 0.001f;
        float rayViewBias = 0.001f;
        uint32_t numBounces = 1;
        uint32_t samplesPerPixel = 1;
    };

    struct Light
    {
        std::string name = "";
        ELightType type  = ELightType::DIRECTIONAL;

        DirectX::XMFLOAT3 position = { 0.f, 0.f, 0.f };
        DirectX::XMFLOAT3 direction = { 0.f, 0.f, 0.f };
        DirectX::XMFLOAT3 color = { 1.f, 1.f, 1.f };

        float power = 1.f;
        float radius = 0.f;
        float umbraAngle = 0.f;
        float penumbraAngle = 0.f;
    };

    struct Camera
    {
        std::string name = "";
        DirectX::XMFLOAT3 position = { 0.f, 0.f, 0.f };
        float fov = 45.f;
        float yaw = 0.f;
        float pitch = 0.f;
        float aspect = 0.f;
    };

    struct Scene
    {
        std::string name = "";
        std::string path = "";
        std::string file = "";
        std::string screenshotPath = "";
        DirectX::XMFLOAT3 skyColor = { 0.f, 0.f, 0.f };
        float skyIntensity = 1.f;

        std::vector<Camera> cameras;
        std::vector<Light> lights;
    };

    struct Input
    {
        bool  invertPan = true;
        float movementSpeed = 1.f;
        float rotationSpeed = 1.f;
    };

    struct Application
    {
        int         width = 1280;
        int         height = 720;
        bool        vsync = true;
        bool        fullscreen = false;
        bool        showUI = true;
        bool        showPerf = false;
        bool        benchmarkRunning = false;

        uint32_t    benchmarkProgress = 0;

        std::string filepath = "";
        std::string root = "";
        std::string rtxgi = "";
        std::string title = "";
        std::string api = "";
        std::string gpuName = "";

        ERenderMode renderMode = ERenderMode::DDGI;
    };

    struct Config
    {
        Application   app;
        Input         input;
        Scene         scene;
        PathTrace     pathTrace;
        DDGI          ddgi;
        RTAO          rtao;
        PostProcess   postProcess;
    };

    bool ParseCommandLine(const std::vector<std::string>& arguments, Config& config, std::ofstream& log);
    bool Load(Config& config, std::ofstream& log);
}
