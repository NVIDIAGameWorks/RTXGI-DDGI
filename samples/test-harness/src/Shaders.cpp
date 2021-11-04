/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "Shaders.h"
#include "graphics/UI.h"

namespace Shaders
{

    /**
     * Initialize the the DirectX Shader Compiler (DXC).
     */
    bool Initialize(const Configs::Config& config, ShaderCompiler& compiler)
    {
        if (FAILED(compiler.dxcDllHelper.Initialize())) return false;
        if (FAILED(compiler.dxcDllHelper.CreateInstance(CLSID_DxcCompiler, &compiler.compiler))) return false;
        if (FAILED(compiler.dxcDllHelper.CreateInstance(CLSID_DxcLibrary, &compiler.library))) return false;

        compiler.root = config.app.root;
        compiler.rtxgi = config.app.rtxgi;

        return true;
    }

    /**
     * Add a define to the shader program with the given name and value.
     */
    void AddDefine(ShaderProgram& shader, std::wstring name, std::wstring value)
    {
        DxcDefine define;

        shader.defineStrs.push_back(new std::wstring(name));
        define.Name = shader.defineStrs.back()->c_str();

        shader.defineStrs.push_back(new std::wstring(value));
        define.Value = shader.defineStrs.back()->c_str();

        shader.defines.push_back(define);
    }

    /**
     * Compile a shader with the DirectX Shader Compiler (DXC).
     */
    bool Compile(ShaderCompiler& compiler, ShaderProgram& shader, bool warningsAsErrors, bool debugInfo)
    {
        uint32_t codePage = 0;
        IDxcBlobEncoding* pShaderText = nullptr;
        IDxcOperationResult* result = nullptr;

        bool retryCompile = true;
        while(retryCompile)
        {
            // Load and encode the shader file
            if (FAILED(compiler.library->CreateBlobFromFile(shader.filepath.c_str(), &codePage, &pShaderText))) return false;

            // Create the compiler include handler
            IDxcIncludeHandler* dxcIncludeHandler = nullptr;
            if (FAILED(compiler.library->CreateIncludeHandler(&dxcIncludeHandler))) return false;

            // Add default shader defines
            AddDefine(shader, L"HLSL", L"1");

            // Treat warnings as errors
            if(warningsAsErrors)
            {
                shader.arguments.push_back(L"-WX");
            }

            // Add with debug information to compiled shaders
            if(debugInfo)
            {
                shader.arguments.push_back(L"-Zi");
                shader.arguments.push_back(L"-Qembed_debug");
            }

            // Add include directories
            std::wstring arg;
            if(!shader.includePath.empty())
            {
                arg.append(L"-I ");
                arg.append(shader.includePath);
                shader.arguments.push_back(arg.c_str());
            }

            // Compile the shader
            if (FAILED(compiler.compiler->Compile(
                pShaderText,
                shader.filepath.c_str(),
                shader.entryPoint.c_str(),
                shader.targetProfile.c_str(),
                shader.arguments.data(),
                static_cast<uint32_t>(shader.arguments.size()),
                shader.defines.data(),
                static_cast<uint32_t>(shader.defines.size()),
                dxcIncludeHandler, &result))) return false;

            // Verify the result
            HRESULT hr = S_OK;
            result->GetStatus(&hr);
            if (FAILED(hr))
            {
                IDxcBlobEncoding* error = nullptr;
                if (FAILED(result->GetErrorBuffer(&error))) return false;

                if (error->GetBufferSize() > 0)
                {
                    // Convert error blob to a std::string
                    std::vector<char> infoLog(error->GetBufferSize() + 1);
                    memcpy(infoLog.data(), error->GetBufferPointer(), error->GetBufferSize());
                    infoLog[error->GetBufferSize()] = 0;

                    std::string errorMsg = "Shader Compiler Error:\n";
                    errorMsg.append(infoLog.data());

                    // Spawn a pop-up that displays the compilation errors and retry dialog
                    if(Graphics::UI::MessageRetryBox(errorMsg.c_str()))
                    {
                        continue; // Try to compile again
                    }
                    return false;
                }
            }

            // Shader compiled successfully
            retryCompile = false;
        }

        // Get the shader bytecode
        if (FAILED(result->GetResult((IDxcBlob**)&shader.bytecode))) return false;
        return true;
    }

    /**
     * Release memory used by the shader compiler.
     */
    void Cleanup(ShaderCompiler& compiler)
    {
        SAFE_RELEASE(compiler.library);
        SAFE_RELEASE(compiler.compiler);
        compiler.root = "";
        compiler.rtxgi = "";
    }

}
