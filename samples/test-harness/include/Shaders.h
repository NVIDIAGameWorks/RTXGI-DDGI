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

namespace Shaders
{
    bool InitCompiler(D3D12ShaderCompiler &shaderCompiler);
    bool Compile(D3D12ShaderCompiler &compilerInfo, D3D12ShaderInfo &info, bool warningsAsErrors = false);
    bool Compile(D3D12ShaderCompiler &compilerInfo, RtProgram &program, bool warningsAsErrors = false);
    void Cleanup(D3D12ShaderCompiler &shaderCompiler);
}
