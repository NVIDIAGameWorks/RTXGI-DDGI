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

#if defined(_WIN32) || defined(WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>

#include <rtxgi/ddgi/DDGIVolume.h>

namespace Graphics
{
    namespace Vulkan
    {
        bool Check(VkResult hr, std::string fileName, uint32_t lineNumber);
        #define VKCHECK(hr) if(!Check(hr, __FILE__, __LINE__)) { return false; }

    #ifdef GFX_PERF_INSTRUMENTATION
        struct Timestamp
        {
            uint64_t timestamp;
            uint64_t availability;
        };

        #define GPU_TIMESTAMP_BEGIN(x) vkCmdWriteTimestamp(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, vkResources.timestampPool, x);
        #define GPU_TIMESTAMP_END(x) vkCmdWriteTimestamp(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, vkResources.timestampPool, x);
    #else
        #define GPU_TIMESTAMP_BEGIN(x) 
        #define GPU_TIMESTAMP_END(x) 
    #endif

        enum class EHeapType
        {
            DEFAULT = 0,
            UPLOAD = 1,
        };

        struct BufferDesc
        {
            VkDeviceSize size = 0;
            VkBufferUsageFlags usage;
            VkMemoryPropertyFlags memoryPropertyFlags;
        };

        struct TextureDesc
        {
            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t arraySize = 1;
            uint32_t mips = 1;
            VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
            VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        };

        struct RasterDesc
        {
            std::vector<VkDynamicState> states;
            VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
            VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
            VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
            VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
            VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
            VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
            VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
            VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
            VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};

            RasterDesc()
            {
                states.push_back(VK_DYNAMIC_STATE_VIEWPORT);
                states.push_back(VK_DYNAMIC_STATE_SCISSOR);

                vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
                inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
                viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
                colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
                rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
                depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
                multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
                dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

                colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;
                depthStencilStateCreateInfo.front = depthStencilStateCreateInfo.back;
            }

        };

        struct ImageBarrierDesc
        {
            VkImageLayout oldLayout;
            VkImageLayout newLayout;
            VkPipelineStageFlags srcMask;
            VkPipelineStageFlags dstMask;
            VkImageSubresourceRange subresourceRange;
        };

        struct AllocateMemoryDesc
        {
            VkMemoryRequirements requirements;
            VkMemoryPropertyFlags properties;
            VkMemoryAllocateFlags flags;
        };

        struct AccelerationStructure
        {
            VkAccelerationStructureKHR asKHR = nullptr;
            VkBuffer asBuffer = nullptr;
            VkDeviceMemory asMemory = nullptr;
            VkBuffer scratch = nullptr;
            VkDeviceMemory scratchMemory = nullptr;
            VkBuffer instances = nullptr;                   // Only valid for TLAS
            VkDeviceMemory instancesMemory = nullptr;       // Only valid for TLAS
            VkBuffer instancesUpload = nullptr;             // Only valid for TLAS
            VkDeviceMemory instancesUploadMemory = nullptr; // Only valid for TLAS

            void Release(VkDevice device)
            {
                vkDestroyAccelerationStructureKHR(device, asKHR, nullptr);
                vkDestroyBuffer(device, asBuffer, nullptr);
                vkFreeMemory(device, asMemory, nullptr);
                vkDestroyBuffer(device, scratch, nullptr);
                vkFreeMemory(device, scratchMemory, nullptr);
                if (instances != nullptr) vkDestroyBuffer(device, instances, nullptr);
                if (instancesMemory != nullptr) vkFreeMemory(device, instancesMemory, nullptr);
                if (instancesUpload != nullptr) vkDestroyBuffer(device, instancesUpload, nullptr);
                if (instancesUploadMemory != nullptr) vkFreeMemory(device, instancesUploadMemory, nullptr);

                asKHR = nullptr;
                asBuffer = nullptr;
                asMemory = nullptr;
                scratch = nullptr;
                scratchMemory = nullptr;
                instances = nullptr;
                instancesMemory = nullptr;
                instancesUpload = nullptr;
                instancesUploadMemory = nullptr;
            }
        };

        struct HitGroupShaderModules
        {
            VkShaderModule chs = nullptr;
            VkShaderModule ahs = nullptr;
            VkShaderModule is = nullptr;

            bool hasCHS() const { return (chs != nullptr); }
            bool hasAHS() const { return (ahs != nullptr); }
            bool hasIS() const { return (is != nullptr); }
            uint32_t numStages() const { return (hasCHS() + hasAHS() + hasIS()); }

            void Release(VkDevice device)
            {
                if(chs) vkDestroyShaderModule(device, chs, nullptr);
                if(ahs) vkDestroyShaderModule(device, ahs, nullptr);
                if(is) vkDestroyShaderModule(device, is, nullptr);
            }
        };

        struct ShaderModules
        {
            VkShaderModule vs = nullptr;
            VkShaderModule ps = nullptr;
            uint32_t numGroups = 0;

            void Release(VkDevice device)
            {
                if (vs) vkDestroyShaderModule(device, vs, nullptr);
                if (ps) vkDestroyShaderModule(device, ps, nullptr);
            }
        };

        struct RTShaderModules
        {
            VkShaderModule rgs = nullptr;
            VkShaderModule miss = nullptr;
            std::vector<HitGroupShaderModules> hitGroups;
            uint32_t numGroups = 0;

            void Release(VkDevice device)
            {
                if(rgs) vkDestroyShaderModule(device, rgs, nullptr);
                if(miss) vkDestroyShaderModule(device, miss, nullptr);
                for (uint32_t hitGroupIndex = 0; hitGroupIndex < static_cast<uint32_t>(hitGroups.size()); hitGroupIndex++)
                {
                    hitGroups[hitGroupIndex].Release(device);
                }
                hitGroups.clear();
                numGroups = 0;
            }
        };

        struct Features
        {
            uint32_t waveLaneCount;
        };

        struct Globals
        {
            VkInstance                              instance = nullptr;
            VkPhysicalDevice                        physicalDevice = nullptr;
            VkDevice                                device = nullptr;
            VkQueue                                 queue = nullptr;
            int                                     queueFamilyIndex = -1;

            VkCommandPool                           commandPool = nullptr;
            VkCommandBuffer                         cmdBuffer[2] = { nullptr, nullptr };

            VkSurfaceKHR                            surface = nullptr;
            VkSwapchainKHR                          swapChain = nullptr;
            VkImage                                 swapChainImage[2] = { nullptr, nullptr };
            VkImageView                             swapChainImageView[2] = { nullptr, nullptr };
            VkFormat                                swapChainFormat = VK_FORMAT_UNDEFINED;
            VkColorSpaceKHR                         swapChainColorSpace;

            VkRenderPass                            renderPass = nullptr;
            VkFramebuffer                           frameBuffer[2] = { nullptr, nullptr };

            VkFence                                 fences[2] = { nullptr, nullptr };
            uint32_t                                frameIndex = 0;
            uint32_t                                frameNumber = 0;

            VkSemaphore                             imageAcquiredSemaphore = nullptr;
            VkSemaphore                             renderingCompleteSemaphore = nullptr;

            VkViewport                              viewport = {};
            VkRect2D                                scissor = {};

            GLFWwindow*                             window = nullptr;
            RECT                                    windowRect = {};

            Shaders::ShaderCompiler                 shaderCompiler;

            Features                                features = {};

            // For Windowed->Fullscreen->Windowed transitions
            int                                     x = 0;
            int                                     y = 0;
            int                                     windowWidth = 0;
            int                                     windowHeight = 0;

            int                                     width = 0;
            int                                     height = 0;
            bool                                    vsync = true;
            bool                                    vsyncChanged = false;
            int                                     fullscreen = 0;
            bool                                    fullscreenChanged = false;

            bool                                    supportsShaderExecutionReordering = false;

            VkDebugUtilsMessengerEXT                debugUtilsMessenger = nullptr;

            VkPhysicalDeviceFeatures                           deviceFeatures = {};
            VkPhysicalDeviceProperties2                        deviceProps = {};
            VkPhysicalDeviceAccelerationStructurePropertiesKHR deviceASProps = {};
            VkPhysicalDeviceRayTracingPipelinePropertiesKHR    deviceRTPipelineProps = {};
            VkPhysicalDeviceSubgroupProperties                 deviceSubgroupProps = {};
        };

        struct RenderTargets
        {
            // GBuffer Textures
            VkImage                        GBufferA = nullptr;         // RGB: Albedo, A: Primary Ray Hit Flag
            VkDeviceMemory                 GBufferAMemory = nullptr;
            VkImageView                    GBufferAView = nullptr;

            VkImage                        GBufferB = nullptr;         // XYZ: World Position, W: Primary Ray Hit Distance
            VkDeviceMemory                 GBufferBMemory = nullptr;
            VkImageView                    GBufferBView = nullptr;

            VkImage                        GBufferC = nullptr;         // XYZ: Normal, W: unused
            VkDeviceMemory                 GBufferCMemory = nullptr;
            VkImageView                    GBufferCView = nullptr;

            VkImage                        GBufferD = nullptr;         // RGB: Direct Diffuse, A: unused
            VkDeviceMemory                 GBufferDMemory = nullptr;
            VkImageView                    GBufferDView = nullptr;

            // Handles to resources created elsewhere
            VkImageView                    RTAOOutputView = nullptr;  // R8 UNORM
            VkImageView                    DDGIOutputView = nullptr;  // RGBA16 FLOAT
        };

        struct Resources
        {
            // Root Constants
            GlobalConstants                         constants = {};

            // Descriptors
            VkDescriptorPool                        descriptorPool = nullptr;
            VkDescriptorSetLayout                   descriptorSetLayout = nullptr;

            // Queries
            VkQueryPool                             timestampPool = nullptr;
            VkBuffer                                timestamps = nullptr;
            VkDeviceMemory                          timestampsMemory = nullptr;

            // Pipeline Layouts
            VkPipelineLayout                        pipelineLayout = nullptr;

            // Constant Buffers
            VkBuffer                                cameraCB = nullptr;
            VkDeviceMemory                          cameraCBMemory = nullptr;
            uint8_t*                                cameraCBPtr = nullptr;

            // Structured Buffers
            VkBuffer                                lightsSTB = nullptr;
            VkDeviceMemory                          lightsSTBMemory = nullptr;
            VkBuffer                                lightsSTBUploadBuffer = nullptr;
            VkDeviceMemory                          lightsSTBUploadMemory = nullptr;
            uint8_t*                                lightsSTBPtr = nullptr;

            VkBuffer                                materialsSTB = nullptr;
            VkDeviceMemory                          materialsSTBMemory = nullptr;
            VkBuffer                                materialsSTBUploadBuffer = nullptr;
            VkDeviceMemory                          materialsSTBUploadMemory = nullptr;
            uint8_t*                                materialsSTBPtr = nullptr;

            // ByteAddress Buffers
            VkBuffer                                meshOffsetsRB = nullptr;
            VkDeviceMemory                          meshOffsetsRBMemory = nullptr;
            VkBuffer                                meshOffsetsRBUploadBuffer = nullptr;
            VkDeviceMemory                          meshOffsetsRBUploadMemory = nullptr;
            uint8_t*                                meshOffsetsRBPtr = nullptr;

            VkBuffer                                geometryDataRB = nullptr;
            VkDeviceMemory                          geometryDataRBMemory = nullptr;
            VkBuffer                                geometryDataRBUploadBuffer = nullptr;
            VkDeviceMemory                          geometryDataRBUploadMemory = nullptr;
            uint8_t*                                geometryDataRBPtr = nullptr;

            // Shared Render Targets
            RenderTargets                           rt;

            // Scene Geometry
            std::vector<VkBuffer>                   sceneVBs;
            std::vector<VkDeviceMemory>             sceneVBMemory;
            std::vector<VkBuffer>                   sceneVBUploadBuffers;
            std::vector<VkDeviceMemory>             sceneVBUploadMemory;

            std::vector<VkBuffer>                   sceneIBs;
            std::vector<VkDeviceMemory>             sceneIBMemory;
            std::vector<VkBuffer>                   sceneIBUploadBuffers;
            std::vector<VkDeviceMemory>             sceneIBUploadMemory;

            // Scene Ray Tracing Acceleration Structures
            std::vector<AccelerationStructure>      blas;
            AccelerationStructure                   tlas;

            // Scene textures
            std::vector<VkImage>                    sceneTextures;
            std::vector<VkDeviceMemory>             sceneTextureMemory;
            std::vector<VkImageView>                sceneTextureViews;
            std::vector<VkBuffer>                   sceneTextureUploadBuffer;
            std::vector<VkDeviceMemory>             sceneTextureUploadMemory;

            // Additional textures
            std::vector<VkImage>                    textures;
            std::vector<VkDeviceMemory>             textureMemory;
            std::vector<VkBuffer>                   textureUploadBuffer;
            std::vector<VkDeviceMemory>             textureUploadMemory;
            std::vector<VkImageView>                textureViews;

            // Samplers
            std::vector<VkSampler>                  samplers;
        };

        VkDeviceAddress GetBufferDeviceAddress(VkDevice device, VkBuffer buffer);

        void SetImageMemoryBarrier(VkCommandBuffer cmdBuffer, VkImage image, const ImageBarrierDesc info);
        void SetImageLayoutBarrier(VkCommandBuffer cmdBuffer, VkImage image, const ImageBarrierDesc info);

        bool CreateBuffer(Globals& vk, const BufferDesc& info, VkBuffer* buffer, VkDeviceMemory* memory);
        bool CreateIndexBuffer(Globals& vk, const Scenes::Mesh& mesh, VkBuffer* ib, VkDeviceMemory* ibMemory, VkBuffer* ibUpload, VkDeviceMemory* ibUploadMemory);
        bool CreateVertexBuffer(Globals& vk, const Scenes::Mesh& mesh, VkBuffer* vb, VkDeviceMemory* vbMemory, VkBuffer* vbUpload, VkDeviceMemory* vbUploadMemory);
        bool CreateTexture(Globals& vk, const TextureDesc& info, VkImage* image, VkDeviceMemory* imageMemory, VkImageView* imageView);

        bool CreateShaderModule(VkDevice device, const Shaders::ShaderProgram& shader, VkShaderModule* module);
        bool CreateRasterShaderModules(VkDevice device, const Shaders::ShaderPipeline& shaders, ShaderModules& modules);
        bool CreateRayTracingShaderModules(VkDevice device, const Shaders::ShaderRTPipeline& shaders, RTShaderModules& modules);

        bool CreateRasterPipeline(
            VkDevice device,
            VkPipelineLayout pipelineLayout,
            VkRenderPass renderPass,
            const Shaders::ShaderPipeline& shaders,
            const ShaderModules& modules,
            const RasterDesc& desc,
            VkPipeline* pipeline);

        bool CreateComputePipeline(
            VkDevice device,
            VkPipelineLayout pipelineLayout,
            const Shaders::ShaderProgram& shader,
            const VkShaderModule& module,
            VkPipeline* pipeline);

        bool CreateRayTracingPipeline(
            VkDevice device,
            VkPipelineLayout pipelineLayout,
            const Shaders::ShaderRTPipeline& shaders,
            const RTShaderModules& modules,
            VkPipeline* pipeline);

        void BeginRenderPass(Globals& vk);

        bool WriteResourceToDisk(Globals& vk, std::string file, VkImage image, uint32_t width, uint32_t height, uint32_t arraySize, VkFormat imageFormat, VkImageLayout originalLayout);

    #ifdef GFX_NAME_OBJECTS
        void SetObjectName(VkDevice device, uint64_t handle, const char* name, VkObjectType type);
    #endif

    #ifdef GFX_PERF_MARKERS
        void AddPerfMarker(Globals& vk, uint8_t r, uint8_t g, uint8_t b, std::string name);
    #endif

        namespace SamplerIndices
        {
            const int BILINEAR_WRAP = 0;                                            //  0: bilinear filter, repeat
            const int POINT_CLAMP = BILINEAR_WRAP + 1;                              //  1: point (nearest neighbor) filter, clamp
            const int ANISO_WRAP = POINT_CLAMP + 1;                                 //  2: anisotropic filter, repeat
        }

        namespace DescriptorLayoutBindings
        {
            const int SAMPLERS = 0;                                                 //  0: Samplers (resource array)

            const int CB_CAMERA = SAMPLERS + 1;                                     //  1: Camera constant buffer

            // Structured Buffers
            const int STB_LIGHTS = CB_CAMERA + 1;                                   //  2: Lights structured buffer
            const int STB_MATERIALS = STB_LIGHTS + 1;                               //  3: Materials structured buffer
            const int STB_TLAS_INSTANCES = STB_MATERIALS + 1;                       //  4: TLAS instance descriptors structured buffer
            const int STB_DDGI_VOLUME_CONSTS = STB_TLAS_INSTANCES + 1;              //  5: DDGIVolume constants structured buffer
            const int STB_DDGI_VOLUME_RESOURCE_INDICES = STB_DDGI_VOLUME_CONSTS + 1;//  6: DDGIVolume resource indices structured buffer

            // Unordered Access Views
            const int UAV_START = STB_DDGI_VOLUME_RESOURCE_INDICES + 1;             //  7: UAV Start
            const int UAV_STB_TLAS_INSTANCES = UAV_START;                           //  7: Scene TLAS instance descriptors structured buffer UAV
            const int UAV_TEX2D = UAV_STB_TLAS_INSTANCES + 1;                       //  8: RWTex2D UAVs (resource array)
            const int UAV_TEX2DARRAY = UAV_TEX2D + 1;                               //  9: RWTex2DArray UAVs (resource array)

            // Shader Resource Views
            const int SRV_TLAS = UAV_TEX2DARRAY + 1;                                // 10: TLAS SRVs (resource array)
            const int SRV_TEX2D = SRV_TLAS + 1;                                     // 11: Tex2D SRVs (resource array)
            const int SRV_TEX2DARRAY = SRV_TEX2D + 1;                               // 12: Tex2DArray SRVs (resource array)
            const int SRV_BYTEADDRESS = SRV_TEX2DARRAY + 1;                         // 13: ByteAddressBuffer SRVs (resource array)
        };

        namespace RWTex2DIndices
        {
            const int PT_OUTPUT = 0;                                                //  0: PT Output RWTexture
            const int PT_ACCUMULATION = PT_OUTPUT + 1;                              //  1: PT Accumulation RWTexture
            const int GBUFFERA = PT_ACCUMULATION + 1;                               //  2: GBufferA RWTexture
            const int GBUFFERB = GBUFFERA + 1;                                      //  3: GBufferB RWTexture
            const int GBUFFERC = GBUFFERB + 1;                                      //  4: GBufferC RWTexture
            const int GBUFFERD = GBUFFERC + 1;                                      //  5: GBufferD RWTexture
            const int RTAO_OUTPUT = GBUFFERD + 1;                                   //  6: RTAO Output RWTexture
            const int RTAO_RAW = RTAO_OUTPUT   + 1;                                 //  7: RTAO Raw RWTexture
            const int DDGI_OUTPUT = RTAO_RAW + 1;                                   //  8: DDGI Output RWTexture
        }

      //namespace RWTex2DArrayIndices
      //{
      //    const int DDGIVOLUME_0_RAYDATA = 0;
      //    const int DDGIVOLUME_0_IRRADIANCE = 1;
      //    const int DDGIVOLUME_0_DISTANCE = 2;
      //    const int DDGIVOLUME_0_PROBEDATA = 3;
      //    const int DDGIVOLUME_1_RAYDATA = 4;
      //    const int DDGIVOLUME_1_IRRADIANCE = 5;
      //    ...etc..
      //}

        namespace TLASIndices
        {
            const int SCENE = 0;                                                    //  0: Scene BVH
            const int DDGI_PROBE_VIS = SCENE + 1;                                   //  1: DDGI Probe Vis BVH
        }

        namespace Tex2DIndices
        {
            const int BLUE_NOISE = 0;                                               //  0: Blue Noise Texture
            const int IMGUI_FONTS = BLUE_NOISE + 1;                                 //  1: ImGui Font Texture
            const int SCENE_TEXTURES = IMGUI_FONTS + 1;                             //  2: Material Textures (300 max)
        }

        //namespace Tex2DArrayIndices
        //{
        //    const int DDGIVOLUME_0_RAYDATA = 0;
        //    const int DDGIVOLUME_0_IRRADIANCE = 1;
        //    const int DDGIVOLUME_0_DISTANCE = 2;
        //    const int DDGIVOLUME_0_PROBEDATA = 3;
        //    const int DDGIVOLUME_1_RAYDATA = 4;
        //    const int DDGIVOLUME_1_IRRADIANCE = 5;
        //    ...etc..
        //}

        namespace ByteAddressIndices
        {
            const int SPHERE_INDICES = 0;                                           //  0: DDGI Probe Vis Sphere Index Buffer
            const int SPHERE_VERTICES = SPHERE_INDICES + 1;                         //  1: DDGI Probe Vis Sphere Vertex Buffer
            const int MATERIAL_INDICES = SPHERE_VERTICES + 1;                       //  2: Mesh Primitive Material Indices
            const int INDICES = MATERIAL_INDICES + 1;                               //  3: Mesh Primitive Index Buffers (interleaved with VB)
            const int VERTICES = INDICES + 1;                                       //  4: Mesh Primitive Vertex Buffers (interleaved with IB)
        }

    }

    using Globals = Graphics::Vulkan::Globals;
    using GlobalResources = Graphics::Vulkan::Resources;

}
