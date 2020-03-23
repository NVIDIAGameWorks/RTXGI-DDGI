/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#define STB_IMAGE_IMPLEMENTATION
#include "thirdparty/stb_image.h"

#include "Textures.h"

namespace Textures
{

//----------------------------------------------------------------------------------------------------------
// Public Functions
//----------------------------------------------------------------------------------------------------------

/**
* Loads a texture and returns the texture index into resources.textures.
* Uses format DXGI_FORMAT_R8G8B8A8_UNORM (_SRGB).
* Does not make mips and puts the texture in state D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE.
* Also, an upload buffer is made per texture loaded and not freed until the app shuts down.
*/
bool LoadTexture(string filepath, bool sRGB, D3D12Info &d3d, D3D12Resources &resources, int &textureIndex, string textureName)
{
    // Load pixels from image on disk
    int width, height, components;
    stbi_uc* pixels = stbi_load(filepath.c_str(), &width, &height, &components, 4);
    if (!pixels) return false;

    RuntimeTexture newTexture;
    newTexture.format = sRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    UINT uploadPitch = (width * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);

    // Create upload buffer and put pixels into it
    {
        UINT uploadSize = height * uploadPitch;

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Alignment = 0;
        desc.Width = uploadSize;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_HEAP_PROPERTIES props = {};
        props.Type = D3D12_HEAP_TYPE_UPLOAD;
        props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        HRESULT hr = d3d.device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&newTexture.uploadBuffer));
        if (FAILED(hr)) return false;

#if RTXGI_NAME_D3D_OBJECTS
        std::wstring uploadName = std::wstring(textureName.begin(), textureName.end());
        uploadName.append(L" Texture Upload Buffer");
        newTexture.uploadBuffer->SetName(uploadName.c_str());
#endif

        void* mapped = NULL;
        D3D12_RANGE range = { 0, uploadSize };
        hr = newTexture.uploadBuffer->Map(0, &range, &mapped);
        if (FAILED(hr)) return false;

        for (int y = 0; y < height; y++)
        {
            memcpy((void*)((uintptr_t)mapped + y * uploadPitch), pixels + y * width * 4, width * 4);
        }
        newTexture.uploadBuffer->Unmap(0, &range);
    }

    // Create texture resource
    {
        D3D12_HEAP_PROPERTIES props = {};
        props.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Alignment = 0;
        desc.Width = width;
        desc.Height = height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = newTexture.format;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        HRESULT hr = d3d.device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&newTexture.texture));
        if (FAILED(hr)) return false;

#if defined(RTXGI_NAME_D3D_OBJECTS)
        std::wstring name = std::wstring(textureName.begin(), textureName.end());
        name.append(L" Texture");
        newTexture.texture->SetName(name.c_str());
#endif
    }

    // Copy pixels from the upload buffer to the texture, on the gpu timeline, then transition the resource to pixel shader reading
    {
        D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
        srcLocation.pResource = newTexture.uploadBuffer;
        srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLocation.PlacedFootprint.Footprint.Format = newTexture.format;
        srcLocation.PlacedFootprint.Footprint.Width = width;
        srcLocation.PlacedFootprint.Footprint.Height = height;
        srcLocation.PlacedFootprint.Footprint.Depth = 1;
        srcLocation.PlacedFootprint.Footprint.RowPitch = uploadPitch;

        D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
        dstLocation.pResource = newTexture.texture;
        dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLocation.SubresourceIndex = 0;

        d3d.cmdList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, NULL);

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = newTexture.texture;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        d3d.cmdList->ResourceBarrier(1, &barrier);
    }

    // Track this texture and return its index
    textureIndex = (int)resources.textures.size();
    resources.textures.push_back(newTexture);

    stbi_image_free(pixels);
    return true;
}

}
