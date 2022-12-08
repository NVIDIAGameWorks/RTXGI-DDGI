/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

#include <dxgi1_6.h>
#include <d3d12.h>

#ifdef GFX_PERF_MARKERS
#include <pix.h>
#endif

#include <rtxgi/ddgi/DDGIVolume.h>

namespace Graphics
{
    namespace D3D12
    {
        static const D3D12_HEAP_PROPERTIES defaultHeapProps = { D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0 };
        static const D3D12_HEAP_PROPERTIES uploadHeapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0 };
        static const D3D12_HEAP_PROPERTIES readbackHeapProps = { D3D12_HEAP_TYPE_READBACK, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0 };

        bool Check(HRESULT hr, std::string fileName, uint32_t lineNumber);
        #define D3DCHECK(hr) if(!Check(hr, __FILE__, __LINE__)) { return false; }

    #ifdef GFX_PERF_INSTRUMENTATION
        #define GPU_TIMESTAMP_BEGIN(x) d3d.cmdList->EndQuery(d3dResources.timestampHeap, D3D12_QUERY_TYPE_TIMESTAMP, x);
        #define GPU_TIMESTAMP_END(x) d3d.cmdList->EndQuery(d3dResources.timestampHeap, D3D12_QUERY_TYPE_TIMESTAMP, x);
    #else
        #define GPU_TIMESTAMP_BEGIN(x) 
        #define GPU_TIMESTAMP_END(x) 
    #endif

        enum class EHeapType
        {
            DEFAULT = 0,
            UPLOAD = 1,
            READBACK = 2
        };

        struct BufferDesc
        {
            UINT64 size = 0;
            UINT64 alignment = 0;
            EHeapType heap = EHeapType::UPLOAD;
            D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
            D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
        };

        struct TextureDesc
        {
            UINT width = 0;
            UINT height = 0;
            UINT arraySize = 1;
            UINT mips = 1;
            DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
            D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_GENERIC_READ;
            D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
        };

        struct RasterDesc
        {
            UINT numInputLayouts = 0;
            D3D12_INPUT_ELEMENT_DESC* inputLayoutDescs = nullptr;
            D3D12_BLEND_DESC blendDesc = {};
            D3D12_RASTERIZER_DESC rasterDesc = {};
        };

        struct AccelerationStructure
        {
            ID3D12Resource* as = nullptr;
            ID3D12Resource* scratch = nullptr;
            ID3D12Resource* instances = nullptr;        // only used in TLAS
            ID3D12Resource* instancesUpload = nullptr;  // only used in TLAS

            void Release()
            {
                SAFE_RELEASE(as);
                SAFE_RELEASE(scratch);
                SAFE_RELEASE(instances);
                SAFE_RELEASE(instancesUpload);
            }
        };

        struct Features
        {
            UINT waveLaneCount;
        };

        struct Globals
        {
            IDXGIFactory7*               factory = nullptr;
            ID3D12Device6*               device = nullptr;
            ID3D12CommandQueue*          cmdQueue = nullptr;
            ID3D12CommandAllocator*      cmdAlloc[2] = { nullptr, nullptr };
            ID3D12GraphicsCommandList4*  cmdList = nullptr;

            IDXGISwapChain4*             swapChain = nullptr;
            ID3D12Resource*              backBuffer[2] = { nullptr, nullptr };

            ID3D12Fence*                 fence = nullptr;
            UINT64                       fenceValue = 0;
            HANDLE                       fenceEvent;
            UINT                         frameIndex = 0;
            UINT                         frameNumber = 0;

            D3D12_VIEWPORT               viewport;
            D3D12_RECT                   scissor;

            GLFWwindow*                  window = nullptr;
            RECT                         windowRect = {};

            Shaders::ShaderCompiler      shaderCompiler;

            Features                     features = {};

            // For Windowed->Fullscreen->Windowed transitions
            int                          x = 0;
            int                          y = 0;
            int                          windowWidth = 0;
            int                          windowHeight = 0;

            int                          width = 0;
            int                          height = 0;
            bool                         vsync = true;
            bool                         vsyncChanged = false;
            int                          fullscreen = 0;
            bool                         fullscreenChanged = false;

            bool                         allowTearing = false;
            bool                         supportsShaderExecutionReordering = false;
        };

        struct RenderTargets
        {
            // GBuffer Textures
            ID3D12Resource*              GBufferA = nullptr;  // RGB: Albedo, A: Primary Ray Hit Flag
            ID3D12Resource*              GBufferB = nullptr;  // XYZ: World Position, W: Primary Ray Hit Distance
            ID3D12Resource*              GBufferC = nullptr;  // XYZ: Normal, W: unused
            ID3D12Resource*              GBufferD = nullptr;  // RGB: Direct Diffuse, A: unused
        };

        struct Resources
        {
            // Root Constants
            GlobalConstants                        constants = {};

            // Descriptor Heaps
            ID3D12DescriptorHeap*                  rtvDescHeap = nullptr;
            ID3D12DescriptorHeap*                  srvDescHeap = nullptr;
            ID3D12DescriptorHeap*                  samplerDescHeap = nullptr;

            D3D12_CPU_DESCRIPTOR_HANDLE            rtvDescHeapStart = { 0 };
            D3D12_CPU_DESCRIPTOR_HANDLE            srvDescHeapStart = { 0 };
            D3D12_CPU_DESCRIPTOR_HANDLE            samplerDescHeapStart = { 0 };

            UINT                                   rtvDescHeapEntrySize = 0;
            UINT                                   srvDescHeapEntrySize = 0;
            UINT                                   samplerDescHeapEntrySize = 0;

            // Performance Queries
            ID3D12QueryHeap*                       timestampHeap = nullptr;
            ID3D12Resource*                        timestamps = nullptr;
            UINT64                                 timestampFrequency = 0;

            // Root signature (bindless resource access)
            ID3D12RootSignature*                   rootSignature = nullptr;

            // Constant Buffers
            ID3D12Resource*                        cameraCB = nullptr;
            UINT8*                                 cameraCBPtr = nullptr;

            // Structured Buffers
            ID3D12Resource*                        lightsSTB = nullptr;
            ID3D12Resource*                        lightsSTBUpload = nullptr;
            ID3D12Resource*                        materialsSTB = nullptr;
            ID3D12Resource*                        materialsSTBUpload = nullptr;

            UINT8*                                 lightsSTBPtr = nullptr;
            UINT8*                                 materialsSTBPtr = nullptr;

            // ByteAddress Buffers
            ID3D12Resource*                        meshOffsetsRB = nullptr;
            ID3D12Resource*                        meshOffsetsRBUpload = nullptr;
            UINT8*                                 meshOffsetsRBPtr = nullptr;

            ID3D12Resource*                        geometryDataRB = nullptr;
            ID3D12Resource*                        geometryDataRBUpload = nullptr;
            UINT8*                                 geometryDataRBPtr = nullptr;

            // Shared Render Targets
            RenderTargets                          rt;

            // Scene Geometry
            std::vector<ID3D12Resource*>           sceneIBs;
            std::vector<ID3D12Resource*>           sceneIBUploadBuffers;
            std::vector<D3D12_INDEX_BUFFER_VIEW>   sceneIBViews;
            std::vector<ID3D12Resource*>           sceneVBs;
            std::vector<ID3D12Resource*>           sceneVBUploadBuffers;
            std::vector<D3D12_VERTEX_BUFFER_VIEW>  sceneVBViews;

            // Scene Ray Tracing Acceleration Structures
            std::vector<AccelerationStructure>     blas;
            AccelerationStructure                  tlas;

            // Scene textures
            std::vector<ID3D12Resource*>           sceneTextures;
            std::vector<ID3D12Resource*>           sceneTextureUploadBuffers;

            // Additional textures
            std::vector<ID3D12Resource*>           textures;
            std::vector<ID3D12Resource*>           textureUploadBuffers;
        };

        ID3D12RootSignature* CreateRootSignature(Globals& d3d, const D3D12_ROOT_SIGNATURE_DESC& desc);
        bool CreateBuffer(Globals& d3d, const BufferDesc& info, ID3D12Resource** ppResource);
        bool CreateVertexBuffer(Globals& d3d, const Scenes::Mesh& mesh, ID3D12Resource** device, ID3D12Resource** upload, D3D12_VERTEX_BUFFER_VIEW& view);
        bool CreateIndexBuffer(Globals& d3d, const Scenes::Mesh& mesh, ID3D12Resource** device, ID3D12Resource** upload, D3D12_INDEX_BUFFER_VIEW& view);
        bool CreateTexture(Globals& d3d, const TextureDesc& info, ID3D12Resource** resource);

        bool CreateRasterPSO(
            ID3D12Device* device,
            ID3D12RootSignature* rootSignature,
            const Shaders::ShaderPipeline& shaders,
            const RasterDesc& desc,
            ID3D12PipelineState** pso);

        bool CreateComputePSO(ID3D12Device* device, ID3D12RootSignature* rootSignature, const Shaders::ShaderProgram& shader, ID3D12PipelineState** pso);

        bool CreateRayTracingPSO(
            ID3D12Device5* device,
            ID3D12RootSignature* rootSignature,
            const Shaders::ShaderRTPipeline& shaders,
            ID3D12StateObject** rtpso,
            ID3D12StateObjectProperties** rtpsoProps);

        bool WriteResourceToDisk(Globals& d3d, std::string file, ID3D12Resource* pResource, D3D12_RESOURCE_STATES state);

        namespace SamplerHeapOffsets
        {
            const int BILINEAR_WRAP = 0;                                            // 0: bilinear filter, repeat
            const int POINT_CLAMP = BILINEAR_WRAP + 1;                              // 1: point (nearest neighbor) filter, clamp
            const int ANISO = POINT_CLAMP + 1;                                      // 2: anisotropic filter, repeat
        }

        namespace DescriptorHeapOffsets
        {
            // Constant Buffer Views
            const int CBV_CAMERA = 0;                                               //   0:   1 CBV for the camera constant buffer

            // Structured Buffers
            const int STB_LIGHTS = CBV_CAMERA + 1;                                  //   1:   1 SRV for the lights structured buffer
            const int STB_MATERIALS = STB_LIGHTS + 1;                               //   2:   1 SRV for the materials structured buffer
            const int STB_TLAS_INSTANCES = STB_MATERIALS + 1;                       //   3:   1 SRV for the Scene TLAS instance descriptors structured buffer
            const int STB_DDGI_VOLUME_CONSTS = STB_TLAS_INSTANCES + 1;              //   4:   1 SRV for DDGIVolume constants structured buffers
            const int STB_DDGI_VOLUME_RESOURCE_INDICES = STB_DDGI_VOLUME_CONSTS + 1;//   5:   1 SRV for DDGIVolume resource indices structured buffers

            // Unordered Access Views
            const int UAV_START = STB_DDGI_VOLUME_RESOURCE_INDICES + 1;             //   6:   UAV Start

            // RW Structured Buffers
            const int UAV_STB_TLAS_INSTANCES = UAV_START;                           //   6:   1 UAV for the Scene TLAS instance descriptors structured buffer

            // Texture2D UAV
            const int UAV_TEX2D_START = UAV_STB_TLAS_INSTANCES + 1;                 //   7:   RWTexture2D UAV Start
            const int UAV_PT_OUTPUT = UAV_TEX2D_START;                              //   7:   1 UAV for the Path Tracer Output RWTexture
            const int UAV_PT_ACCUMULATION = UAV_PT_OUTPUT + 1;                      //   8    1 UAV for the Path Tracer Accumulation RWTexture
            const int UAV_GBUFFERA = UAV_PT_ACCUMULATION + 1;                       //   9:   1 UAV for the GBufferA RWTexture
            const int UAV_GBUFFERB = UAV_GBUFFERA + 1;                              //  10:   1 UAV for the GBufferB RWTexture
            const int UAV_GBUFFERC = UAV_GBUFFERB + 1;                              //  11:   1 UAV for the GBufferC RWTexture
            const int UAV_GBUFFERD = UAV_GBUFFERC + 1;                              //  12:   1 UAV for the GBufferD RWTexture
            const int UAV_RTAO_OUTPUT = UAV_GBUFFERD + 1;                           //  13:   1 UAV for the RTAO Output RWTexture
            const int UAV_RTAO_RAW = UAV_RTAO_OUTPUT + 1;                           //  14:   1 UAV for the RTAO Raw RWTexture
            const int UAV_DDGI_OUTPUT = UAV_RTAO_RAW + 1;                           //  15:   1 UAV for the DDGI RWTexture

            // Texture2DArray UAV
            const int UAV_TEX2DARRAY_START = UAV_DDGI_OUTPUT + 1;                   //  16:   RWTexture2DArray UAV Start
            const int UAV_DDGI_VOLUME_TEX2DARRAY = UAV_TEX2DARRAY_START;            //  16:   36 UAV, 6 for each DDGIVolume (RayData, Irradiance, Distance, Probe Data, Variability, VariabilityAverage)

            // Shader Resource Views                                                //  52:   SRV Start
            const int SRV_START = UAV_DDGI_VOLUME_TEX2DARRAY + (rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors() * MAX_DDGIVOLUMES);

            // RaytracingAccelerationStructure SRV
            const int SRV_TLAS_START = SRV_START;                                   //  52:   TLAS SRV Start
            const int SRV_SCENE_TLAS = SRV_TLAS_START;                              //  52:   1 SRV for the Scene TLAS
            const int SRV_DDGI_PROBE_VIS_TLAS = SRV_SCENE_TLAS + 1;                 //  53:   1 SRV for the DDGI Probe Vis TLAS

            // Texture2D SRV
            const int SRV_TEX2D_START = SRV_TLAS_START + MAX_TLAS;                  //  54:   Texture2D SRV Start
            const int SRV_BLUE_NOISE = SRV_TEX2D_START;                             //  54:   1 SRV for the Blue Noise Texture
            const int SRV_IMGUI_FONTS = SRV_BLUE_NOISE + 1;                         //  55:   1 SRV for the ImGui Font Texture
            const int SRV_SCENE_TEXTURES = SRV_IMGUI_FONTS + 1;                     //  56: 300 SRV (max), 1 SRV for each Material Texture

            // Texture2DArray SRV
            const int SRV_TEX2DARRAY_START = SRV_SCENE_TEXTURES + MAX_TEXTURES;     // 356:   Texture2DArray SRV Start
            const int SRV_DDGI_VOLUME_TEX2DARRAY = SRV_TEX2DARRAY_START;            // 356:  36 SRV, 6 for each DDGIVolume (RayData, Irradiance, Distance, Probe Data, Variability, Variability Average)

            // ByteAddressBuffer SRV                                                // 392:   ByteAddressBuffer SRV Start
            const int SRV_BYTEADDRESS_START = SRV_TEX2DARRAY_START + (rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors() * MAX_DDGIVOLUMES);
            const int SRV_SPHERE_INDICES = SRV_BYTEADDRESS_START;                   // 392:  1 SRV for DDGI Probe Vis Sphere Index Buffer
            const int SRV_SPHERE_VERTICES = SRV_SPHERE_INDICES + 1;                 // 393:  1 SRV for DDGI Probe Vis Sphere Vertex Buffer
            const int SRV_MESH_OFFSETS = SRV_SPHERE_VERTICES + 1;                   // 394:  1 SRV for Mesh Offsets in the Geometry Data Buffer
            const int SRV_GEOMETRY_DATA = SRV_MESH_OFFSETS + 1;                     // 395:  1 SRV for Geometry (Mesh Primitive) Data
            const int SRV_INDICES = SRV_GEOMETRY_DATA + 1;                          // 396:  n SRV for Mesh Index Buffers
            const int SRV_VERTICES = SRV_INDICES + 1;                               // 397:  n SRV for Mesh Vertex Buffers
        };
    }

    using Globals = Graphics::D3D12::Globals;
    using GlobalResources = Graphics::D3D12::Resources;

}
