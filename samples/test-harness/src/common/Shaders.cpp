/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN         // Exclude rarely-used items from Windows headers.
#endif

#include <Windows.h>
#include <atlcomcli.h>
#include <dxcapi.h>
#include <string>
#include <vector>

#include "Common.h"

namespace Shaders
{

/**
* Initialize the DXC shader compiler.
*/
bool InitCompiler(D3D12ShaderCompiler &shaderCompiler)
{
    HRESULT hr = shaderCompiler.DxcDllHelper.Initialize();
    if (FAILED(hr))return false;

    hr = shaderCompiler.DxcDllHelper.CreateInstance(CLSID_DxcCompiler, &shaderCompiler.compiler);
    if (FAILED(hr))return false;

    hr = shaderCompiler.DxcDllHelper.CreateInstance(CLSID_DxcLibrary, &shaderCompiler.library);
    if (FAILED(hr))return false;

    return true;
}

/**
* Compile a shader with DXC.
*/
bool Compile(D3D12ShaderCompiler &compilerInfo, D3D12ShaderInfo &shader, bool warningsAsErrors)
{
    UINT32 codePage(0);
    IDxcBlobEncoding* pShaderText(nullptr);

    // Load and encode the shader file
    HRESULT hr = compilerInfo.library->CreateBlobFromFile(shader.filename, &codePage, &pShaderText);
    if (FAILED(hr)) return false;

    // Create the compiler include handler
    CComPtr<IDxcIncludeHandler> dxcIncludeHandler;
    hr = compilerInfo.library->CreateIncludeHandler(&dxcIncludeHandler);
    if (FAILED(hr)) return false;

    // Compile the shader
    IDxcOperationResult* result;
    hr = compilerInfo.compiler->Compile(pShaderText, shader.filename, shader.entryPoint, shader.targetProfile, nullptr, 0, shader.defines, shader.numDefines, dxcIncludeHandler, &result);
    if (FAILED(hr)) return false;

    // Verify the result
    result->GetStatus(&hr);
    if (FAILED(hr) || warningsAsErrors)
    {
        IDxcBlobEncoding* error;
        hr = result->GetErrorBuffer(&error);
        if (FAILED(hr)) return false;

        if (error->GetBufferSize() > 0)
        {
            // Convert error blob to a string
            std::vector<char> infoLog(error->GetBufferSize() + 1);
            memcpy(infoLog.data(), error->GetBufferPointer(), error->GetBufferSize());
            infoLog[error->GetBufferSize()] = 0;

            std::string errorMsg = "Shader Compiler Error:\n";
            errorMsg.append(infoLog.data());

            MessageBoxA(nullptr, errorMsg.c_str(), "Error!", MB_OK);
            return false;
        }
    }

    hr = result->GetResult((IDxcBlob**)&shader.bytecode);
    if (FAILED(hr)) return false;

    return true;
}

/**
* Compile a D3D HLSL shader using dxcompiler.
*/
bool Compile(D3D12ShaderCompiler &compilerInfo, RtProgram &program, bool warningsAsErrors)
{
    bool result = Compile(compilerInfo, program.info, warningsAsErrors);
    program.SetBytecode();
    return result;
}

/**
* Release memory used by the shader compiler.
*/
void Cleanup(D3D12ShaderCompiler &shaderCompiler)
{
    shaderCompiler.library->Release();
    shaderCompiler.compiler->Release();
}

}
