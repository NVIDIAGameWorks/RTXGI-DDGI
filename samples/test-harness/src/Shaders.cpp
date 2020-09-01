/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "Shaders.h"

namespace Shaders
{

/**
* Initialize the the DirectX Shader Compiler (DXC).
*/
bool InitCompiler(ShaderCompiler &compiler)
{
    HRESULT hr = compiler.dxcDllHelper.Initialize();
    if (FAILED(hr))return false;

    hr = compiler.dxcDllHelper.CreateInstance(CLSID_DxcCompiler, &compiler.compiler);
    if (FAILED(hr))return false;

    hr = compiler.dxcDllHelper.CreateInstance(CLSID_DxcLibrary, &compiler.library);
    if (FAILED(hr))return false;

    return true;
}

/**
* Compile a shader with the DirectX Shader Compiler (DXC).
*/
bool Compile(ShaderCompiler &compiler, ShaderProgram &shader, bool warningsAsErrors)
{
    UINT codePage = 0;
    IDxcBlobEncoding* pShaderText = nullptr;

    // Load and encode the shader file
    HRESULT hr = compiler.library->CreateBlobFromFile(shader.filepath, &codePage, &pShaderText);
    if (FAILED(hr)) return false;

    // Create the compiler include handler
    IDxcIncludeHandler* dxcIncludeHandler = nullptr;
    hr = compiler.library->CreateIncludeHandler(&dxcIncludeHandler);
    if (FAILED(hr)) return false;

    // Compile the shader
    IDxcOperationResult* result = nullptr;
    hr = compiler.compiler->Compile(pShaderText, shader.filepath, shader.entryPoint, shader.targetProfile, nullptr, 0, shader.defines, shader.numDefines, dxcIncludeHandler, &result);
    if (FAILED(hr)) return false;

    // Verify the result
    result->GetStatus(&hr);
    if (FAILED(hr) || warningsAsErrors)
    {
        IDxcBlobEncoding* error = nullptr;
        hr = result->GetErrorBuffer(&error);
        if (FAILED(hr)) return false;

        if (error->GetBufferSize() > 0)
        {
            // Convert error blob to a std::string
            std::vector<char> infoLog(error->GetBufferSize() + 1);
            memcpy(infoLog.data(), error->GetBufferPointer(), error->GetBufferSize());
            infoLog[error->GetBufferSize()] = 0;

            std::string errorMsg = "Shader Compiler Error:\n";
            errorMsg.append(infoLog.data());

            MessageBox(nullptr, errorMsg.c_str(), "Error!", MB_OK);
            return false;
        }
    }

    // Get the shader bytecode
    hr = result->GetResult((IDxcBlob**)&shader.bytecode);
    if (FAILED(hr)) return false;
    return true;
}

/**
* Release memory used by the shader compiler.
*/
void Cleanup(ShaderCompiler &compiler)
{
    compiler.library->Release();
    compiler.compiler->Release();
}

}
