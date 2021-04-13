/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

#include "Config.h"

#include <shellapi.h>
#include <sstream>
#include <stdlib.h>

#include "rtxgi/Types.h"

using namespace DirectX;
using namespace rtxgi;

namespace Config
{

//----------------------------------------------------------------------------------------------------------
// Parsing Helpers
//----------------------------------------------------------------------------------------------------------

struct ParseLineInfo
{
    bool status = true;
    const std::string* str;
    std::ofstream* log;
};

std::string ParseValue(ParseLineInfo &data)
{
    size_t pos = data.str->find('=');
    if (pos == std::string::npos)
    {
        *data.log << "Failed to parse config file at the line with: " << data.str->c_str() << std::endl;
        data.status = false;
    }

    size_t endPos = data.str->find_first_of("#\t%\x0D\x0A", pos); // any of the possible delimiters afer the value
    if (endPos - pos == 0)
    {
        *data.log << "Failed to parse config file at the line with: " << data.str->c_str() << std::endl;
        data.status = false;
    }

    return data.str->substr(pos + 1, endPos - pos - 1);
};

void Parse(ParseLineInfo &data, std::string &result)
{
    result = ParseValue(data);
}

void Parse(ParseLineInfo &data, float &result)
{
    result = stof(ParseValue(data));
}

void Parse(ParseLineInfo &data, int &result)
{
    result = stoi(ParseValue(data));
}

void Parse(ParseLineInfo &data, unsigned int &result)
{
    result = (unsigned int)stoi(ParseValue(data));
}

void Parse(ParseLineInfo &data, bool &result)
{
    result = (bool)stoi(ParseValue(data));
}

void Parse(ParseLineInfo &data, ERenderMode &result)
{
    result = (ERenderMode)stoi(ParseValue(data));
}

void Parse(ParseLineInfo &data, float3 &result)
{
    std::string line = ParseValue(data);
    if (!data.status) return;

    size_t prevPosition, position;
    prevPosition = position = 0;

    float values[3];
    for (int i = 0; i < 3; i++)
    {
        position = line.find(",", prevPosition);
        values[i] = stof(line.substr(prevPosition, position - prevPosition));
        prevPosition = (position + 1);
    }

    result = { values[0], values[1], values[2] };
}

void Parse(ParseLineInfo &data, XMFLOAT3 &result)
{
    float3 vector;
    Parse(data, vector);
    if (!data.status) return;

    result = { vector.x, vector.y, vector.z };
}

void Parse(ParseLineInfo &data, int3 &result)
{
    std::string line = ParseValue(data);
    if (!data.status) return;

    size_t prevPosition, position;
    prevPosition = position = 0;

    int values[3];
    for (int i = 0; i < 3; i++)
    {
        position = line.find(",", prevPosition);
        values[i] = stoi(line.substr(prevPosition, position - prevPosition));
        prevPosition = (position + 1);
    }

    result = { values[0], values[1], values[2] };
}

template<typename T>
bool ParseIfExists(const char* parameter, T& dst, const std::string& line, bool &status, std::ofstream &log)
{
    size_t found = line.find(parameter);
    if (found != std::string::npos)
    {
        ParseLineInfo data = {};
        data.str = &line;
        data.log = &log;

        Parse(data, dst);
        status = data.status;
        return true;
    }
    return false;
};

//----------------------------------------------------------------------------------------------------------
// Config File Category Parsers
//----------------------------------------------------------------------------------------------------------

bool ParseApplication(std::string &configString, ConfigInfo &config, std::ofstream& log)
{
    std::string line;
    std::stringstream stream;
    
    size_t index = configString.find("#[application]");
    if (index == std::string::npos) return true;

    stream.str(configString.substr(index));

    bool status = true;
    while(!stream.eof())
    {
        getline(stream, line, '\n');
        if (line.find_first_of("#") == 0) continue;     // commented line, skip it

        if (ParseIfExists("width=", config.width, line, status, log)) continue;
        if (ParseIfExists("height=", config.height, line, status, log)) continue;
        if (ParseIfExists("vsync=", config.vsync, line, status, log)) continue;
        if (ParseIfExists("mode=", config.mode, line, status, log)) continue;
        if (ParseIfExists("ui=", config.ui, line, status, log)) continue;
        if (ParseIfExists("root=", config.root, line, status, log)) continue;
        if (ParseIfExists("rtxgi-sdk=", config.rtxgi, line, status, log)) continue;
        if (ParseIfExists("scene-path=", config.scenePath, line, status, log)) continue;
        if (ParseIfExists("scene-file=", config.sceneFile, line, status, log)) continue;
        if (ParseIfExists("screenshot-file=", config.screenshotFile, line, status, log)) continue;
        
        if (!status) return false;
    }
    return true;
}

bool ParseLights(std::string &configString, LightInfo &lights, std::ofstream &log)
{
    std::string line;
    std::stringstream stream;

    size_t index = configString.find("#[lights]");
    if (index == std::string::npos) return true;

    stream.str(configString.substr(index));

    bool status = true;
    while (!stream.eof())
    {
        getline(stream, line, '\n');
        if (line.find_first_of("#") == 0) continue;     // commented line, skip it

        // Directional light
        if (ParseIfExists("directionalLight=", lights.directionalLight.direction, line, status, log)) 
        {
            lights.lightMask |= 0x00000001;
            lights.lightCounts.x = 1;
            continue;
        }
        if (ParseIfExists("directionalLightColor=", lights.directionalLight.color, line, status, log)) continue;
        if (ParseIfExists("directionalLightPower=", lights.directionalLight.power, line, status, log)) continue;

        // Point Light
        if (ParseIfExists("pointLightPosition=", lights.pointLight.position, line, status, log))
        {
            lights.lightMask |= 0x00000010;
            lights.lightCounts.y = 1;
            continue;
        }
        if (ParseIfExists("pointLightColor=", lights.pointLight.color, line, status, log)) continue;
        if (ParseIfExists("pointLightPower=", lights.pointLight.power, line, status, log)) continue;
        if (ParseIfExists("pointLightMaxDistance=", lights.pointLight.maxDistance, line, status, log)) continue;

        // Spot Light
        if (ParseIfExists("spotLightPosition=", lights.spotLight.position, line, status, log))
        {
            lights.lightMask |= 0x00000100;
            lights.lightCounts.z = 1;
            continue;
        }
        if (ParseIfExists("spotLightDirection=", lights.spotLight.direction, line, status, log)) continue;
        if (ParseIfExists("spotLightColor=", lights.spotLight.color, line, status, log)) continue;
        if (ParseIfExists("spotLightPower=", lights.spotLight.power, line, status, log)) continue;
        if (ParseIfExists("spotLightMaxDistance=", lights.spotLight.maxDistance, line, status, log)) continue;
        if (ParseIfExists("spotLightUmbraAngle=", lights.spotLight.umbraAngle, line, status, log))
        {
            lights.spotLight.umbraAngle = lights.spotLight.umbraAngle * (XM_PI / 180.f);         // convert to radians
            continue;
        }
        if (ParseIfExists("spotLightPenumbraAngle=", lights.spotLight.penumbraAngle, line, status, log))
        {
            lights.spotLight.penumbraAngle = lights.spotLight.penumbraAngle * (XM_PI / 180.f);   // convert to radians
            continue;
        }

        if (!status) return false;
    }
    return true;
}

bool ParseCamera(std::string &configString, Camera &camera, InputInfo &input, std::ofstream &log)
{
    std::string line;
    std::stringstream stream;

    size_t index = configString.find("#[camera]");
    if (index == std::string::npos) return true;

    stream.str(configString.substr(index));

    bool status = true;
    while (!stream.eof())
    {
        getline(stream, line, '\n');
        if (line.find_first_of("#") == 0) continue;     // commented line, skip it

        if (ParseIfExists("cameraPosition=", camera.position, line, status, log)) continue;
        if (ParseIfExists("cameraYaw=", input.yaw, line, status, log))
        {
            input.initialized = true;
            continue;
        }
        if (ParseIfExists("cameraPitch=", input.pitch, line, status, log))
        {
            input.initialized = true;
            continue;
        }
        if (ParseIfExists("cameraFov=", camera.fov, line, status, log))
        {
            camera.tanHalfFovY = tanf(camera.fov * (XM_PI / 180.f) * 0.5f);
            continue;
        }

        if (!status) return false;
    }
    return true;
}

bool ParseVolumes(std::string &configString, std::vector<DDGIVolumeDesc> &descs, std::ofstream &log)
{
    std::string line;
    std::stringstream stream;

    bool foundVolume = false;
    bool searching = true;
    size_t searchPos = 0;
    while (searching)
    {
        size_t index = configString.find("#[volume]", searchPos);
        if (index == std::string::npos) break;

        stream.clear();
        stream.str(configString.substr(index));
        // advance one line so that we don't immediately parse the #[volume] again
        stream.ignore(UINT_MAX, '\n');

        DDGIVolumeDesc desc = {};
        bool status = false;
        while (!stream.eof())
        {
            getline(stream, line, '\n');
            size_t index = line.find("#[volume]", 0);
            if (index != std::string::npos) break; // break out if we see the start of another volume
            if (line.find_first_of("#") == 0) continue;     // comment line, skip it

            if (ParseIfExists("origin=", desc.origin, line, status, log)) continue;
            if (ParseIfExists("eulerAngles=", desc.eulerAngles, line, status, log))
            {
                desc.eulerAngles = desc.eulerAngles * RTXGI_PI / 180.0f;
                continue;
            }
            if (ParseIfExists("probeGridCounts=", desc.probeGridCounts, line, status, log)) continue;
            if (ParseIfExists("probeGridSpacing=", desc.probeGridSpacing, line, status, log)) continue;
            if (ParseIfExists("viewBias=", desc.viewBias, line, status, log)) continue;
            if (ParseIfExists("normalBias=", desc.normalBias, line, status, log)) continue;
            if (ParseIfExists("maxRayDistance=", desc.probeMaxRayDistance, line, status, log)) continue;
            if (ParseIfExists("hysteresis=", desc.probeHysteresis, line, status, log)) continue;
            if (ParseIfExists("changeThreshold=", desc.probeChangeThreshold, line, status, log)) continue;
            if (ParseIfExists("brightnessThreshold=", desc.probeBrightnessThreshold, line, status, log)) continue;
            if (ParseIfExists("numRaysPerProbe=", desc.numRaysPerProbe, line, status, log)) continue;
            if (ParseIfExists("numIrradianceTexels=", desc.numIrradianceTexels, line, status, log)) continue;
            if (ParseIfExists("numDistanceTexels=", desc.numDistanceTexels, line, status, log)) continue;
            if (ParseIfExists("name=", desc.name, line, status, log)) continue;

            if (!status) break;
        }
        if (status)
        {
            foundVolume = true;
            descs.push_back(std::move(desc));
        }
        searchPos = index + 1; // start next iteration searching past the current index so that we don't read the same volume twice
    }
    return foundVolume;
}

bool ParseInput(std::string &configString, InputOptions &options, std::ofstream &log)
{
    std::string line;
    std::stringstream stream;

    size_t index = configString.find("#[input]");
    if (index == std::string::npos) return true;

    stream.str(configString.substr(index));

    bool status = true;
    while (!stream.eof())
    {
        getline(stream, line, '\n');
        if (line.find_first_of("#") == 0) continue;     // commented line, skip it

        if (ParseIfExists("inputInvertPan=", options.invertPan, line, status, log)) continue;
        if (ParseIfExists("inputMovementSpeed=", options.movementSpeed, line, status, log)) continue;
        if (ParseIfExists("inputRotationSpeed=", options.rotationSpeed, line, status, log)) continue;

        if (!status) return false;
    }
    return true;
}

bool ParseRayTracing(std::string &configString, RTOptions &options, std::ofstream &log)
{
    std::string line;
    std::stringstream stream;

    size_t index = configString.find("#[raytracing]");
    if (index == std::string::npos) return true;

    stream.str(configString.substr(index));

    bool status = true;
    while (!stream.eof())
    {
        getline(stream, line, '\n');
        if (line.find_first_of("#") == 0) continue;     // commented line, skip it

        if (ParseIfExists("rtViewBias=", options.viewBias, line, status, log)) continue;
        if (ParseIfExists("rtNormalBias=", options.normalBias, line, status, log)) continue;
        if (ParseIfExists("rtNumBounces=", options.numBounces, line, status, log)) continue;
        if (ParseIfExists("rtSkyIntensity=", options.skyIntensity, line, status, log)) continue;

        if (!status) return false;
    }
    return true;
}

bool ParsePostProcess(std::string &configString, PostProcessOptions &options, std::ofstream &log)
{
    std::string line;
    std::stringstream stream;

    size_t index = configString.find("#[postprocess]");
    if (index == std::string::npos) return true;

    stream.str(configString.substr(index));

    bool status = true;
    while (!stream.eof())
    {
        getline(stream, line, '\n');
        if (line.find_first_of("#") == 0) continue;     // commented line, skip it

        if (ParseIfExists("AORadius=", options.AORadius, line, status, log)) continue;
        if (ParseIfExists("AOBias=", options.AOBias, line, status, log)) continue;
        if (ParseIfExists("AOFilterDepthSigma=", options.AOFilterDepthSigma, line, status, log)) continue;
        if (ParseIfExists("AOPower=", options.AOPowerLog, line, status, log)) continue;

        if (!status) return false;
    }
    return true;
}

bool ParseViz(std::string &configString, VizOptions &options, std::ofstream &log)
{
    std::string line;
    std::stringstream stream;

    size_t index = configString.find("#[visualization]");
    if (index == std::string::npos) return true;

    stream.str(configString.substr(index));

    bool status = true;
    while (!stream.eof())
    {
        getline(stream, line, '\n');
        if (line.find_first_of("#") == 0) continue;     // commented line, skip it

        if (ParseIfExists("vizShowDDGIBuffers=", options.showDDGIVolumeBuffers, line, status, log)) continue;
        if (ParseIfExists("vizShowDDGIProbes=", options.showDDGIVolumeProbes, line, status, log)) continue;
        if (ParseIfExists("vizProbeRadius=", options.probeRadius, line, status, log)) continue;
        if (ParseIfExists("vizBufferIrradianceScale=", options.irradianceScale, line, status, log)) continue;
        if (ParseIfExists("vizBufferDistanceScale=", options.distanceScale, line, status, log)) continue;
        if (ParseIfExists("vizBufferDistanceDivisor=", options.distanceDivisor, line, status, log)) continue;
        if (ParseIfExists("vizBufferRadianceScale=", options.radianceScale, line, status, log)) continue;
        if (ParseIfExists("vizBufferOffsetScale=", options.offsetScale, line, status, log)) continue;
        if (ParseIfExists("vizBufferStateScale=", options.stateScale, line, status, log)) continue;

        if (!status) return false;
    }
    return true;
}

/**
* Parse the configuration file.
*/
bool ParseConfig(
    const char* buffer,
    ConfigInfo &config,
    LightInfo &lights,
    Camera &camera,
    std::vector<DDGIVolumeDesc> &descs,
    InputInfo &inputInfo,
    InputOptions &inputOptions,
    RTOptions &rtOptions,
    PostProcessOptions &postOptions,
    VizOptions &vizOptions,
    std::ofstream &log)
{
    std::string configString(buffer);

    if (!ParseApplication(configString, config, log)) return false;
    if (!ParseLights(configString, lights, log)) return false;
    if (!ParseCamera(configString, camera, inputInfo, log)) return false;
    if (!ParseVolumes(configString, descs, log)) return false;
    if (!ParseInput(configString, inputOptions, log)) return false;
    if (!ParseRayTracing(configString, rtOptions, log)) return false;
    if (!ParsePostProcess(configString, postOptions, log)) return false;
    if (!ParseViz(configString, vizOptions, log)) return false;

    camera.aspect = (float)config.width / (float)config.height;

    return true;
}

/**
* Load and parse the configuration file.
*/
bool Load(
    ConfigInfo &config,
    LightInfo &lights,
    Camera &camera,
    std::vector<DDGIVolumeDesc> &descs,
    InputInfo &inputInfo,
    InputOptions &inputOptions,
    RTOptions &rtOptions,
    PostProcessOptions &postOptions,
    VizOptions &vizOptions,
    std::ofstream &log)
{
    // Load the config file
    std::ifstream file(config.filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        log << "Error: failed to load configuration file: '";
        log << config.filepath << "'\n";
        return false;
    }

    // Read the config file to a buffer
    size_t fileSize = (size_t)file.tellg();
    char* buffer = new char[fileSize];

    file.seekg(0);
    file.read(buffer, fileSize);
    file.close();

    // Parse the config file
    bool result = ParseConfig(buffer, config, lights, camera, descs, inputInfo, inputOptions, rtOptions, postOptions, vizOptions, log);

    // Delete the config file buffer
    delete[] buffer;

    // Check the probe ray counts for each volume descriptor
    for (size_t i = 0; i < descs.size(); i++)
    {
        if (descs[i].numRaysPerProbe % descs[i].numIrradianceTexels != 0)
        {
            log << "Warning: numRaysPerProbe is not a multiple of numIrradianceTexels. This is not an optimal configuration when using shared memory during blending.\n";
        }
    }

    return result;
}

/**
* Parse the command line to get the configuration file path.
*/
bool ParseCommandLine(LPWSTR lpCmdLine, ConfigInfo &config, std::ofstream &log)
{
    if (__argc <= 1)
    {
        // There is one argument (or less) - just the executable path
        // Exit since a configuration file is not specified
        log << "Error: a configuration file must be specified!\n";
        return false;
    }

    if (__argc > 2)
    {
        // There are more than two arguments (executable + configuration file)
        // Exit since there must be a single argument after the executable path
        log << "Error: incorrect command line usage! A single argument (the configuration file) must be specified.\n";
        return false;
    }

    size_t len;
    size_t max = wcslen(__wargv[1]) + 1;
    char* dst = new char[max];

    wcstombs_s(&len, dst, max, __wargv[1], max);
    config.filepath = std::string(dst);

    delete[] dst;

    return true;
}

}
