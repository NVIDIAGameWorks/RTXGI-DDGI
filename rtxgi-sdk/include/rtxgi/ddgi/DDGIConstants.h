/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RTXGI_DDGI_CONSTANTS_H
#define RTXGI_DDGI_CONSTANTS_H

#ifndef HLSL
#include "../Types.h"
using namespace rtxgi;
#endif

struct DDGIConstants
{
    uint  volumeIndex;
    uint  uavOffset;
    uint  srvOffset;

#ifndef HLSL
    uint32_t data[3] = {0, 0, 0};
    static uint32_t GetNum32BitValues() { return 3; }
    static uint32_t GetSizeInBytes() { return 12; }
    static uint32_t GetAlignedNum32BitValues() { return 4; }
    static uint32_t GetAlignedSizeInBytes() { return 16; }
    uint32_t* GetData()
    {
        data[0] = volumeIndex;
        data[1] = uavOffset;
        data[2] = srvOffset;
      //data[3] = 0; // empty, alignment padding

        return data;
    }
#endif
};

#endif // RTXGI_DDGI_CONSTANTS_H
