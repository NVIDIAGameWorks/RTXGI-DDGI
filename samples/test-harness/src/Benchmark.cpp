/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "Benchmark.h"

namespace Benchmark
{
    void StartBenchmark(BenchmarkRun& benchmarkRun, Instrumentation::Performance& perf, Configs::Config& config, Graphics::Globals& gfx)
    {
        benchmarkRun.cpuTimingCsv.str("");
        benchmarkRun.gpuTimingCsv.str("");
        // Clear timer history if beginning benchmark mode, this should not break timers that are currently running
        perf.Reset(NumBenchmarkFrames);
        if (config.app.renderMode == ERenderMode::DDGI)
        {
            // reload ddgi configs in order to reset RNG state
            config.ddgi.reload = true;
        }
        gfx.frameNumber = 1;
    }

    bool UpdateBenchmark(BenchmarkRun& benchmarkRun, Instrumentation::Performance& perf, Configs::Config& config, Graphics::Globals& gfx, std::ofstream& log)
    {
        // if benchmark is currently running, make a csv row for the frame's timings
        benchmarkRun.cpuTimingCsv << gfx.frameNumber << ",";
        benchmarkRun.gpuTimingCsv << gfx.frameNumber << ",";

        // print the timer values to the row
        benchmarkRun.cpuTimingCsv << perf.cpuTimes;
        benchmarkRun.gpuTimingCsv << perf.gpuTimes;

        // if benchmark is done, print the timing results to files
        if (gfx.frameNumber >= NumBenchmarkFrames)
        {
            // generate the header row of the csvs
            std::stringstream header;
            header << "FrameIndex,";
            for (std::vector<Instrumentation::Stat*>::const_iterator& it = perf.cpuTimes.cbegin(); it != perf.cpuTimes.cend(); it++)
            {
                Instrumentation::Stat* stat = *it;
                header << stat->name << ",";
            }
            std::string cpuHeader = header.str();

            header.str("");
            header << "FrameIndex,";
            for (std::vector<Instrumentation::Stat*>::const_iterator& it = perf.gpuTimes.cbegin(); it != perf.gpuTimes.cend(); it++)
            {
                Instrumentation::Stat* stat = *it;
                header << stat->name << ",";
            }
            std::string gpuHeader = header.str();

            // write csv to file
            std::ofstream csv;
            csv.open(config.scene.screenshotPath + "\\benchmarkCpu.csv", std::ios::out);
            if (csv.is_open())
            {
                csv << cpuHeader << std::endl << benchmarkRun.cpuTimingCsv.str();
            }
            csv.close();
            csv.open(config.scene.screenshotPath + "\\benchmarkGpu.csv", std::ios::out);
            if (csv.is_open())
            {
                csv << gpuHeader << std::endl << benchmarkRun.gpuTimingCsv.str();
            }
            csv.close();
            log << "wrote benchmark results to csv." << std::endl;

            // print averages to the log file
            log << "Benchmark Timings:" << std::endl;
            for (Instrumentation::Stat* stat : perf.cpuTimes)
            {
                log << "\t" << stat->name << "=" << stat->average << "ms(CPU)" << std::endl;;
            }
            for (Instrumentation::Stat* stat : perf.gpuTimes)
            {
                log << "\t" << stat->name << "=" << stat->average << "ms(GPU)" << std::endl;;
            }

            return false;
        }
        return true;
    }
}