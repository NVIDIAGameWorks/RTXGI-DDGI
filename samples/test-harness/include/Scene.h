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

#include "Common.h"

namespace SceneGraph
{
    // TODO:
    // Put these functions in the Scene namespace. Can call 
    // CreateCornellBox instead of loading a scene.

    void Update(float time, Scene &scene);

    bool CreateCornellBox(D3D12Global &d3d, D3D12Resources &resources);
    bool CreateSphere(D3D12Global &d3d, D3D12Resources &resources);


}
