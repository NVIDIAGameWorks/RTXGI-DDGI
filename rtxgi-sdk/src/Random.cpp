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

#include <time.h>
#include <stdlib.h>
#include <math.h>

#include "rtxgi/Math.h"
#include "rtxgi/Random.h"

namespace rtxgi
{

    void InitRandomSeed()
    {
        srand((unsigned int)time(NULL));
    }

    float GetRandomNumber()
    {
        return (float)rand() / RAND_MAX;
    }

}
