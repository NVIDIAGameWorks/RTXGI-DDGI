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

//----------------------------------------------------------------------------------------------------------
// Parsing Helpers
//----------------------------------------------------------------------------------------------------------

struct ParseLineInfo
{
    bool status = true;
    const string* str;
    ofstream* log;
};

string ParseValue(ParseLineInfo &data)
{
    size_t pos = data.str->find('=');
    if (pos == string::npos)
    {
        *data.log << "Failed to parse config file at the line with: " << data.str->c_str() << endl;
        data.status = false;
    }

    size_t endPos = data.str->find_first_of("#\t%\x0D\x0A", pos); // any of the possible delimiters afer the value
    if (endPos - pos == 0)
    {
        *data.log << "Failed to parse config file at the line with: " << data.str->c_str() << endl;
        data.status = false;
    }

    return data.str->substr(pos + 1, endPos - pos - 1);
};

void Parse(ParseLineInfo &data, string &result)
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
    string line = ParseValue(data);
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
    string line = ParseValue(data);
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
bool ParseIfExists(const char* parameter, T& dst, const string& line, bool &status, ofstream &log)
{
    size_t found = line.find(parameter);
    if (found != string::npos)
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

bool ParseApplication(string &configString, ConfigInfo &config, ofstream& log)
{
    string line;
    stringstream stream;
    
    size_t index = configString.find("#[application]");
    if (index == string::npos) return true;

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
        if (ParseIfExists("scene=", config.scene, line, status, log)) continue;
        
        if (!status) return false;
    }
    return true;
}

bool ParseLights(string &configString, LightInfo &lights, ofstream &log)
{
    bool result;
    string line;
    stringstream stream;

    size_t index = configString.find("#[lights]");
    if (index == string::npos) return true;

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

bool ParseCamera(string &configString, CameraInfo &camera, InputInfo &input, ofstream &log)
{
    string line;
    stringstream stream;

    size_t index = configString.find("#[camera]");
    if (index == string::npos) return true;

    stream.str(configString.substr(index));

    bool status = true;
    while (!stream.eof())
    {
        getline(stream, line, '\n');
        if (line.find_first_of("#") == 0) continue;     // commented line, skip it

        if (ParseIfExists("cameraPosition=", camera.origin, line, status, log)) continue;
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

bool ParseVolume(string &configString, DDGIVolumeDesc &desc, ofstream &log)
{
    string line;
    stringstream stream;

    size_t index = configString.find("#[volume]");
    if (index == string::npos) return true;

    stream.str(configString.substr(index));

    bool status = true;
    while(!stream.eof())
    {
        getline(stream, line, '\n');
        if (line.find_first_of("#") == 0) continue;     // comment line, skip it

        if (ParseIfExists("origin=", desc.origin, line, status, log)) continue;
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

        if (!status) return false;
    }
    return true;
}

bool ParseInput(string &configString, InputOptions &options, ofstream &log)
{
    string line;
    stringstream stream;

    size_t index = configString.find("#[input]");
    if (index == string::npos) return true;

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

bool ParseRayTracing(string &configString, RTOptions &options, ofstream &log)
{
    string line;
    stringstream stream;

    size_t index = configString.find("#[raytracing]");
    if (index == string::npos) return true;

    stream.str(configString.substr(index));

    bool status = true;
    while (!stream.eof())
    {
        getline(stream, line, '\n');
        if (line.find_first_of("#") == 0) continue;     // commented line, skip it

        if (ParseIfExists("rtViewBias=", options.viewBias, line, status, log)) continue;
        if (ParseIfExists("rtNormalBias=", options.normalBias, line, status, log)) continue;
        if (ParseIfExists("rtNumBounces=", options.numBounces, line, status, log)) continue;

        if (!status) return false;
    }
    return true;
}

bool ParsePostProcess(string &configString, PostProcessOptions &options, ofstream &log)
{
    string line;
    stringstream stream;

    size_t index = configString.find("#[postprocess]");
    if (index == string::npos) return true;

    stream.str(configString.substr(index));

    bool status = true;
    while (!stream.eof())
    {
        getline(stream, line, '\n');
        if (line.find_first_of("#") == 0) continue;     // commented line, skip it

        if (ParseIfExists("AORadius=", options.AORadius, line, status, log)) continue;
        if (ParseIfExists("AOFilterDepthSigma=", options.AOFilterDepthSigma, line, status, log)) continue;

        if (!status) return false;
    }
    return true;
}

bool ParseViz(string &configString, VizOptions &options, ofstream &log)
{
    string line;
    stringstream stream;

    size_t index = configString.find("#[visualization]");
    if (index == string::npos) return true;

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
    CameraInfo &camera,
    DDGIVolumeDesc &desc,
    InputInfo &inputInfo,
    InputOptions &inputOptions,
    RTOptions &rtOptions,
    PostProcessOptions &postOptions,
    VizOptions &vizOptions,
    ofstream &log)
{
    string configString(buffer);

    if (!ParseApplication(configString, config, log)) return false;
    if (!ParseLights(configString, lights, log)) return false;
    if (!ParseCamera(configString, camera, inputInfo, log)) return false;
    if (!ParseVolume(configString, desc, log)) return false;
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
    CameraInfo &camera,
    DDGIVolumeDesc &desc,
    InputInfo &inputInfo,
    InputOptions &inputOptions,
    RTOptions &rtOptions,
    PostProcessOptions &postOptions,
    VizOptions &vizOptions,
    ofstream &log)
{
    // Load the config file
    ifstream file(config.filepath, ios::ate | ios::binary);
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
    bool result = ParseConfig(buffer, config, lights, camera, desc, inputInfo, inputOptions, rtOptions, postOptions, vizOptions, log);

    // Delete the config file buffer
    delete[] buffer;

    // Check the probe ray count
    if (desc.numRaysPerProbe % desc.numIrradianceTexels != 0)
    {
        log << "Warning: numRaysPerProbe is not a multiple of numIrradianceTexels. This is not an optimal configuration when using shared memory during blending.\n";
    }

    return result;
}

/**
 * Parse the command line to get the configuration file path.
 */
bool ParseCommandLine(LPWSTR lpCmdLine, ConfigInfo &config, ofstream &log)
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
    config.filepath = string(dst);

    delete[] dst;

    return true;
}

}
