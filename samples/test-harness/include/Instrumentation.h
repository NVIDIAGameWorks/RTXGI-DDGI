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

#include <queue>
#include <stack>
#include <iostream>

namespace Instrumentation
{

    enum class EStatType
    {
        CPU = 0,
        GPU,
    };

    struct Stat
    {
        static uint32_t frameGPUQueryCount;
        static void ResetGPUQueryCount() { frameGPUQueryCount = 0; }

        Stat() {}
        Stat(EStatType type) { this->type = type; }
        Stat(EStatType type, std::string name, uint32_t sampleSize)
        {
            this->type = type;
            this->name = name;
            this->sampleSize = sampleSize;
        }

        const static uint32_t FallbackSampleSize = 10;

        std::string name = "";
        EStatType type;

        int32_t gpuQueryStartIndex = -1;
        int32_t gpuQueryEndIndex = -1;

        int32_t GetGPUQueryBeginIndex();
        int32_t GetGPUQueryEndIndex();
        void ResetGPUQueryIndices();

        uint64_t timestamp = 0;
        uint32_t sampleSize = FallbackSampleSize;
        double elapsed = 0; // milliseconds
        double average = 0;
        double total = 0;
        std::queue<double> samples;

        void Reset(uint32_t sampleSize = FallbackSampleSize)
        {
            elapsed = 0;
            average = 0;
            total = 0;
            samples = {};
            this->sampleSize = sampleSize;
        }

    };

    struct Performance
    {
        std::vector<Stat*> gpuTimes;
        std::vector<Stat*> cpuTimes;

        const static uint32_t DefaultSampleSize = 50;

        uint32_t GetNumActiveGPUQueries() const { return (Stat::frameGPUQueryCount * 2); }

        uint32_t GetNumTotalGPUQueries() const { return static_cast<uint32_t>(gpuTimes.size()) * 2; }

        Stat*& AddCPUStat(std::string name, uint32_t sampleSize = DefaultSampleSize)
        {
            cpuTimes.emplace_back(new Stat(EStatType::CPU, name, sampleSize));
            return cpuTimes.back();
        }

        Stat*& AddGPUStat(std::string name, uint32_t sampleSize = DefaultSampleSize)
        {
            gpuTimes.emplace_back(new Stat(EStatType::GPU, name, sampleSize));
            return gpuTimes.back();
        }

        void AddStat(std::string name, Stat*& cpu, Stat*& gpu, uint32_t sampleSize = DefaultSampleSize)
        {
            cpu = AddCPUStat(name, sampleSize);
            gpu = AddGPUStat(name, sampleSize);
        }

        void Reset(uint32_t sampleSize = DefaultSampleSize)
        {
            for (Stat* stat : cpuTimes)
            {
                stat->Reset(sampleSize);
            }

            for (Stat* stat : gpuTimes)
            {
                stat->Reset(sampleSize);
            }
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

    std::ostream& operator<<(std::ostream& os, Stat& stat);
    std::ostream& operator<<(std::ostream& os, std::vector<Stat*>& stats);

}

#define CPU_TIMESTAMP_BEGIN(x) Begin(x)
#define CPU_TIMESTAMP_END(x) End(x)
#define CPU_TIMESTAMP_RESOLVE(x) Resolve(x)
#define CPU_TIMESTAMP_ENDANDRESOLVE(x) EndAndResolve(x)
