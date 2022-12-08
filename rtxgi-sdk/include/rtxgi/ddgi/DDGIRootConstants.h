/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RTXGI_DDGI_ROOT_CONSTANTS_H
#define RTXGI_DDGI_ROOT_CONSTANTS_H

#ifndef HLSL
#include "../Types.h"
using namespace rtxgi;
#endif

struct DDGIRootConstants
{
    uint volumeIndex;
    uint volumeConstantsIndex;
    uint volumeResourceIndicesIndex;
    // Split uint3 into three uints to prevent internal padding
    // while keeping these values at the end of the struct
    uint  reductionInputSizeX;
    uint  reductionInputSizeY;
    uint  reductionInputSizeZ;

#ifndef HLSL
    uint32_t data[6] = {};
    static uint32_t GetNum32BitValues() { return 6; }
    static uint32_t GetSizeInBytes() { return GetNum32BitValues() * 4; }
    static uint32_t GetAlignedNum32BitValues() { return 8; }
    static uint32_t GetAlignedSizeInBytes() { return GetAlignedNum32BitValues() * 4; }
    uint32_t* GetData()
    {
        data[0] = volumeIndex;
        data[1] = volumeConstantsIndex;
        data[2] = volumeResourceIndicesIndex;
        data[3] = reductionInputSizeX;
        data[4] = reductionInputSizeY;
        data[5] = reductionInputSizeZ;
        //data[6/7] = 0; // empty, alignment padding

        return data;
    }
#endif
};

#endif // RTXGI_DDGI_ROOT_CONSTANTS_H
