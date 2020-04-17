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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN    // Exclude rarely-used items from Windows headers.
#endif

#include <Windows.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <dxcapi.h>
#include <dxcapi.use.h>
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <cmath>

#include <rtxgi/Common.h>
#include <rtxgi/Defines.h>
#include <rtxgi/ddgi/DDGIVolumeDefines.h>

#define LIGHTS_CPU
#include "Lights.h"

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

using namespace std;
using namespace DirectX;

// Sets the number of probe relocation iterations to use during probe relocation
#if RTXGI_DDGI_PROBE_RELOCATION
const static int RTXGI_DDGI_MAX_PROBE_RELOCATION_ITERATIONS = 50;
#endif

#define AO_FILTER_BLOCK_SIZE 8 // Block is NxN. Tuned for perf. 32 maximum.

static const D3D12_HEAP_PROPERTIES defaultHeapProperties =
{
    D3D12_HEAP_TYPE_DEFAULT,
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN,
    0, 0
};

enum class ERenderMode
{
    PathTrace = 0,
    DDGI,
    Count
};

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;

    Vertex& operator=(const Vertex &v)
    {
        position = v.position;
        normal = v.normal;
        return *this;
    }
};

struct Material
{
    string   name = "defaultMaterial";
    XMFLOAT3 color;
};

struct RuntimeMesh
{
    string name;
    UINT numVertices = 0;
    UINT numIndices = 0;
    UINT materialIndex = 0;

    Vertex* vertices = nullptr;
    UINT* indices = nullptr;

    void Release()
    {
        RTXGI_SAFE_DELETE_ARRAY(vertices);
        RTXGI_SAFE_DELETE_ARRAY(indices);
    }
};

struct RuntimeGeometry
{
    vector<Material> materials;
    vector<RuntimeMesh> meshes;

    void Release()
    {
        for (UINT meshIndex = 0; meshIndex < meshes.size(); meshIndex++)
        {
            meshes[meshIndex].Release();
        }
    }
};

struct RuntimeTexture
{
    DXGI_FORMAT     format;
    ID3D12Resource* texture;
    ID3D12Resource* uploadBuffer;
};

struct D3D12Info
{
    IDXGIFactory4*                     factory = nullptr;
    ID3D12Device5*                     device = nullptr;
    ID3D12CommandQueue*                cmdQueue = nullptr;
    ID3D12CommandAllocator*            cmdAlloc[2] = { nullptr, nullptr };
    ID3D12GraphicsCommandList4*        cmdList = nullptr;

    IDXGISwapChain3*                   swapChain = nullptr;
    ID3D12Resource*                    backBuffer[2] = { nullptr, nullptr };

    ID3D12Fence*                       fence = nullptr;
    UINT64                             fenceValues[2] = { 0, 0 };
    HANDLE                             fenceEvent;
    UINT                               frameIndex = 0;

    D3D12_VIEWPORT                     viewport;
    D3D12_RECT                         scissor;

    bool                               vsync = true;

    int                                width = 0;
    int                                height = 0;
    UINT                               frameNumber = 0;
};

struct D3D12Resources
{
    ID3D12Resource*                    RTGBufferA = nullptr;        // RGB: albedo, A: primary hit flag
    ID3D12Resource*                    RTGBufferB = nullptr;        // XYZ: World Position, W: Hit Distance
    ID3D12Resource*                    RTGBufferC = nullptr;        // XYZ: Normal, W: unused
    ID3D12Resource*                    RTGBufferD = nullptr;        // RGB: direct lit diffuse, A: unused

    ID3D12Resource*                    RTAORaw = nullptr;
    ID3D12Resource*                    RTAOFiltered = nullptr;

    ID3D12Resource*                    PTOutput = nullptr;
    ID3D12Resource*                    PTAccumulation = nullptr;

    ID3D12DescriptorHeap*              rtvHeap = nullptr;
    ID3D12DescriptorHeap*              cbvSrvUavHeap = nullptr;
    ID3D12DescriptorHeap*              samplerHeap = nullptr;

    ID3D12RootSignature*               computeRootSig = nullptr;
    ID3D12PipelineState*               AOFilterPSO = nullptr;

    ID3D12RootSignature*               rasterRootSig = nullptr;
    ID3D12PipelineState*               indirectPSO = nullptr;
    ID3D12PipelineState*               visBuffersPSO = nullptr;

    ID3D12Resource*                    cameraCB = nullptr;
    UINT8*                             cameraCBStart = nullptr;

    ID3D12Resource*                    materialCB = nullptr;
    UINT8*                             materialCBStart = nullptr;

    ID3D12Resource*                    lightsCB = nullptr;
    UINT8*                             lightsCBStart = nullptr;

    ID3D12Resource*                    volumeCB = nullptr;

    UINT                               rtvDescSize = 0;
    UINT                               cbvSrvUavDescSize = 0;

    RuntimeGeometry                    geometry = {};
    bool                               isGeometryProcedural = false;

    int                                blueNoiseRGBTextureIndex = -1;

    vector<ID3D12Resource*>            vertexBuffers;
    vector<D3D12_VERTEX_BUFFER_VIEW>   vertexBufferViews;
    vector < ID3D12Resource*>          indexBuffers;
    vector < D3D12_INDEX_BUFFER_VIEW>  indexBufferViews;
    vector<RuntimeTexture>             textures;

    ID3D12Resource*                    sphereVertexBuffer = nullptr;
    D3D12_VERTEX_BUFFER_VIEW           sphereVertexBufferView;
    ID3D12Resource*                    sphereIndexBuffer = nullptr;
    D3D12_INDEX_BUFFER_VIEW            sphereIndexBufferView;
};

struct D3D12ShaderCompiler
{
    dxc::DxcDllSupport DxcDllHelper;
    IDxcCompiler*      compiler = nullptr;
    IDxcLibrary*       library = nullptr;
    string             root = "";
    string             rtxgi = "";
};

struct D3D12ShaderInfo
{
    LPCWSTR filename = L"";
    LPCWSTR targetProfile = L"";
    LPCWSTR entryPoint = L"";

    int numDefines = 0;
    DxcDefine* defines = nullptr;
    ID3DBlob* bytecode = nullptr;

    D3D12ShaderInfo() {};
    D3D12ShaderInfo(LPCWSTR inFilename, LPCWSTR inEntryPoint, LPCWSTR inProfile)
    {
        filename = inFilename;
        entryPoint = inEntryPoint;
        targetProfile = inProfile;
    }
};

struct D3D12BufferCreateInfo
{
    UINT64 size = 0;
    UINT64 alignment = 0;
    D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;

    D3D12BufferCreateInfo() {}
    D3D12BufferCreateInfo(UINT64 InSize, UINT64 InAlignment, D3D12_HEAP_TYPE InHeapType, D3D12_RESOURCE_FLAGS InFlags, D3D12_RESOURCE_STATES InState) :
        size(InSize), 
        alignment(InAlignment),
        heapType(InHeapType),
        flags(InFlags),
        state(InState) {}

    D3D12BufferCreateInfo(UINT64 InSize, D3D12_RESOURCE_FLAGS InFlags, D3D12_RESOURCE_STATES InState) :
        size(InSize),
        flags(InFlags),
        state(InState) {}

    D3D12BufferCreateInfo(UINT64 InSize, D3D12_HEAP_TYPE InHeapType, D3D12_RESOURCE_STATES InState) :
        size(InSize),
        heapType(InHeapType),
        state(InState) {}

    D3D12BufferCreateInfo(UINT64 InSize, D3D12_RESOURCE_FLAGS InFlags) :
        size(InSize),
        flags(InFlags){}
};

struct AccelerationStructureBuffer
{
    ID3D12Resource* pScratch = nullptr;
    ID3D12Resource* pResult = nullptr;
    ID3D12Resource* pInstanceDesc = nullptr;    // only used in TLAS

    void Release()
    {
        RTXGI_SAFE_RELEASE(pScratch);
        RTXGI_SAFE_RELEASE(pResult);
        RTXGI_SAFE_RELEASE(pInstanceDesc);
    }
};

struct RtProgram
{
    D3D12ShaderInfo         info = {};
    ID3D12RootSignature*    pRootSignature = nullptr;
    D3D12_DXIL_LIBRARY_DESC dxilLibDesc;
    D3D12_EXPORT_DESC       exportDesc;
    D3D12_STATE_SUBOBJECT   subobject;
    wstring                 exportName;

    RtProgram() {}
    RtProgram(D3D12ShaderInfo shaderInfo)
    {
        info = shaderInfo;
        info.bytecode = nullptr;
        subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
        exportName = shaderInfo.entryPoint;
        exportDesc.ExportToRename = nullptr;
        exportDesc.Flags = D3D12_EXPORT_FLAG_NONE;
        pRootSignature = nullptr;
    }

    void SetBytecode()
    {
        exportDesc.Name = exportName.c_str();

        dxilLibDesc.NumExports = 1;
        dxilLibDesc.pExports = &exportDesc;
        dxilLibDesc.DXILLibrary.BytecodeLength = info.bytecode->GetBufferSize();
        dxilLibDesc.DXILLibrary.pShaderBytecode = info.bytecode->GetBufferPointer();

        subobject.pDesc = &dxilLibDesc;
    }

    void Release()
    {
        RTXGI_SAFE_RELEASE(info.bytecode);
        RTXGI_SAFE_RELEASE(pRootSignature);
    }

};

struct HitProgram
{
    RtProgram ahs;
    RtProgram chs;

    std::wstring exportName = L"";
    D3D12_HIT_GROUP_DESC desc = {};
    D3D12_STATE_SUBOBJECT subobject = {};

    HitProgram() {}
    HitProgram(LPCWSTR name)
    {
        exportName = name;
        desc.HitGroupExport = name;
        subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
        subobject.pDesc = &desc;
    }

    void SetExports(bool anyHit)
    {
        desc.HitGroupExport = exportName.c_str();
        if (anyHit) desc.AnyHitShaderImport = ahs.exportDesc.Name;
        desc.ClosestHitShaderImport = chs.exportDesc.Name;
    }

    void Release()
    {
        ahs.Release();
        chs.Release();
    }
};

struct DXRInfo
{
    AccelerationStructureBuffer     TLAS;
    AccelerationStructureBuffer     visTLAS;
    AccelerationStructureBuffer     BLAS;
    AccelerationStructureBuffer     probeBLAS;

    uint64_t                        tlasSize = 0;
    uint64_t                        visTlasSize = 0;

    ID3D12Resource*                 shaderTable = nullptr;
    uint32_t                        shaderTableRecordSize = 0;

    RtProgram                       probeRGS;
    RtProgram                       primaryRGS;
    RtProgram                       ambientOcclusionRGS;
    RtProgram                       probeVisRGS;
    RtProgram                       pathTraceRGS;
    RtProgram                       miss;
    HitProgram                      hit;

    ID3DBlob*                       visUpdateTLASCS = nullptr;
    ID3D12PipelineState*            visUpdateTLASPSO = nullptr;

    ID3D12RootSignature*            globalRootSig = nullptr;
    ID3D12StateObject*              rtpso = nullptr;
    ID3D12StateObjectProperties*    rtpsoInfo = nullptr;
};

struct CameraInfo
{
    XMFLOAT3    origin;
    float       aspect;
    XMFLOAT3    up;
    float       tanHalfFovY;
    XMFLOAT3    right;
    float       fov;
    XMFLOAT3    forward;
    float       pad1 = 0.f;

    CameraInfo()
    {
        aspect = 16.f / 9.f;
        fov = 45.f;
        tanHalfFovY = std::tanf(fov * (XM_PI / 180.f) * 0.5f);

#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT
        up = { 0.f, 1.f, 0.f };
        right = { 1.f, 0.f, 0.f };
        forward = { 0.f, 0.f, 1.f };
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
        up = { 0.f, 1.f, 0.f };
        right = { 1.f, 0.f, 0.f };
        forward = { 0.f, 0.f, -1.f };
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_UNREAL
        up = { 0.f, 0.f, 1.f };
        right = { 0.f, 1.f, 0.f };
        forward = { 1.f, 0.f, 0.f };
#endif
    }
};

struct ConfigInfo
{
    int width = 1280;
    int height = 720;
    bool vsync = true;
    bool ui = true;
    string filepath = "";
    string root = "";
    string rtxgi = "";
    string scene = "";
    ERenderMode mode = ERenderMode::DDGI;
};

struct InputOptions
{
    bool    invertPan = true;
    float   movementSpeed = 6.f;
    float   rotationSpeed = 3.f;
#if RTXGI_DDGI_PROBE_RELOCATION
    bool    runProbeRelocation = false;
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
    bool    activateAllProbes = false;
    bool    enableProbeClassification = false;
#endif
};

struct VizOptions
{
    float probeRadius = 1.f;
    float irradianceScale = 2.f;
    float distanceScale = 1.f;
    float radianceScale = 1.f;
    float offsetScale = 10.f;
    float stateScale = 10.f;
    float distanceDivisor = 1.f;
    bool  showDDGIVolumeBuffers = false;
    bool  showDDGIVolumeProbes = false;
};

struct RTOptions
{
    float normalBias = 0.f;
    float viewBias = 0.f;
    UINT  numBounces = 1;
};

struct PostProcessOptions
{
    bool  useDDGI = true;
    bool  useRTAO = true;
    bool  viewAO = false;
    float AORadius = 1.f;
    float AOPowerLog = -1;
    float AOBias = 0.0001f;
    float AOFilterDistanceSigma = 10.f;
    float AOFilterDepthSigma = 0.25f;
    float exposureFStops = 1.f;
};

struct LightInfo
{
    UINT  lightMask = 0;
    uint3 lightCounts = { 0, 0, 0 };
    DirectionalLightDescGPU directionalLight;
    PointLightDescGPU pointLight;
    SpotLightDescGPU spotLight;
};

namespace D3D12
{
    bool Initialize(D3D12Info &d3d, D3D12Resources &resources, D3D12ShaderCompiler &shaderCompiler, HWND &window);
    ID3D12RootSignature* CreateRootSignature(D3D12Info &d3d, const D3D12_ROOT_SIGNATURE_DESC &desc);
    bool CreateComputePSO(ID3D12Device* device, ID3D12RootSignature* rootSignature, ID3DBlob* shader, ID3D12PipelineState** pipeline);
    bool CreateBuffer(D3D12Info &d3d, D3D12BufferCreateInfo &info, ID3D12Resource** ppResource);
    bool CreateTexture(UINT64 width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_STATES state, ID3D12Resource** resource, ID3D12Device* device);
    bool CreateDevice(ID3D12Device5* &device);
    bool CreateDevice(D3D12Info &d3d);
    bool ResetCmdList(D3D12Info &d3d);
    void SubmitCmdList(D3D12Info &d3d);
    void Present(D3D12Info &d3d);
    bool WaitForGPU(D3D12Info &d3d);
    bool MoveToNextFrame(D3D12Info &d3d);   
    void Cleanup(D3D12Info &d3d, D3D12Resources &resources);
};

namespace DXR
{
    bool Initialize(D3D12Info &d3d, DXRInfo &dxr, D3D12Resources &resources, D3D12ShaderCompiler &shaderCompiler);
    bool CreateVisTLAS(D3D12Info &d3d, DXRInfo &dxr, D3D12Resources &resources, int numProbes);
    bool UpdateVisTLAS(D3D12Info &d3d, DXRInfo &dxr, D3D12Resources &resources, int numProbes, float probeScale);
    void Cleanup(DXRInfo &dxr, D3D12Resources &resources);
};
