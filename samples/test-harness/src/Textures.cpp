/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "Textures.h"

#define STBI_NO_BMP
#define STBI_NO_GIF
#define STBI_NO_PIC
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

static const D3D12_HEAP_PROPERTIES defaultHeapProps =
{
    D3D12_HEAP_TYPE_DEFAULT,
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN,
    0, 0
};

static const D3D12_HEAP_PROPERTIES uploadHeapProps =
{
    D3D12_HEAP_TYPE_UPLOAD,
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN,
    0, 0
};

namespace Textures
{

//----------------------------------------------------------------------------------------------------------
// Public Functions
//----------------------------------------------------------------------------------------------------------

/**
* Load an image from disk.
*/
bool LoadTexture(Texture &texture)
{
    // Load the texture with stb_image (require 4 component RGBA)
    texture.pixels = stbi_load(texture.filepath.c_str(), &texture.width, &texture.height, &texture.stride, STBI_rgb_alpha);
    texture.stride = 4;
    if (!texture.pixels)
    {
        std::string msg = "Error: failed to load texture: \'";
        msg.append(texture.name);
        msg.append("\'");
        MessageBox(NULL, msg.c_str(), "Error", MB_OK);
        return false;
    }
    return true;
}

/**
* Release texture memory.
*/
void UnloadTexture(Texture &texture)
{
    stbi_image_free(texture.pixels);
    texture.pixels = nullptr;
}

/**
* Loads a texture from disk, creates a D3D12 GPU resource, and uploads the texels to the GPU. 
* Returns the index of the texture in the resources.textures array.
* Uses format DXGI_FORMAT_R8G8B8A8_UNORM.
* Does not make mips and puts the texture in state D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE.
* An upload buffer is created and not freed until cleanup.
*/
bool LoadAndCreateTexture(D3D12Global &d3d, D3D12Resources &resources, Texture &texture, int &index)
{
    // Load the texture from disk
    if (!LoadTexture(texture)) return false;

    // Create the texture resource on the default heap
    ID3D12Resource* textureResource = nullptr;
    {
        // Describe the texture resource
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Alignment = 0;
        desc.Width = texture.width;
        desc.Height = texture.height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        // Create the texture resource        
        HRESULT hr = d3d.device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&textureResource));
        if (FAILED(hr)) return false;
#if defined(RTXGI_NAME_D3D_OBJECTS)
        std::string name = "Texture: ";
        name.append(texture.name);
        std::wstring n = std::wstring(name.begin(), name.end());
        textureResource->SetName(n.c_str());
#endif
    }

    UINT rowPitch = RTXGI_ALIGN(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT, texture.width * texture.stride);
    ID3D12Resource* uploadBuffer = nullptr;

    // Create an upload buffer and copy texels into it
    {
        UINT uploadBufferSize = (rowPitch * texture.height);

        // Describe the upload buffer
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Alignment = 0;
        desc.Width = uploadBufferSize;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;
        
        // Create the upload buffer        
        HRESULT hr = d3d.device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&uploadBuffer));
        if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
        std::string name = " Texture Upload Buffer: ";
        name.append(texture.name);
        std::wstring n = std::wstring(name.begin(), name.end());
        uploadBuffer->SetName(n.c_str());
#endif

        // Copy the texel data to the upload buffer
        UINT8* pData = nullptr;
        D3D12_RANGE range = { 0, uploadBufferSize };
        hr = uploadBuffer->Map(0, &range, reinterpret_cast<void**>(&pData));
        if (FAILED(hr)) return false;

        size_t rowSize = (texture.width * texture.stride);
        if (rowSize < D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)
        {
            // Copy each row of the image, padding the copies for the row pitch alignment
            UINT8* source = texture.pixels;
            for (size_t rowIndex = 0; rowIndex < texture.height; rowIndex++)
            {
                memcpy(pData, source, rowSize);
                pData += D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
                source += rowSize;
            }
        }
        else
        {
            // RowPitch is aligned, copy the entire image
            size_t size = (texture.width * texture.height * texture.stride);
            memcpy(pData, texture.pixels, size);
            pData += size;
        }

        uploadBuffer->Unmap(0, &range);
    }

    // Copy the texture from the upload buffer to the default heap resource, then transition it to a shader resource
    {
        // Describe the upload buffer resource (source)
        D3D12_TEXTURE_COPY_LOCATION source = {};
        source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        source.pResource = uploadBuffer;
        source.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        source.PlacedFootprint.Footprint.Width = texture.width;
        source.PlacedFootprint.Footprint.Height = texture.height;
        source.PlacedFootprint.Footprint.RowPitch = rowPitch;
        source.PlacedFootprint.Footprint.Depth = 1;

        // Describe the default heap resource (destination)
        D3D12_TEXTURE_COPY_LOCATION destination = {};
        destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destination.pResource = textureResource;
        destination.SubresourceIndex = 0;

        // Copy the texture from the upload heap to the default heap
        d3d.cmdList->CopyTextureRegion(&destination, 0, 0, 0, &source, NULL);

        // Transition the default heap texture resource to a shader resource
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = textureResource;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        d3d.cmdList->ResourceBarrier(1, &barrier);
    }

    // Track the texture resource and return the array index
    index = (int)resources.textures.size();
    resources.textures.push_back(textureResource);

    // Track the upload buffer so we can release it later
    resources.textureUploadBuffers.push_back(uploadBuffer);

    // Unload texels
    UnloadTexture(texture);

    return true;
}

}
