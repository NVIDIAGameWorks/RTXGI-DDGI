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

#include "Instrumentation.h"
#include "Configs.h"
#include "Graphics.h"

#include <sstream>

namespace Benchmark
{
    const static uint32_t NumBenchmarkFrames = 1024;

    struct BenchmarkRun
    {
        uint32_t numFramesBenched = 0;
        std::stringstream cpuTimingCsv;
        std::stringstream gpuTimingCsv;
    };
    void StartBenchmark(BenchmarkRun& benchmarkRun, Instrumentation::Performance& perf, Configs::Config& config, Graphics::Globals& gfx);
    bool UpdateBenchmark(BenchmarkRun& benchmarkRun, Instrumentation::Performance& perf, Configs::Config& config, Graphics::Globals& gfx, std::ofstream& log);
}
