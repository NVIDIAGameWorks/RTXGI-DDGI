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

#include "Config.h"

#include <fstream>
#include <shellapi.h>
#include <sstream>
#include <stdlib.h>

#include "rtxgi/Types.h"

using namespace std;
using namespace rtxgi;

namespace Config
{

void ParseFloat3(string line, size_t startPosition, float3 &result)
{
    size_t prevPosition, position;
    prevPosition = position = startPosition;

    float values[3];
    for (int i = 0; i < 3; i++)
    {
        position = line.find(",", prevPosition);
        values[i] = stof(line.substr(prevPosition, position - prevPosition));
        prevPosition = (position + 1);
    }
    result = { values[0], values[1], values[2] };
}

void ParseFloat3(string line, size_t startPosition, XMFLOAT3 &result)
{
    float3 vector;
    ParseFloat3(line, startPosition, vector);
    result = { vector.x, vector.y, vector.z };
}

void ParseInt3(string line, size_t startPosition, int3 &result)
{
    size_t prevPosition, position;
    prevPosition = position = startPosition;

    int values[3];
    for (int i = 0; i < 3; i++)
    {
        position = line.find(",", prevPosition);
        values[i] = stoi(line.substr(prevPosition, position - prevPosition));
        prevPosition = (position + 1);
    }
    result = { values[0], values[1], values[2] };
}

size_t FindSection(string config, string section)
{
    size_t index = config.find(section);
    if (index == string::npos) return -1;
    return index;
}

void ParseApplication(const char* buffer, ConfigInfo &config)
{
    string line;
    stringstream stream;
    
    string configString(buffer);
    size_t index = configString.find("#[application]");
    if (index == string::npos) return;

    stream.str(configString.substr(index));

    size_t found, length;
    while(!stream.eof())
    {
        getline(stream, line, '\n');
        if (line.find_first_of("#") == 0) continue;     // commented line, skip it

        found = line.find("width=");
        if (found != string::npos)
        {
            length = strlen("width=");
            config.width = stoi(line.substr(strlen("width="), line.size() - length - 1));
            continue;
        }

        found = line.find("height=");
        if (found != string::npos)
        {
            length = strlen("height=");
            config.height = stoi(line.substr(strlen("height="), line.size() - length - 1));
            continue;
        }

        found = line.find("vsync=");
        if (found != string::npos)
        {
            length = strlen("vsync=");
            config.vsync = (bool)stoi(line.substr(strlen("vsync="), line.size() - length - 1));
            continue;
        }

        found = line.find("mode=");
        if (found != string::npos)
        {
            length = strlen("mode=");
            config.mode = (ERenderMode)stoi(line.substr(strlen("mode="), line.size() - length - 1));
            continue;
        }

        found = line.find("ui=");
        if (found != string::npos)
        {
            length = strlen("ui=");
            config.ui = (bool)stoi(line.substr(strlen("ui="), line.size() - length - 1));
            continue;
        }

        found = line.find("root=");
        if (found != string::npos)
        {
            length = strlen("root=");
            config.root = line.substr(strlen("root="), line.size() - length - 1);
        }

        found = line.find("rtxgi-sdk=");
        if (found != string::npos)
        {
            length = strlen("rtxgi-sdk=");
            config.rtxgi = line.substr(strlen("rtxgi-sdk="), line.size() - length - 1);
        }

        found = line.find("scene=");
        if (found != string::npos)
        {
            length = strlen("scene=");
            config.scene = line.substr(strlen("scene="), line.size() - length - 1);
        }
    }
}

void ParseLights(const char* buffer, LightInfo &lights)
{
    string line;
    stringstream stream;

    string configString(buffer);
    size_t index = configString.find("#[lights]");
    if (index == string::npos) return;

    stream.str(configString.substr(index));

    size_t found;
    while (!stream.eof())
    {
        getline(stream, line, '\n');
        if (line.find_first_of("#") == 0) continue;     // commented line, skip it

        // Directional light

        found = line.find("directionalLight=");
        if (found != string::npos)
        {
            ParseFloat3(line, strlen("directionalLight="), lights.directionalLight.direction);
            lights.lightMask |= 0x00000001;
            lights.lightCounts.x = 1;
            continue;
        }

        found = line.find("directionalLightColor=");
        if (found != string::npos)
        {
            ParseFloat3(line, strlen("directionalLightColor="), lights.directionalLight.color);
            continue;
        }

        found = line.find("directionalLightPower=");
        if (found != string::npos)
        {
            lights.directionalLight.power = stof(line.substr((found + strlen("directionalLightPower="))));
            continue;
        }

        // Point light

        found = line.find("pointLightPosition=");
        if (found != string::npos)
        {
            ParseFloat3(line, strlen("pointLightPosition="), lights.pointLight.position);
            lights.lightMask |= 0x00000010;
            lights.lightCounts.y = 1;
            continue;
        }   

        found = line.find("pointLightColor=");
        if (found != string::npos)
        {
            ParseFloat3(line, strlen("pointLightColor="), lights.pointLight.color);
            continue;
        }

        found = line.find("pointLightPower=");
        if (found != string::npos)
        {
            lights.pointLight.power = stof(line.substr((found + strlen("pointLightPower="))));
            continue;
        }

        found = line.find("pointLightMaxDistance=");
        if (found != string::npos)
        {
            lights.pointLight.maxDistance = stof(line.substr((found + strlen("pointLightMaxDistance="))));
            continue;
        }

        // Spot light

        found = line.find("spotLightPosition=");
        if (found != string::npos)
        {
            ParseFloat3(line, strlen("spotLightPosition="), lights.spotLight.position);
            lights.lightMask |= 0x00000100;
            lights.lightCounts.z = 1;
            continue;
        }

        found = line.find("spotLightDirection=");
        if (found != string::npos)
        {
            ParseFloat3(line, strlen("spotLightDirection="), lights.spotLight.direction);
            continue;
        }

        found = line.find("spotLightColor=");
        if (found != string::npos)
        {
            ParseFloat3(line, strlen("spotLightColor="), lights.spotLight.color);
            continue;
        }

        found = line.find("spotLightPower=");
        if (found != string::npos)
        {
            lights.spotLight.power = stof(line.substr((found + strlen("spotLightPower="))));
            continue;
        }

        found = line.find("spotLightMaxDistance=");
        if (found != string::npos)
        {
            lights.spotLight.maxDistance = stof(line.substr((found + strlen("spotLightMaxDistance="))));
            continue;
        }

        found = line.find("spotLightUmbraAngle=");
        if (found != string::npos)
        {
            lights.spotLight.umbraAngle = stof(line.substr((found + strlen("spotLightUmbraAngle="))));
            lights.spotLight.umbraAngle = lights.spotLight.umbraAngle * (XM_PI / 180.f);      // convert to radians

            continue;
        }

        found = line.find("spotLightPenumbraAngle=");
        if (found != string::npos)
        {
            lights.spotLight.penumbraAngle = stof(line.substr((found + strlen("spotLightPenumbraAngle="))));
            lights.spotLight.penumbraAngle = lights.spotLight.penumbraAngle * (XM_PI / 180.f);   // convert to radians
            continue;
        }
    }
}

void ParseCamera(const char* buffer, CameraInfo &camera, InputInfo &input)
{
    string line;
    stringstream stream;

    string configString(buffer);
    size_t index = configString.find("#[camera]");
    if (index == string::npos) return;

    stream.str(configString.substr(index));

    size_t found;
    while (!stream.eof())
    {
        getline(stream, line, '\n');
        if (line.find_first_of("#") == 0) continue;     // commented line, skip it

        found = line.find("cameraPosition=");
        if (found != string::npos)
        {
            ParseFloat3(line, strlen("cameraPosition="), camera.origin);
            continue;
        }

        found = line.find("cameraYaw=");
        if (found != string::npos)
        {
            input.yaw = stof(line.substr((found + strlen("cameraYaw="))));
            input.initialized = true;
            continue;
        }

        found = line.find("cameraPitch=");
        if (found != string::npos)
        {
            input.pitch = stof(line.substr((found + strlen("cameraPitch="))));
            input.initialized = true;
            continue;
        }

        found = line.find("cameraFov=");
        if (found != string::npos)
        {
            camera.fov = stof(line.substr((found + strlen("cameraFov="))));
            camera.tanHalfFovY = tanf(camera.fov * (XM_PI / 180.f) * 0.5f);
            continue;
        }
    }
}

void ParseVolume(const char* buffer, DDGIVolumeDesc &desc)
{
    string line;
    stringstream stream;

    string configString(buffer);
    size_t index = configString.find("#[volume]");
    if (index == string::npos) return;

    stream.str(configString.substr(index));

    while(!stream.eof())
    {
        getline(stream, line, '\n');
        if (line.find_first_of("#") == 0) continue;     // comment line, skip it

        size_t found = line.find("origin=");
        if (found != string::npos)
        {
            ParseFloat3(line, strlen("origin="), desc.origin);
            continue;
        }

        found = line.find("probeGridCounts=");
        if (found != string::npos)
        {
            ParseInt3(line, strlen("probeGridCounts="), desc.probeGridCounts);          
            continue;
        }

        found = line.find("probeGridSpacing=");
        if (found != string::npos)
        {
            ParseFloat3(line, strlen("probeGridSpacing="), desc.probeGridSpacing);
            continue;
        }

        found = line.find("viewBias=");
        if (found != string::npos)
        {
            desc.viewBias = stof(line.substr((found + strlen("viewBias="))));
            continue;
        }

        found = line.find("normalBias=");
        if (found != string::npos)
        {
            desc.normalBias = stof(line.substr((found + strlen("normalBias="))));
            continue;
        }

        found = line.find("hysteresis=");
        if (found != string::npos)
        {
            desc.probeHysteresis = stof(line.substr((found + strlen("hysteresis="))));
            continue;
        }

        found = line.find("changeThreshold=");
        if (found != string::npos)
        {
            desc.probeChangeThreshold = stof(line.substr((found + strlen("changeThreshold="))));
            continue;
        }

        found = line.find("brightnessThreshold=");
        if (found != string::npos)
        {
            desc.probeBrightnessThreshold = stof(line.substr((found + strlen("brightnessThreshold="))));
            continue;
        }

        found = line.find("numRaysPerProbe=");
        if (found != string::npos)
        {
            desc.numRaysPerProbe = stoi(line.substr((found + strlen("numRaysPerProbe="))));
            continue;
        }

        found = line.find("numIrradianceTexels=");
        if (found != string::npos)
        {
            desc.numIrradianceTexels = stoi(line.substr((found + strlen("numIrradianceTexels="))));
            continue;
        }

        found = line.find("numDistanceTexels=");
        if (found != string::npos)
        {
            desc.numDistanceTexels = stoi(line.substr((found + strlen("numDistanceTexels="))));
        }
    }
}

void ParseInput(const char* buffer, InputOptions &options)
{
    string line;
    stringstream stream;

    string configString(buffer);
    size_t index = configString.find("#[input]");
    if (index == string::npos) return;

    stream.str(configString.substr(index));

    size_t found;
    while (!stream.eof())
    {
        getline(stream, line, '\n');
        if (line.find_first_of("#") == 0) continue;     // commented line, skip it

        found = line.find("inputInvertPan=");
        if (found != string::npos)
        {
            options.invertPan = (stoi(line.substr((found + strlen("inputInvertPan=")))) > 0);
            continue;
        }

        found = line.find("inputMovementSpeed=");
        if (found != string::npos)
        {
            options.movementSpeed = stof(line.substr((found + strlen("inputMovementSpeed="))));
            continue;
        }

        found = line.find("inputRotationSpeed=");
        if (found != string::npos)
        {
            options.rotationSpeed = stof(line.substr((found + strlen("inputRotationSpeed="))));
        }
    }
}

void ParseRT(const char* buffer, RTOptions &options)
{
    string line;
    stringstream stream;

    string configString(buffer);
    size_t index = configString.find("#[raytracing]");
    if (index == string::npos) return;

    stream.str(configString.substr(index));

    size_t found;
    while (!stream.eof())
    {
        getline(stream, line, '\n');
        if (line.find_first_of("#") == 0) continue;     // commented line, skip it

        found = line.find("rtViewBias=");
        if (found != string::npos)
        {
            options.viewBias = stof(line.substr((found + strlen("rtViewBias="))));
            continue;
        }

        found = line.find("rtNormalBias=");
        if (found != string::npos)
        {
            options.normalBias = stof(line.substr((found + strlen("rtNormalBias="))));
            continue;
        }

        found = line.find("rtNumBounces=");
        if (found != string::npos)
        {
            options.numBounces = stoi(line.substr((found + strlen("rtNumBounces="))));
            continue;
        }
    }
}

void ParsePostProcess(const char* buffer, PostProcessOptions &options)
{
    string line;
    stringstream stream;

    string configString(buffer);
    size_t index = configString.find("#[postprocess]");
    if (index == string::npos) return;

    stream.str(configString.substr(index));

    size_t found;
    while (!stream.eof())
    {
        getline(stream, line, '\n');
        if (line.find_first_of("#") == 0) continue;     // commented line, skip it

        found = line.find("AORadius=");
        if (found != string::npos)
        {
            options.AORadius = stof(line.substr((found + strlen("AORadius="))));
        }

        found = line.find("AOFilterDepthSigma=");
        if (found != string::npos)
        {
            options.AOFilterDepthSigma = stof(line.substr((found + strlen("AOFilterDepthSigma="))));
        }
    }
}

void ParseViz(const char* buffer, VizOptions &options)
{
    string line;
    stringstream stream;

    string configString(buffer);
    size_t index = configString.find("#[visualization]");
    if (index == string::npos) return;

    stream.str(configString.substr(index));

    size_t found;
    while (!stream.eof())
    {
        getline(stream, line, '\n');
        if (line.find_first_of("#") == 0) continue;     // commented line, skip it

        found = line.find("vizShowDDGIBuffers=");
        if (found != string::npos)
        {
            options.showDDGIVolumeBuffers = (stoi(line.substr((found + strlen("vizShowDDGIBuffers=")))) > 0);
            continue;
        }

        found = line.find("vizShowDDGIProbes=");
        if (found != string::npos)
        {
            options.showDDGIVolumeProbes = (stoi(line.substr((found + strlen("vizShowDDGIProbes=")))) > 0);
            continue;
        }

        found = line.find("vizProbeRadius=");
        if (found != string::npos)
        {
            options.probeRadius = stof(line.substr((found + strlen("vizProbeRadius="))));
            continue;
        }

        found = line.find("vizBufferIrradianceScale=");
        if (found != string::npos)
        {
            options.irradianceScale = stof(line.substr((found + strlen("vizBufferIrradianceScale="))));
            continue;
        }

        found = line.find("vizBufferDistanceScale=");
        if (found != string::npos)
        {
            options.distanceScale = stof(line.substr((found + strlen("vizBufferDistanceScale="))));
            continue;
        }

        found = line.find("vizBufferDistanceDivisor=");
        if (found != string::npos)
        {
            options.distanceDivisor = stof(line.substr((found + strlen("vizBufferDistanceDivisor="))));
        }

        found = line.find("vizBufferRadianceScale=");
        if (found != string::npos)
        {
            options.radianceScale = stof(line.substr((found + strlen("vizBufferRadianceScale="))));
            continue;
        }

        found = line.find("vizBufferOffsetScale=");
        if (found != string::npos)
        {
            options.offsetScale = stof(line.substr((found + strlen("vizBufferOffsetScale="))));
            continue;
        }

        found = line.find("vizBufferStateScale=");
        if (found != string::npos)
        {
            options.stateScale = stof(line.substr((found + strlen("vizBufferStateScale="))));
        }
    }
}

void ParseConfig(
    const char* buffer,
    ConfigInfo &config,
    LightInfo &lights,
    CameraInfo &camera,
    DDGIVolumeDesc &desc,
    InputInfo &inputInfo,
    InputOptions &inputOptions,
    RTOptions &rtOptions,
    PostProcessOptions &postOptions,
    VizOptions &vizOptions)
{
    ParseApplication(buffer, config);
    ParseLights(buffer, lights);
    ParseCamera(buffer, camera, inputInfo);
    ParseVolume(buffer, desc);
    ParseInput(buffer, inputOptions);
    ParseRT(buffer, rtOptions);
    ParsePostProcess(buffer, postOptions);
    ParseViz(buffer, vizOptions);

    camera.aspect = (float)config.width / (float)config.height;
}

/**
 * Load and parse the configuration file at the provided file path.
 */
bool Load(
    ConfigInfo &config,
    LightInfo &lights,
    CameraInfo &camera,
    DDGIVolumeDesc &desc,
    InputInfo &inputInfo,
    InputOptions &inputOptions,
    RTOptions &rtOptions,
    PostProcessOptions &postOptions,
    VizOptions &vizOptions,
    ofstream &log)
{
    ifstream file(config.filepath, ios::ate | ios::binary);
    if (!file.is_open())
    {
        log << "Error: failed to load configuration file: '";
        log << config.filepath << "'\n";
        return false;
    }

    // Read the configuration file to a buffer
    size_t fileSize = (size_t)file.tellg();
    char* buffer = new char[fileSize];

    file.seekg(0);
    file.read(buffer, fileSize);
    file.close();

    // Parse the configuration file
    ParseConfig(buffer, config, lights, camera, desc, inputInfo, inputOptions, rtOptions, postOptions, vizOptions);

    delete[] buffer;

    // Check the probe ray count
    if (desc.numRaysPerProbe % desc.numIrradianceTexels != 0)
    {
        log << "Warning: numRaysPerProbe is not a multiple of numIrradianceTexels. Not an optimal configuration when using shared memory during blending.\n";
    }

    return true;
}

/**
 * Parse the command line to get the configuration file path.
 */
bool ParseCommandLine(LPWSTR lpCmdLine, ConfigInfo &config, ofstream &log)
{
    LPWSTR* argv = NULL;
    int argc = 0;

    argv = CommandLineToArgvW(lpCmdLine, &argc);
    if (argv == NULL)
    {
        log << "Error: failed to parse command line!\n";
        return false;
    }

    if (argc != 1)
    {
        log << "Error: incorrect command line usage!\n";
        return false;
    }

    size_t len;
    size_t max = wcslen(argv[0]) + 1;
    char* dst = new char[max];

    wcstombs_s(&len, dst, max, argv[0], max);
    config.filepath = string(dst);

    delete[] dst;

    LocalFree(argv);
    return true;
}

}
