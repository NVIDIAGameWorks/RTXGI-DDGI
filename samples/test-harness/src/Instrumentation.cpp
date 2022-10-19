/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "Instrumentation.h"

#if __linux__
#include <time.h>
#endif

namespace Instrumentation
{
    //----------------------------------------------------------------------------------------------------------
    // Private Functions
    //----------------------------------------------------------------------------------------------------------

    int64_t GetPerfCounter()
    {
    #if defined(_WIN32) || defined(WIN32)
        LARGE_INTEGER value;
        QueryPerformanceCounter(&value);
        return value.QuadPart;
    #elif __linux__
        timespec ts;
        int result = -1;
        while(result < 0)
        {
            result = clock_gettime(CLOCK_REALTIME, &ts);
        }
        return (ts.tv_sec * 1.0e9) + ts.tv_nsec;
    #endif
    }

#if defined(_WIN32) || defined(WIN32)
    int64_t GetPerfCounterFrequency()
    {
        LARGE_INTEGER value;
        QueryPerformanceFrequency(&value);
        return value.QuadPart;
    }

    static int64_t frequency = GetPerfCounterFrequency();
#endif

    /**
     * Get the number of milliseconds between the timer's start and the current time.
     */
    void GetElapsed(Stat* s)
    {
    #if defined(_WIN32) || defined(WIN32)
        // Frequency is ticks per second, so elapsed ticks / frequency = seconds
        uint64_t seconds = (GetPerfCounter() - s->timestamp);
        s->elapsed += (static_cast<double>(seconds) / static_cast<double>(frequency)) * 1000;
    #elif __linux__
        // GetPerfCounter times are in nanoseconds on Linux
        s->elapsed += (GetPerfCounter() - s->timestamp) * 0.000001;
    #endif
    }

    //----------------------------------------------------------------------------------------------------------
    // Public Functions
    //----------------------------------------------------------------------------------------------------------

    uint32_t Stat::frameGPUQueryCount = 0;

    int32_t Stat::GetGPUQueryBeginIndex()
    {
        gpuQueryStartIndex = (Stat::frameGPUQueryCount * 2);
        Stat::frameGPUQueryCount++;
        return gpuQueryStartIndex;
    }

    int32_t Stat::GetGPUQueryEndIndex()
    {
        gpuQueryEndIndex = gpuQueryStartIndex + 1;
        return gpuQueryEndIndex;
    }

    void Stat::ResetGPUQueryIndices()
    {
        gpuQueryStartIndex = gpuQueryEndIndex = -1;
    }

    void Begin(Stat* s)
    {
        s->elapsed = 0;
        s->timestamp = GetPerfCounter();
    }

    void End(Stat* s)
    {
        GetElapsed(s);
    }

    void Resolve(Stat* s)
    {
        s->total += s->elapsed;
        if (static_cast<uint32_t>(s->samples.size()) >= s->sampleSize)
        {
            s->total -= s->samples.front();
            s->samples.pop();
        }
        s->samples.push(s->elapsed);
        s->average = std::max(s->total / s->samples.size(), (double)0);
    }

    void EndAndResolve(Stat* s)
    {
        End(s);
        Resolve(s);
    }

    std::ostream& operator<<(std::ostream& os, Stat* stat)
    {
        if(stat) os << stat->elapsed;
        return os;
    }

    std::ostream& operator<<(std::ostream& os, std::vector<Stat*>& stats)
    {
        for (size_t index = 0; index < stats.size(); index++)
        {
            os << stats[index] << ",";
        }
        os << std::endl;
        return os;
    }

}
