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

#include <filesystem>

namespace Benchmark
{
    void StartBenchmark(BenchmarkRun& benchmarkRun, Instrumentation::Performance& perf, Configs::Config& config, Graphics::Globals& gfx)
    {
        std::filesystem::create_directories(config.scene.screenshotPath.c_str());

        benchmarkRun.numFramesBenched = 0;
        benchmarkRun.cpuTimingCsv.str("");
        benchmarkRun.gpuTimingCsv.str("");

        // Clear timer history when starting benchmark mode
        perf.Reset(NumBenchmarkFrames);
        if (config.app.renderMode == ERenderMode::DDGI)
        {
            // Reload ddgi configs to reset the RNG state
            config.ddgi.reload = true;
        }
        gfx.frameNumber = 1;
        config.app.benchmarkRunning = true;
    }

    bool UpdateBenchmark(BenchmarkRun& benchmarkRun, Instrumentation::Performance& perf, Configs::Config& config, Graphics::Globals& gfx, std::ofstream& log)
    {
        config.app.benchmarkProgress = (uint32_t)(((float)benchmarkRun.numFramesBenched / (float)NumBenchmarkFrames) * 100.f);

        // If the benchmark is currently running, make a row for the frame's timings
        if(benchmarkRun.numFramesBenched < NumBenchmarkFrames)
        {
            benchmarkRun.cpuTimingCsv << gfx.frameNumber << ",";
            benchmarkRun.gpuTimingCsv << gfx.frameNumber << ",";
            benchmarkRun.cpuTimingCsv << perf.cpuTimes;
            benchmarkRun.gpuTimingCsv << perf.gpuTimes;
        }
        else
        {
            // If the benchmark is done, write the timing results to disk
            size_t index;
            std::string cpuHeader = "";
            std::string gpuHeader = "";

            std::vector<float> cpuAverages;
            std::vector<float> gpuAverages;

            // Generate the average timing rows for the CPU
            cpuHeader.append("Average,");
            for (index = 0; index < perf.cpuTimes.size(); index++)
            {
                cpuHeader.append(std::to_string(perf.cpuTimes[index]->average));
                cpuHeader.append(",");
            }

            // Generate the header rows for the CPU time categories
            cpuHeader.append("\n");
            cpuHeader.append("FrameIndex,");
            for (index = 0; index < perf.cpuTimes.size(); index++)
            {
                cpuHeader.append(perf.cpuTimes[index]->name.c_str());
                cpuHeader.append(",");
            }

            // Generate the average timing rows for the GPU
            gpuHeader.append("Average,");
            for (index = 0; index < perf.gpuTimes.size(); index++)
            {
                gpuHeader.append(std::to_string(perf.gpuTimes[index]->average));
                gpuHeader.append(",");
            }

            // Generate the header rows for the GPU time categories
            gpuHeader.append("\n");
            gpuHeader.append("FrameIndex,");
            for (index = 0; index < perf.gpuTimes.size(); index++)
            {
                gpuHeader.append(perf.gpuTimes[index]->name);
                gpuHeader.append(",");
            }

            // Write the CPU times to file
            std::ofstream csv;
            csv.open(config.scene.screenshotPath + "/benchmarkCpu.csv", std::ios::out);
            if (csv.is_open())
            {
                csv << cpuHeader << std::endl << benchmarkRun.cpuTimingCsv.str();
            }
            csv.close();

            // Write the GPU times to file
            csv.open(config.scene.screenshotPath + "/benchmarkGpu.csv", std::ios::out);
            if (csv.is_open())
            {
                csv << gpuHeader << std::endl << benchmarkRun.gpuTimingCsv.str();
            }
            csv.close();
            log << "Wrote benchmark results to csv." << std::endl;

            // Print averages to the log file
            log << "Benchmark Timings:" << std::endl;
            for (Instrumentation::Stat* stat : perf.cpuTimes)
            {
                log << "\t" << stat->name << "=" << stat->average << "ms(CPU)" << std::endl;
            }
            for (Instrumentation::Stat* stat : perf.gpuTimes)
            {
                log << "\t" << stat->name << "=" << stat->average << "ms(GPU)" << std::endl;
            }

            config.app.benchmarkRunning = false;
            return true;
        }

        benchmarkRun.numFramesBenched++;
        return false;
    }
}
