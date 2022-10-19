/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "Shaders.h"
#include "graphics/UI.h"

#if __linux__
#include <filesystem>
#endif

namespace Shaders
{
    //----------------------------------------------------------------------------------------------------------
    // Private Functions
    //----------------------------------------------------------------------------------------------------------

    void UnloadDirectXCompiler(ShaderCompiler& dxc)
    {
    #if _WIN32
        FreeLibrary(dxc.dll);
    #elif __linux__
        ::dlclose(dxc.dll);
    #endif
        dxc.DxcCreateInstance = nullptr;
        dxc.dll = nullptr;
    }

    HRESULT LoadDirectXCompiler(ShaderCompiler& dxc)
    {
        if(dxc.dll != nullptr) return S_OK;

    #if _WIN32
        dxc.dll = LoadLibrary("dxcompiler.dll");
    #elif __linux__
        const std::filesystem::path subdir("bin/vulkan/libdxcompiler.so");
        std::filesystem::path path = std::filesystem::current_path();
        path = path.parent_path();
        path /= subdir;
        dxc.dll = dlopen(path.c_str(), RTLD_LAZY);
    #endif

        // DLL was not loaded, error out
        if (dxc.dll == nullptr) return HRESULT_FROM_WIN32(GetLastError());

        // Load the function
    #if _WIN32
        dxc.DxcCreateInstance = (DxcCreateInstanceProc)GetProcAddress(dxc.dll, "DxcCreateInstance");
    #elif __linux__
        dxc.DxcCreateInstance = (DxcCreateInstanceProc)::dlsym(dxc.dll, "DxcCreateInstance");
    #endif

        // DLL function not loaded
        if(dxc.DxcCreateInstance == nullptr)
        {
            UnloadDirectXCompiler(dxc);
            return HRESULT_FROM_WIN32(GetLastError());
        }

        return S_OK;
    }

    //----------------------------------------------------------------------------------------------------------
    // Public Functions
    //----------------------------------------------------------------------------------------------------------

    /**
     * Initialize the the DirectX Shader Compiler (DXC).
     */
    bool Initialize(const Configs::Config& config, ShaderCompiler& dxc)
    {
        // Load the DXC DLL
        if(FAILED(LoadDirectXCompiler(dxc))) return false;

        // Create the utils instance
        if(FAILED(dxc.DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxc.utils)))) return false;

        // Create the compiler instance
        if(FAILED(dxc.DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxc.compiler)))) return false;

        // Create the default include handler
        if(FAILED(dxc.utils->CreateDefaultIncludeHandler(&dxc.includes))) return false;

        dxc.root = config.app.root;
        dxc.rtxgi = config.app.rtxgi;

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
    bool Compile(ShaderCompiler& dxc, ShaderProgram& shader, bool warningsAsErrors, bool debugInfo)
    {
        uint32_t codePage = 0;
        IDxcBlobEncoding* pShaderSource = nullptr;
        IDxcResult* result = nullptr;

        bool retry = true;
        while(retry)
        {
            // Load and encode the shader file
            if (FAILED(dxc.utils->LoadFile(shader.filepath.c_str(), nullptr, &pShaderSource))) return false;

            DxcBuffer source;
            source.Ptr = pShaderSource->GetBufferPointer();
            source.Size = pShaderSource->GetBufferSize();
            source.Encoding = DXC_CP_ACP;

            // Add default shader defines
            AddDefine(shader, L"HLSL", L"1");

            // Treat warnings as errors
            if(warningsAsErrors) shader.arguments.push_back(L"-WX");

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

            // Build the arguments array
            IDxcCompilerArgs* args = nullptr;
            dxc.utils->BuildArguments(
                shader.filepath.c_str(),
                shader.entryPoint.c_str(),
                shader.targetProfile.c_str(),
                shader.arguments.data(),
                static_cast<UINT>(shader.arguments.size()),
                shader.defines.data(),
                static_cast<UINT>(shader.defines.size()),
                &args);

            // Compile the shader
            if (FAILED(dxc.compiler->Compile(&source, args->GetArguments(), args->GetCount(), dxc.includes, IID_PPV_ARGS(&result)))) return false;

            // Get the errors (if there are any)
            IDxcBlobUtf8* errors = nullptr;
            if(FAILED(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr))) return false;

            // Display errors and allow recompilation
            if(errors != nullptr && errors->GetStringLength() != 0)
            {
                // Convert error blob to a std::string
                std::vector<char> log(errors->GetStringLength() + 1);
                memcpy(log.data(), errors->GetStringPointer(), errors->GetStringLength());

                std::string errorMsg = "Shader Compiler Error:\n";
                errorMsg.append(log.data());

                // Spawn a pop-up that displays the compilation errors and retry dialog
                if (Graphics::UI::MessageRetryBox(errorMsg.c_str()))
                {
                    continue; // Try to compile again
                }

                return false;
            }

            // Shader compiled successfully
            retry = false;

            // Get the shader bytecode
            if(FAILED(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader.bytecode), &shader.shaderName))) return false;
        }
        return true;
    }

    /**
     * Release memory used by the shader compiler.
     */
    void Cleanup(ShaderCompiler& dxc)
    {
        SAFE_RELEASE(dxc.utils);
        SAFE_RELEASE(dxc.compiler);
        SAFE_RELEASE(dxc.includes);
        UnloadDirectXCompiler(dxc);
        dxc.root = "";
        dxc.rtxgi = "";
    }

}
