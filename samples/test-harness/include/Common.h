/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

#define AO_FILTER_BLOCK_SIZE 8 // Block is NxN. Tuned for perf. 32 maximum.
#define NUM_MAX_VOLUMES 4

//-----------------------------------------------------------------------------
// Application Options
//-----------------------------------------------------------------------------

enum class ERenderMode
{
    PathTrace = 0,
    DDGI,
    Count
};

struct InputOptions
{
    bool    invertPan = true;
    float   movementSpeed = 6.f;
    float   rotationSpeed = 3.f;
#if RTXGI_DDGI_PROBE_RELOCATION
    bool    enableProbeRelocation = false;
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
    bool    activateAllProbes = false;
    bool    enableProbeClassification = false;
#endif
    UINT    volumeSelect = 0;
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
    float skyIntensity = 0.f;
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
    bool  useTonemapping = true;
    bool  useDithering = true;
    bool  useExposure = true;
    float exposureFStops = 1.f;
};

//-----------------------------------------------------------------------------
// Global Structures
//-----------------------------------------------------------------------------

struct LightInfo
{
    UINT  lightMask = 0;
    uint3 lightCounts = { 0, 0, 0 };
    DirectionalLightDescGPU directionalLight;
    PointLightDescGPU pointLight;
    SpotLightDescGPU spotLight;
};

struct ConfigInfo
{
    int width = 1280;
    int height = 720;
    bool vsync = true;
    bool ui = true;
    std::string filepath = "";
    std::string root = "";
    std::string rtxgi = "";
    std::string scenePath = "";
    std::string sceneFile = "";
    std::string screenshotFile = "";
    ERenderMode mode = ERenderMode::DDGI;
};

struct Camera
{
    DirectX::XMFLOAT3    position;
    float                aspect = 16.f / 9.f;
    DirectX::XMFLOAT3    up;
    float                fov = 45.f;
    DirectX::XMFLOAT3    right;
    float                tanHalfFovY = std::tanf(fov * (DirectX::XM_PI / 180.f) * 0.5f);
    DirectX::XMFLOAT3    forward;
    int                  numPaths = 1;  // path tracer only

    Camera()
    {
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT
        up = { 0.f, 1.f, 0.f };
        right = { 1.f, 0.f, 0.f };
        forward = { 0.f, 0.f, 1.f };
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
        up = { 0.f, 0.f, 1.f };
        right = { 0.f, 1.f, 0.f };
        forward = { 1.f, 0.f, 0.f };
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
        up = { 0.f, 1.f, 0.f };
        right = { 1.f, 0.f, 0.f };
        forward = { 0.f, 0.f, -1.f };
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
        up = { 0.f, 0.f, 1.f };
        right = { 1.f, 0.f, 0.f };
        forward = { 0.f, 1.f, 0.f };
#endif
    }
};

// ---[ Geometry ] ------------------------------------------------------------

struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT4 tangent;
    DirectX::XMFLOAT2 uv0;

    Vertex& operator=(const Vertex &v)
    {
        position = v.position;
        normal = v.normal;
        tangent = v.tangent;
        uv0 = v.uv0;
        return *this;
    }
};

struct MeshPrimitive
{
    int                  index = -1;
    int                  material = -1;
    bool                 opaque = true;
    AABB                 boundingBox;
    std::vector<Vertex>  vertices;
    std::vector<UINT>    indices;
};

struct Mesh
{
    std::string name = "";
    std::vector<MeshPrimitive> primitives;
};

// ---[ Materials ] -----------------------------------------------------------

struct GPUMaterial
{
    DirectX::XMFLOAT3 albedo = DirectX::XMFLOAT3(1.f, 1.f, 1.f);
    float   opacity = 1.f;
    DirectX::XMFLOAT3 emissiveColor = DirectX::XMFLOAT3(0.f, 0.f, 0.f);
    float   roughness = 1.f;
    float   metallic = 0.f;
    int     alphaMode = 0;                  // 0: Opaque, 1: Blend, 2: Masked
    float   alphaCutoff = 0.f;
    int     doubleSided = 0;                // 0: false, 1: true
    int     albedoTexIdx = -1;              // RGBA [0-1]
    int     roughnessMetallicTexIdx = -1;   // Occlusion in R (not supported), Roughness in G, Metallic in B
    int     normalTexIdx = -1;              // Tangent space XYZ
    int     emissiveTexIdx = -1;            // Emissive RGB
};

struct Material
{
    std::string name = "";
    GPUMaterial data;
};

struct Texture
{
    std::string name = "";
    std::string filepath = "";

    UINT8* pixels = nullptr;
    int width = 0;
    int height = 0;
    int stride = 0;
};

// ---[ Scene Graph ] ---------------------------------------------------------

struct Instance
{
    int mesh = -1;
    std::string name = "";
    float transform[3][4] =
    {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f
    };
};

struct Node
{
    int instance = -1;
    int camera = -1;
    std::vector<int> children;
    DirectX::XMFLOAT3 translation = DirectX::XMFLOAT3(0.f, 0.f, 0.f);
    DirectX::XMFLOAT4 rotation = DirectX::XMFLOAT4(0.f, 0.f, 0.f, 1.f);
    DirectX::XMFLOAT3 scale = DirectX::XMFLOAT3(1.f, 1.f, 1.f);
};

struct Scene
{
    UINT numGeometries = 0;
    std::vector<int> roots;
    std::vector<Node> nodes;
    std::vector<Camera> cameras;
    std::vector<Instance> instances;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Texture> textures;
};

// ---[ Shaders ] -------------------------------------------------------------

struct ShaderCompiler
{
    dxc::DxcDllSupport dxcDllHelper;
    IDxcCompiler*      compiler = nullptr;
    IDxcLibrary*       library = nullptr;
    std::string        root = "";
    std::string        rtxgi = "";
};

struct ShaderProgram
{
    LPCWSTR filepath = L"";
    LPCWSTR targetProfile = L"lib_6_3";
    LPCWSTR entryPoint = L"";
    LPCWSTR exportName = L"";

    int                   numDefines = 0;
    DxcDefine*            defines = nullptr;
    ID3DBlob*             bytecode = nullptr;
    ID3D12RootSignature*  pRootSignature = nullptr;

    void Release()
    {
        RTXGI_SAFE_DELETE_ARRAY(defines);
        RTXGI_SAFE_RELEASE(bytecode);
        RTXGI_SAFE_RELEASE(pRootSignature);
    }
};

// ---[ D3D12 ] ---------------------------------------------------------------

struct D3D12BufferInfo
{
    UINT64 size = 0;
    UINT64 alignment = 0;
    D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;

    D3D12BufferInfo() {}
    D3D12BufferInfo(UINT64 InSize, UINT64 InAlignment, D3D12_HEAP_TYPE InHeapType, D3D12_RESOURCE_FLAGS InFlags, D3D12_RESOURCE_STATES InState) :
        size(InSize),
        alignment(InAlignment),
        heapType(InHeapType),
        flags(InFlags),
        state(InState) {}

    D3D12BufferInfo(UINT64 InSize, D3D12_RESOURCE_FLAGS InFlags, D3D12_RESOURCE_STATES InState) :
        size(InSize),
        flags(InFlags),
        state(InState) {}

    D3D12BufferInfo(UINT64 InSize, D3D12_HEAP_TYPE InHeapType, D3D12_RESOURCE_STATES InState) :
        size(InSize),
        heapType(InHeapType),
        state(InState) {}

    D3D12BufferInfo(UINT64 InSize, D3D12_RESOURCE_FLAGS InFlags) :
        size(InSize),
        flags(InFlags) {}
};

struct D3D12Resources
{
    // Render Targets 
    ID3D12Resource*                        GBufferA = nullptr;        // RGB: albedo, A: primary hit flag
    ID3D12Resource*                        GBufferB = nullptr;        // XYZ: World Position, W: Hit Distance
    ID3D12Resource*                        GBufferC = nullptr;        // XYZ: Normal, W: unused
    ID3D12Resource*                        GBufferD = nullptr;        // RGB: direct lit diffuse, A: unused
    ID3D12Resource*                        RTAORaw = nullptr;
    ID3D12Resource*                        RTAOFiltered = nullptr;
    ID3D12Resource*                        PTOutput = nullptr;
    ID3D12Resource*                        PTAccumulation = nullptr;

    // Descriptor Heaps
    ID3D12DescriptorHeap*                  rtvHeap = nullptr;
    ID3D12DescriptorHeap*                  cbvSrvUavHeap = nullptr;
    ID3D12DescriptorHeap*                  samplerHeap = nullptr;

    // Root Signatures
    ID3D12RootSignature*                   computeRootSig = nullptr;
    ID3D12RootSignature*                   rasterRootSig = nullptr;

    // Pipeline State Objects
    ID3D12PipelineState*                   indirectPSO = nullptr;
    ID3D12PipelineState*                   AOFilterPSO = nullptr;
    ID3D12PipelineState*                   visBuffersPSO = nullptr;

    // Constant Buffers
    ID3D12Resource*                        cameraCB = nullptr;
    ID3D12Resource*                        materialCB = nullptr;
    ID3D12Resource*                        lightsCB = nullptr;
    ID3D12Resource*                        volumeGroupCB = nullptr;


    UINT8*                                 cameraCBStart = nullptr;
    UINT8*                                 materialCBStart = nullptr;
    UINT8*                                 lightsCBStart = nullptr;

    // Scene geometry
    std::vector<ID3D12Resource*>           sceneVBs;
    std::vector<ID3D12Resource*>           sceneIBs;
    std::vector<D3D12_VERTEX_BUFFER_VIEW>  sceneVBViews;
    std::vector<D3D12_INDEX_BUFFER_VIEW>   sceneIBViews;

    // Visualization Geometry
    ID3D12Resource*                        sphereVB = nullptr;
    ID3D12Resource*                        sphereIB = nullptr;
    D3D12_VERTEX_BUFFER_VIEW               sphereVBView;
    D3D12_INDEX_BUFFER_VIEW                sphereIBView;

    // Scene textures
    std::vector<ID3D12Resource*>           sceneTextures;
    ID3D12Resource*                        sceneTextureUploadBuffer = nullptr;

    // Additional textures
    std::vector<ID3D12Resource*>           textures;
    std::vector<ID3D12Resource*>           textureUploadBuffers;

    UINT                                   rtvDescSize = 0;
    UINT                                   cbvSrvUavDescSize = 0;
    int                                    blueNoiseIndex = -1;
    UINT                                   numVolumes = 0;
};

struct D3D12Global
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
    UINT                               frameNumber = 0;

    D3D12_VIEWPORT                     viewport;
    D3D12_RECT                         scissor;

    int                                width = 0;
    int                                height = 0;
    bool                               vsync = true;
};

// ---[ DXR ] -----------------------------------------------------------------

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

struct ShaderHitGroup
{
    ShaderProgram ahs;
    ShaderProgram chs;
    LPCWSTR exportName = L"";
    ID3D12RootSignature* pRootSignature = nullptr;

    void Release()
    {
        ahs.Release();
        chs.Release();
        RTXGI_SAFE_RELEASE(pRootSignature);
    }
};

struct DXRGlobal
{
    // Acceleration Structures
    AccelerationStructureBuffer               TLAS;
    std::vector<AccelerationStructureBuffer>  visTLASes;
    std::vector<AccelerationStructureBuffer>  BLASes;
    AccelerationStructureBuffer               probeBLAS;

    // Shader Table
    ID3D12Resource*                           shaderTable = nullptr;
    UINT                                      shaderTableRecordSize = 0;

    // Ray Tracing Shaders
    ShaderProgram                             probeRGS;
    ShaderProgram                             primaryRGS;
    ShaderProgram                             ambientOcclusionRGS;
    ShaderProgram                             probeVisRGS;
    ShaderProgram                             pathTraceRGS;
    ShaderProgram                             miss;
    ShaderHitGroup                            hit;

    // TLAS Update Compute Shader
    ID3DBlob*                                 visUpdateTLASCS = nullptr;
    ID3D12PipelineState*                      visUpdateTLASPSO = nullptr;

    // Root Signature
    ID3D12RootSignature*                      globalRootSig = nullptr;
    
    // Ray Tracing Pipeline State Object
    ID3D12StateObject*                        rtpso = nullptr;
    ID3D12StateObjectProperties*              rtpsoInfo = nullptr;

    UINT64                                    tlasSize = 0;
    std::vector<uint64_t>                     visTlasSizes;
};

//-----------------------------------------------------------------------------
// Global Functions
//-----------------------------------------------------------------------------

namespace D3D12
{
    bool Initialize(D3D12Global &d3d, D3D12Resources &resources, ShaderCompiler &shaderCompiler, Scene &scene, HWND &window);
    ID3D12RootSignature* CreateRootSignature(D3D12Global &d3d, const D3D12_ROOT_SIGNATURE_DESC &desc);
    bool CreateComputePSO(ID3D12Device* device, ID3D12RootSignature* rootSignature, ID3DBlob* shader, ID3D12PipelineState** pipeline);
    bool CreateBuffer(D3D12Global &d3d, D3D12BufferInfo &info, ID3D12Resource** ppResource);
    bool CreateTexture(D3D12Global &d3d, ID3D12Resource** resource, UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_STATES state);
    bool CreateVertexBuffer(D3D12Global &d3d, ID3D12Resource** vb, D3D12_VERTEX_BUFFER_VIEW &view, const MeshPrimitive &primitive);
    bool CreateIndexBuffer(D3D12Global &d3d, ID3D12Resource** ib, D3D12_INDEX_BUFFER_VIEW &view, const MeshPrimitive &primitive);
    bool CreateDevice(D3D12Global &d3d);
    bool ResetCmdList(D3D12Global &d3d);
    void SubmitCmdList(D3D12Global &d3d);
    void Present(D3D12Global &d3d);
    bool WaitForGPU(D3D12Global &d3d);
    bool ScreenCapture(D3D12Global &d3d, std::string filename);
    bool MoveToNextFrame(D3D12Global &d3d);
    void Cleanup(D3D12Global &d3d);
};

namespace D3DResources
{
    bool Initialize(D3D12Global &d3d, D3D12Resources &resources, Scene &scene);
    void Cleanup(D3D12Resources &resources);
}

namespace DXR
{
    bool Initialize(D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources, ShaderCompiler &compiler, Scene &scene);
    bool CreateVisTLAS(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources, int numProbes, size_t index = 0);
    bool UpdateVisTLAS(D3D12Global& d3d, DXRGlobal& dxr, D3D12Resources& resources, int numProbes, float probeScale, size_t index = 0);
    void Cleanup(DXRGlobal &dxr);
};

namespace DescriptorHeapConstants
{
    // Describe the CBV/SRV/UAV descriptor heap
    // 0:  1 CBV for the camera constants (b1)
    // 1:  1 CBV for the lights constants (b2)
    // 2:  1 UAV for the GBufferA (u0)
    // 3:  1 UAV for the GBufferB (u1)
    // 4:  1 UAV for the GBufferC (u2)
    // 5:  1 UAV for the GBufferD (u3)
    // 6:  1 UAV for the RT AO Raw (u4)
    // 7:  1 UAV for the RT AO Filtered (u5)
    // 8:  1 UAV for the PT output (u6)
    // 9:  1 UAV for the PT accumulation (u7)
    // 10:  1 UAV for the Vis TLAS instance data (u6)
    // 11,12,13 3 UAVs for three more Vis TLAS
    // --- Entries added by the SDK for a DDGIVolume -----------
    // 14: 1 UAV for the probe RT radiance (u0, space1)
    // 15: 1 UAV for the probe irradiance (u1, space1)
    // 16: 1 UAV for the probe distance (u2, space1)
    // 17: 1 UAV for the probe offsets (optional) (u3, space1)
    // 18: 1 UAV for the probe states (optional) (u4, space1)
    // ---------------------------------------------------------
    // Entries used for sampling the DDGIVolume:
    // 19: 1 SRV for the probe irradiance (t0)
    // 20: 1 SRV for the probe distance (t1)
    // ---------------------------------------------------------
    // 21-27, 28-34, 35-41: same as 14-20 above for three more volumes
    // ---------------------------------------------------------
    // Loaded Textures:
    // 42: 1 SRV for 256x256 RGB blue noise texture
    // ---------------------------------------------------------
    // ImGui:
    // 43: ImGui font texture
    // ---------------------------------------------------------
    // Scene Textures
    // 44+: loaded scene textures
    // ---------------------------------------------------------
    const int CAMERA_OFFSET     = 0;
    const int LIGHTS_OFFSET     = 1;
    const int RT_GBUFFER_OFFSET = 2;
    const int RT_AO_OFFSET      = 6;
    const int PT_OFFSET         = 8;
    const int VIS_TLAS_OFFSET   = 10;
    const int VOLUME_OFFSET     = 14;
    const int DESCRIPTORS_PER_VOLUME = 7;
    const int BLUE_NOISE_OFFSET = 42;
    const int IMGUI_OFFSET      = 43;
    const int SCENE_TEXTURE_OFFSET = 44;
};