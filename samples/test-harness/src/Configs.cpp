/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "Configs.h"

#include <rtxgi/ddgi/DDGIVolume.h>

#include <sstream>
#include <stdlib.h>
#include <filesystem>

#define PARSE_CHECK(statement, lineNumber, log) { if(!statement) { log << "\nFailed to parse config file at line " << lineNumber; return false; } }

using namespace DirectX;

namespace Configs
{

    //----------------------------------------------------------------------------------------------------------
    // Private Parsing Helpers
    //----------------------------------------------------------------------------------------------------------

    /**
     * Extract a string of the data, stripping out unnecessary characters.
     */
    bool Extract(const std::string& input, std::string& output)
    {
        size_t endPos = input.find_first_of("#\t%\x0D\x0A"); // any of the possible delimiters after the value
        if (endPos == 0) return false;
        output = input.substr(0, endPos);
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

    void Store(std::string source, bool& destination)
    {
        destination = (bool)stoi(source);
    }

    void Store(std::string source, int& destination)
    {
        destination = stoi(source);
    }

    void Store(std::string source, unsigned int& destination)
    {
        destination = (unsigned int)stoi(source);
    }

    void Store(std::string source, float& destination)
    {
        destination = stof(source);
    }

    void StoreWorldCounts(std::string source, XMINT3& destination)
    {
        // Note: used to store world-space probe counts, negative values are not allowed
        std::vector<std::string> values = Split(source, ' ');
    #if COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT || COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT
        destination = { static_cast<int32_t>(stol(values[0])), static_cast<int32_t>(stol(values[1])), static_cast<int32_t>(stol(values[2])) };
    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT_Z_UP
        destination = { static_cast<int32_t>(stol(values[0])), static_cast<int32_t>(stol(values[2])), static_cast<int32_t>(stol(values[1])) };
    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT_Z_UP
        destination = { static_cast<int32_t>(stol(values[2])), static_cast<int32_t>(stol(values[0])), static_cast<int32_t>(stol(values[1])) };
    #endif
    }

    void Store(std::string source, XMINT3& destination)
    {
        std::vector<std::string> values = Split(source, ' ');
        destination = { static_cast<int32_t>(stol(values[0])), static_cast<int32_t>(stol(values[1])), static_cast<int32_t>(stol(values[2])) };
    }

    void StoreWorldVector(std::string source, XMFLOAT3& destination)
    {
        // Note: used to store world-space positions and directions
        std::vector<std::string> values = Split(source, ' ');
    #if COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT
        destination = { stof(values[0]), stof(values[1]), stof(values[2]) };
    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT_Z_UP
        destination = { stof(values[0]), -stof(values[2]), stof(values[1]) };
    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT
        destination = { stof(values[0]), stof(values[1]), -stof(values[2]) };
    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT_Z_UP
        destination = { -stof(values[2]), stof(values[0]), stof(values[1]) };
    #endif
    }

    void StoreEulerAngles(std::string source, XMFLOAT3& destination)
    {
        // Store Euler angles for volume rotation
        std::vector<std::string> values = Split(source, ' ');

        rtxgi::float3 radians = rtxgi::ConvertEulerAngles({stof(values[0]), stof(values[1]), stof(values[2])}, static_cast<rtxgi::ECoordinateSystem>(COORDINATE_SYSTEM));
        destination = { radians.x, radians.y, radians.z };
    }

    void StoreWorldDistance(std::string source, XMFLOAT3& destination)
    {
        // Note: used to store world-space distance values (between probes), negative values are not allowed
        std::vector<std::string> values = Split(source, ' ');
    #if COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT || COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT
        destination = { stof(values[0]), stof(values[1]), stof(values[2]) };
    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_RIGHT_Z_UP
        destination = { stof(values[0]), stof(values[2]), stof(values[1]) };
    #elif COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT_Z_UP
        destination = { stof(values[2]), stof(values[0]), stof(values[1]) };
    #endif
    }

    void Store(std::string source, XMFLOAT3& destination)
    {
        std::vector<std::string> values = Split(source, ' ');
        destination = { stof(values[0]), stof(values[1]), stof(values[2]) };
    }

    void Store(std::string source, ERenderMode& destination)
    {
        destination = (ERenderMode)stoi(source);
    }

    void Store(std::string source, ELightType& destination)
    {
        destination = (ELightType)stoi(source);
    }

    void Store(std::string source, rtxgi::EDDGIVolumeTextureFormat& destination)
    {
        destination = (rtxgi::EDDGIVolumeTextureFormat)stoi(source);
    }

    void Store(std::string source, rtxgi::EDDGIVolumeProbeVisType& destination)
    {
        destination = (rtxgi::EDDGIVolumeProbeVisType)stoi(source);
    }

    /**
     * Parse a post process configuration entry.
     */
    bool ParseConfigPostProcessEntry(const std::vector<std::string>& tokens, const std::string& rhs, Config& config, uint32_t lineNumber, std::ofstream& log)
    {
        // Post process entries have no more than 3 tokens
        PARSE_CHECK((tokens.size() <= 3), lineNumber, log);

        // Extract the data from the rhs, stripping out unnecessary characters
        std::string data;
        PARSE_CHECK(Extract(rhs, data), lineNumber, log);

        if (tokens.size() == 2)
        {
            if (tokens[1].compare("enable") == 0) { Store(data, config.postProcess.enabled); return true; }
        }

        if (tokens[1].compare("exposure") == 0)
        {
            if (tokens[2].compare("enable") == 0) { Store(data, config.postProcess.exposure.enabled); return true; }
            if (tokens[2].compare("fstops") == 0) { Store(data, config.postProcess.exposure.fstops); return true; }
        }

        if (tokens[1].compare("tonemap") == 0)
        {
            if (tokens[2].compare("enable") == 0) { Store(data, config.postProcess.tonemap.enabled); return true; }
        }

        if (tokens[1].compare("dither") == 0)
        {
            if (tokens[2].compare("enable") == 0) { Store(data, config.postProcess.dither.enabled); return true; }
        }

        if (tokens[1].compare("gamma") == 0)
        {
            if (tokens[2].compare("enable") == 0) { Store(data, config.postProcess.gamma.enabled); return true; }
        }

        log << "\nUnsupported configuration value specified!";
        PARSE_CHECK(0, lineNumber, log);
        return false;
    }

    /**
     * Parse a DDGI configuration entry.
     */
    bool ParseConfigDDGIEntry(const std::vector<std::string>& tokens, const std::string& rhs, Config& config, uint32_t lineNumber, std::ofstream& log)
    {
        // DDGI entries have no more than 6 tokens
        PARSE_CHECK((tokens.size() <= 6), lineNumber, log);

        // Extract the data from the rhs, stripping out unnecessary characters
        std::string data;
        PARSE_CHECK(Extract(rhs, data), lineNumber, log);

        if (tokens[1].compare("volume") == 0)
        {
            int volumeIndex = stoi(tokens[2]);
            if (volumeIndex >= static_cast<int>(config.ddgi.volumes.size()))
            {
                config.ddgi.volumes.emplace_back();
                config.ddgi.volumes.back().index = volumeIndex;
            }

            if (tokens[3].compare("name") == 0) { config.ddgi.volumes[volumeIndex].name = data; return true; }
            if (tokens[3].compare("origin") == 0) { StoreWorldVector(data, config.ddgi.volumes[volumeIndex].origin); return true; }
            if (tokens[3].compare("rotation") == 0) { StoreEulerAngles(data, config.ddgi.volumes[volumeIndex].eulerAngles); return true; }
            if (tokens[3].compare("probeCounts") == 0) { StoreWorldCounts(data, config.ddgi.volumes[volumeIndex].probeCounts); return true; }
            if (tokens[3].compare("probeSpacing") == 0) { StoreWorldDistance(data, config.ddgi.volumes[volumeIndex].probeSpacing); return true; }
            if (tokens[3].compare("probeNumRays") == 0) { Store(data, config.ddgi.volumes[volumeIndex].probeNumRays); return true; }
            if (tokens[3].compare("probeNumIrradianceTexels") == 0) { Store(data, config.ddgi.volumes[volumeIndex].probeNumIrradianceTexels); return true; }
            if (tokens[3].compare("probeNumDistanceTexels") == 0) { Store(data, config.ddgi.volumes[volumeIndex].probeNumDistanceTexels); return true; }
            if (tokens[3].compare("probeHysteresis") == 0) { Store(data, config.ddgi.volumes[volumeIndex].probeHysteresis); return true; }
            if (tokens[3].compare("probeNormalBias") == 0) { Store(data, config.ddgi.volumes[volumeIndex].probeNormalBias); return true; }
            if (tokens[3].compare("probeViewBias") == 0) { Store(data, config.ddgi.volumes[volumeIndex].probeViewBias); return true; }
            if (tokens[3].compare("probeMaxRayDistance") == 0) { Store(data, config.ddgi.volumes[volumeIndex].probeMaxRayDistance); return true; }
            if (tokens[3].compare("probeIrradianceThreshold") == 0) { Store(data, config.ddgi.volumes[volumeIndex].probeIrradianceThreshold); return true; }
            if (tokens[3].compare("probeBrightnessThreshold") == 0) { Store(data, config.ddgi.volumes[volumeIndex].probeBrightnessThreshold); return true; }
            if (tokens[3].compare("rngSeed") == 0) { Store(data, config.ddgi.volumes[volumeIndex].rngSeed); return true; }

            if (tokens[3].compare("probeRelocation") == 0)
            { 
                if (tokens.size() == 5 && tokens[4].compare("enabled") == 0)
                {
                    Store(data, config.ddgi.volumes[volumeIndex].probeRelocationEnabled);
                    return true;
                }
                else if (tokens.size() == 5 && tokens[4].compare("minFrontfaceDistance") == 0)
                {
                    Store(data, config.ddgi.volumes[volumeIndex].probeMinFrontfaceDistance);
                    return true;
                }
            }

            if (tokens[3].compare("probeClassification") == 0)
            {
                if (tokens.size() == 5 && tokens[4].compare("enabled") == 0)
                {
                    Store(data, config.ddgi.volumes[volumeIndex].probeClassificationEnabled); return true;
                }
            }

            if (tokens[3].compare("probeVariability") == 0)
            {
                if (tokens.size() == 5 && tokens[4].compare("enabled") == 0)
                {
                    Store(data, config.ddgi.volumes[volumeIndex].probeVariabilityEnabled); return true;
                }
                else if (tokens.size() == 5 && tokens[4].compare("threshold") == 0)
                {
                    Store(data, config.ddgi.volumes[volumeIndex].probeVariabilityThreshold); return true;
                }
            }

            if (tokens[3].compare("infiniteScrolling") == 0)
            {
                if (tokens.size() == 5 && tokens[4].compare("enabled") == 0)
                {
                    Store(data, config.ddgi.volumes[volumeIndex].infiniteScrollingEnabled);
                    return true;
                }
            }

            if (tokens[3].compare("textures") == 0)
            {
                if (tokens[4].compare("rayData") == 0 && tokens[5].compare("format") == 0)
                {
                    Store(data, config.ddgi.volumes[volumeIndex].textureFormats.rayDataFormat);
                    return true;
                }
                else if (tokens[4].compare("irradiance") == 0 && tokens[5].compare("format") == 0)
                {
                    Store(data, config.ddgi.volumes[volumeIndex].textureFormats.irradianceFormat);
                    return true;
                }
                else if (tokens[4].compare("distance") == 0 && tokens[5].compare("format") == 0)
                {
                    Store(data, config.ddgi.volumes[volumeIndex].textureFormats.distanceFormat);
                    return true;
                }
                else if (tokens[4].compare("data") == 0 && tokens[5].compare("format") == 0)
                {
                    Store(data, config.ddgi.volumes[volumeIndex].textureFormats.dataFormat);
                    return true;
                }
                else if (tokens[4].compare("variability") == 0 && tokens[5].compare("format") == 0)
                {
                    Store(data, config.ddgi.volumes[volumeIndex].textureFormats.variabilityFormat);
                    return true;
                }
            }

            if (tokens[3].compare("vis") == 0)
            {
                if(tokens[4].compare("probeVisType") == 0)
                {
                    Store(data, config.ddgi.volumes[volumeIndex].probeVisType);
                    return true;
                }

                if (tokens[4].compare("probeRadius") == 0)
                {
                    Store(data, config.ddgi.volumes[volumeIndex].probeRadius);
                    return true;
                }

                if (tokens[4].compare("probeDistanceDivisor") == 0)
                {
                    Store(data, config.ddgi.volumes[volumeIndex].probeDistanceDivisor);
                    return true;
                }

                if (tokens[4].compare("showProbes") == 0)
                {
                    Store(data, config.ddgi.volumes[volumeIndex].showProbes);
                    return true;
                }

                if (tokens[4].compare("texture") == 0)
                {
                    if (tokens[5].compare("rayDataScale") == 0)
                    {
                        Store(data, config.ddgi.volumes[volumeIndex].probeRayDataScale);
                        return true;
                    }

                    if (tokens[5].compare("irradianceScale") == 0)
                    {
                        Store(data, config.ddgi.volumes[volumeIndex].probeIrradianceScale);
                        return true;
                    }

                    if (tokens[5].compare("distanceScale") == 0)
                    {
                        Store(data, config.ddgi.volumes[volumeIndex].probeDistanceScale);
                        return true;
                    }

                    if (tokens[5].compare("probeDataScale") == 0)
                    {
                        Store(data, config.ddgi.volumes[volumeIndex].probeDataScale);
                        return true;
                    }

                    if (tokens[5].compare("probeVariabilityScale") == 0)
                    {
                        Store(data, config.ddgi.volumes[volumeIndex].probeVariabilityScale);
                        return true;
                    }
                }
            }
        }

        log << "\nUnsupported configuration value specified!";
        PARSE_CHECK(0, lineNumber, log);
        return false;
    }

    /*
    * Parse a ray traced ambient occlusion configuration entry.
    */
    bool ParseConfigRTAOEntry(const std::vector<std::string>& tokens, const std::string& rhs, Config& config, uint32_t lineNumber, std::ofstream& log)
    {
        // RTAO entries have exactly 2 tokens
        PARSE_CHECK((tokens.size() == 2), lineNumber, log);

        // Extract the data from the rhs, stripping out unnecessary characters
        std::string data;
        PARSE_CHECK(Extract(rhs, data), lineNumber, log);

        if (tokens[1].compare("enable") == 0) { Store(data, config.rtao.enabled); return true; }
        if (tokens[1].compare("rayLength") == 0) { Store(data, config.rtao.rayLength); return true; }
        if (tokens[1].compare("rayNormalBias") == 0) { Store(data, config.rtao.rayNormalBias); return true; }
        if (tokens[1].compare("rayViewBias") == 0) { Store(data, config.rtao.rayViewBias); return true; }
        if (tokens[1].compare("powerLog") == 0) { Store(data, config.rtao.powerLog); return true; }
        if (tokens[1].compare("filterDistanceSigma") == 0) { Store(data, config.rtao.filterDistanceSigma); return true; }
        if (tokens[1].compare("filterDepthSigma") == 0) { Store(data, config.rtao.filterDepthSigma); return true; }

        log << "\nUnsupported configuration value specified!";
        PARSE_CHECK(0, lineNumber, log);
        return false;
    }

    /*
    * Parse a path trace configuration entry.
    */
    bool ParseConfigPathTraceEntry(const std::vector<std::string>& tokens, const std::string& rhs, Config& config, uint32_t lineNumber, std::ofstream& log)
    {
        // Path trace entries have exactly 2 tokens
        PARSE_CHECK((tokens.size() == 2), lineNumber, log);

        // Extract the data from the rhs, stripping out unnecessary characters
        std::string data;
        PARSE_CHECK(Extract(rhs, data), lineNumber, log);

        if (tokens[1].compare("rayNormalBias") == 0) { Store(data, config.pathTrace.rayNormalBias); return true; }
        if (tokens[1].compare("rayViewBias") == 0) { Store(data, config.pathTrace.rayViewBias); return true; }
        if (tokens[1].compare("numBounces") == 0) { Store(data, config.pathTrace.numBounces); return true; }
        if (tokens[1].compare("samplesPerPixel") == 0) { Store(data, config.pathTrace.samplesPerPixel); return true; }
        if (tokens[1].compare("antialiasing") == 0) { Store(data, config.pathTrace.antialiasing); return true; }

        log << "\nUnsupported configuration value specified!";
        PARSE_CHECK(0, lineNumber, log);
        return false;
    }

    /*
    * Parse an input configuration entry.
    */
    bool ParseConfigInputEntry(const std::vector<std::string>& tokens, const std::string& rhs, Config& config, uint32_t lineNumber, std::ofstream& log)
    {
        // Input entries have exactly 2 tokens
        PARSE_CHECK((tokens.size() == 2), lineNumber, log);

        // Extract the data from the rhs, stripping out unnecessary characters
        std::string data;
        PARSE_CHECK(Extract(rhs, data), lineNumber, log);

        if (tokens[1].compare("movementSpeed") == 0) { Store(data, config.input.movementSpeed); return true; }
        if (tokens[1].compare("rotationSpeed") == 0) { Store(data, config.input.rotationSpeed); return true; }
        if (tokens[1].compare("invertPan") == 0) { Store(data, config.input.invertPan); return true; }

        log << "\nUnsupported configuration value specified!";
        PARSE_CHECK(0, lineNumber, log);
        return false;
    }

    /**
     * Parse a scene configuration entry.
     */
    bool ParseConfigSceneEntry(const std::vector<std::string>& tokens, const std::string& rhs, Config& config, uint32_t lineNumber, std::ofstream& log)
    {
        // Scene config entries have no more than 4 tokens
        PARSE_CHECK((tokens.size() <= 4), lineNumber, log);

        // Extract the data from the rhs, stripping out unnecessary characters
        std::string data;
        PARSE_CHECK(Extract(rhs, data), lineNumber, log);

        // Scene configs
        if (tokens.size() == 2)
        {
            if (tokens[1].compare("name") == 0) { config.scene.name = data; return true; }
            if (tokens[1].compare("path") == 0) { config.scene.path = data; return true; }
            if (tokens[1].compare("file") == 0) { config.scene.file = data; return true; }
            if (tokens[1].compare("screenshotPath") == 0) { config.scene.screenshotPath = data; return true; }
            if (tokens[1].compare("skyColor") == 0) { Store(data, config.scene.skyColor); return true; }
            if (tokens[1].compare("skyIntensity") == 0) { Store(data, config.scene.skyIntensity); return true; }
        }

        // Lights
        if (tokens[1].compare("lights") == 0)
        {
            int lightIndex = stoi(tokens[2]);
            if (lightIndex >= config.scene.lights.size()) config.scene.lights.emplace_back();

            if (tokens[3].compare("name") == 0) { config.scene.lights[lightIndex].name = data; return true; }
            if (tokens[3].compare("type") == 0) { Store(data, config.scene.lights[lightIndex].type); return true; }
            if (tokens[3].compare("position") == 0) { StoreWorldVector(data, config.scene.lights[lightIndex].position); return true; }
            if (tokens[3].compare("direction") == 0) { StoreWorldVector(data, config.scene.lights[lightIndex].direction); return true; }
            if (tokens[3].compare("color") == 0) { Store(data, config.scene.lights[lightIndex].color); return true; }
            if (tokens[3].compare("power") == 0) { Store(data, config.scene.lights[lightIndex].power); return true; }
            if (tokens[3].compare("radius") == 0) { Store(data, config.scene.lights[lightIndex].radius); return true; }
            if (tokens[3].compare("umbraAngle") == 0) { Store(data, config.scene.lights[lightIndex].umbraAngle); return true; }
            if (tokens[3].compare("penumbraAngle") == 0) { Store(data, config.scene.lights[lightIndex].penumbraAngle); return true; }
        }

        // Cameras
        if (tokens[1].compare("cameras") == 0)
        {
            int cameraIndex = stoi(tokens[2]);
            if (cameraIndex >= config.scene.cameras.size()) config.scene.cameras.emplace_back();

            if (tokens[3].compare("name") == 0) { config.scene.cameras[cameraIndex].name = data; return true; }
            if (tokens[3].compare("position") == 0) { StoreWorldVector(data, config.scene.cameras[cameraIndex].position); return true; }
            if (tokens[3].compare("fov") == 0) { Store(data, config.scene.cameras[cameraIndex].fov); return true; }
            if (tokens[3].compare("aspect") == 0) { Store(data, config.scene.cameras[cameraIndex].aspect); return true; }
            if (tokens[3].compare("yaw") == 0) { Store(data, config.scene.cameras[cameraIndex].yaw); return true; }
            if (tokens[3].compare("pitch") == 0) { Store(data, config.scene.cameras[cameraIndex].pitch); return true; }
        }

        log << "\nUnsupported configuration value specified!";
        PARSE_CHECK(0, lineNumber, log);
        return false;
    }

    /**
     * Parse an application configuration entry.
     */
    bool ParseConfigAppEntry(const std::vector<std::string>& tokens, const std::string& rhs, Config& config, uint32_t lineNumber, std::ofstream& log)
    {
        // App config entries have exactly 2 tokens
        PARSE_CHECK((tokens.size() == 2), lineNumber, log);

        // Extract the data from the rhs, stripping out unnecessary characters
        std::string data;
        PARSE_CHECK(Extract(rhs, data), lineNumber, log);

        if (tokens[1].compare("width") == 0) { Store(data, config.app.width); return true; }
        if (tokens[1].compare("height") == 0) { Store(data, config.app.height); return true; }
        if (tokens[1].compare("vsync") == 0) { Store(data, config.app.vsync); return true; }
        if (tokens[1].compare("fullscreen") == 0) { Store(data, config.app.fullscreen); return true; }
        if (tokens[1].compare("showUI") == 0) { Store(data, config.app.showUI); return true; }
        if (tokens[1].compare("root") == 0)
        {
            std::filesystem::path configFilePath(config.app.filepath);
            std::filesystem::path appRootRelativeToConfig(data);
            std::filesystem::path appRootRelativeToRunDirectory = configFilePath.parent_path() / appRootRelativeToConfig;
            config.app.root = appRootRelativeToRunDirectory.lexically_normal().string();
            return true;
        }
        if (tokens[1].compare("rtxgiSDK") == 0)
        {
            std::filesystem::path configFilePath(config.app.filepath);
            std::filesystem::path sdkRootRelativeToConfig(data);
            std::filesystem::path sdkRootrelativeToRunDirectory = configFilePath.parent_path() / sdkRootRelativeToConfig;
            config.app.rtxgi = sdkRootrelativeToRunDirectory.lexically_normal().string();
            return true;
        }
        if (tokens[1].compare("title") == 0) { config.app.title = data; return true; }

        if (tokens[1].compare("renderMode") == 0) {
            Store(data, config.app.renderMode);
            config.pathTrace.enabled = (config.app.renderMode == ERenderMode::PATH_TRACE);
            config.ddgi.enabled = (config.app.renderMode == ERenderMode::DDGI);
            return true;
        }

        log << "\nUnsupported configuration value specified!";
        PARSE_CHECK(0, lineNumber, log);
        return false;
    }

    /**
     * Parse the configuration file.
     */
    bool ParseConfig(const char* buffer, Config& config, std::ofstream& log)
    {
        std::string file(buffer);
        std::stringstream stream;
        std::string line;
        uint32_t lineNumber = 0;

        stream.str(file);
        while (!stream.eof())
        {
            getline(stream, line, '\n');
            if (stream.eof()) break;
            lineNumber++;
            if (line.length() == 0) continue;                      // line break, skip it
            if (line.find_first_of("#\t%\x0D\x0A") == 0) continue; // commented line, skip it

            // Get the variable and data to assign
            std::vector<std::string> expression = Split(line, '=');
            PARSE_CHECK((expression.size() == 2), lineNumber, log);

            // Split the variable expression on each '.' character
            std::vector<std::string> tokens = Split(expression[0]);

            if (tokens[0].compare("app") == 0) { CHECK(ParseConfigAppEntry(tokens, expression[1], config, lineNumber, log), "parse config application entry!", log); continue; };
            if (tokens[0].compare("scene") == 0) { CHECK(ParseConfigSceneEntry(tokens, expression[1], config, lineNumber, log), "parse config scene entry!", log); continue; };
            if (tokens[0].compare("input") == 0) { CHECK(ParseConfigInputEntry(tokens, expression[1], config, lineNumber, log), "parse config input entry!", log); continue; };
            if (tokens[0].compare("pt") == 0) { CHECK(ParseConfigPathTraceEntry(tokens, expression[1], config, lineNumber, log), "parse config path trace entry!", log); continue; };
            if (tokens[0].compare("ddgi") == 0) { CHECK(ParseConfigDDGIEntry(tokens, expression[1], config, lineNumber, log), "parse config ddgi entry!", log); continue; };
            if (tokens[0].compare("rtao") == 0) { CHECK(ParseConfigRTAOEntry(tokens, expression[1], config, lineNumber, log), "parse config rtao entry!", log); continue; };
            if (tokens[0].compare("pp") == 0) { CHECK(ParseConfigPostProcessEntry(tokens, expression[1], config, lineNumber, log), "parse config post process entry!", log); continue; };
        }

        // Check the probe ray counts for each volume
        for (uint32_t volumeIndex = 0; volumeIndex < static_cast<uint32_t>(config.ddgi.volumes.size()); volumeIndex++)
        {
            if (config.ddgi.volumes[volumeIndex].probeNumRays % config.ddgi.volumes[volumeIndex].probeNumIrradianceTexels != 0)
            {
                log << "\nWarning: probeNumRays is not a multiple of probeNumIrradianceTexels! This is not an optimal configuration when using shared memory with DDGI.\n";
            }
        }

        return true;
    }

    //----------------------------------------------------------------------------------------------------------
    // Public Functions
    //----------------------------------------------------------------------------------------------------------

    /**
     * Parse the command line arguments and get the configuration file path.
     */
    bool ParseCommandLine(const std::vector<std::string>& arguments, Config& config, std::ofstream& log)
    {
        if (arguments.size() == 0)
        {
            // Early out, a configuration file is not specified
            log << "\nError: a configuration file must be specified!\n";
            return false;
        }

        if (arguments.size() > 1)
        {
            // Early out, there must be a single argument after the executable path
            log << "\nError: incorrect command line usage! A single argument - the path to the configuration file - must be specified.\n";
            return false;
        }

        // Set the config file path
        config.app.filepath = arguments[0];

        return true;
    }

    /**
     * Load and parse the configuration file.
     */
    bool Load(Config& config, std::ofstream& log)
    {
        // Load the config file
        std::ifstream file(config.app.filepath, std::ios::ate | std::ios::binary);
        if (!file.is_open())
        {
            log << "\nError: failed to load configuration file: '" << config.app.filepath << "'\n";
            return false;
        }

        // Read the config file to a buffer
        size_t fileSize = (size_t)file.tellg();
        char* buffer = new char[fileSize];

        file.seekg(0);
        file.read(buffer, fileSize);
        file.close();

        // Parse the config file
        CHECK(ParseConfig(buffer, config, log), "parse config file!", log);

        // Delete the config file buffer
        delete[] buffer;

        return true;
    }

}
