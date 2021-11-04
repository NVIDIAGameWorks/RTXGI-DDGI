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

#include "Common.h"

#include <queue>
#include <stack>

namespace Instrumentation
{

    enum class EStatType
    {
        CPU = 0,
        GPU,
    };

    struct Stat
    {
        Stat() {}
        Stat(EStatType type) { this->type = type; }
        Stat(EStatType type, std::string name, uint32_t sampleSize) 
        { 
            this->type = type;
            this->name = name;
            this->sampleSize = sampleSize;
        }
        Stat(EStatType type, uint32_t index, std::string name, uint32_t sampleSize)
        {
            this->type = type;
            this->index = index;
            this->name = name;
            this->sampleSize = sampleSize;
        }

        std::string name = "";
        EStatType type;

        uint32_t index = 0;
        uint64_t timestamp = 0;
        uint32_t sampleSize = 10;
        double elapsed = 0; // milliseconds
        double average = 0;
        double total = 0;
        std::queue<double> samples;

        uint32_t GetQueryBeginIndex() { return (index * 2); }
        uint32_t GetQueryEndIndex() { return (index * 2) + 1; }

    };

    struct Performance
    {
        std::vector<Stat*> gpuTimes;
        std::vector<Stat*> cpuTimes;

        uint32_t GetNumGPUQueries() { return static_cast<uint32_t>(gpuTimes.size() * 2); };

        Stat*& AddCPUStat(std::string name, uint32_t sampleSize = 50)
        {
            cpuTimes.emplace_back(new Stat(EStatType::CPU, name, sampleSize));
            return cpuTimes.back();
        }

        Stat*& AddGPUStat(std::string name, uint32_t sampleSize = 50)
        {
            uint32_t index = static_cast<uint32_t>(gpuTimes.size());
            gpuTimes.emplace_back(new Stat(EStatType::GPU, index, name, sampleSize));
            return gpuTimes.back();
        }

        void AddStat(std::string name, Stat*& cpu, Stat*& gpu, uint32_t sampleSize = 50)
        {
            cpu = AddCPUStat(name, sampleSize);
            gpu = AddGPUStat(name, sampleSize);
        }

        void Cleanup()
        {
            size_t index;
            for (index = 0; index < cpuTimes.size(); index++)
            {
                delete cpuTimes[index];
                cpuTimes[index] = nullptr;
            }

            for (index = 0; index < gpuTimes.size(); index++)
            {
                delete gpuTimes[index];
                gpuTimes[index] = nullptr;
            }

            cpuTimes.clear();
            gpuTimes.clear();
        }
    };

    void Begin(Stat* s);
    void End(Stat* s);
    void Resolve(Stat* s);
    void EndAndResolve(Stat* s);

}

#define CPU_TIMESTAMP_BEGIN(x) Begin(x)
#define CPU_TIMESTAMP_END(x) End(x)
#define CPU_TIMESTAMP_RESOLVE(x) Resolve(x)
#define CPU_TIMESTAMP_ENDANDRESOLVE(x) EndAndResolve(x)
