/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

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

#include "Textures.h"
#include "UI.h"

#if defined(GPU_COMPRESSION)
#include <d3d11.h>
static ID3D11Device* d3d11Device = nullptr;
#endif

#if __linux__
// Note: disabling gcc warnings for ignored attributes
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif
#include "thirdparty/directxtex/DirectXTex.h"
#if __linux__
#pragma GCC diagnostic pop
#endif

using namespace DirectX;

namespace Textures
{

    //----------------------------------------------------------------------------------------------------------
    // Private Functions
    //----------------------------------------------------------------------------------------------------------

    /**
     * Compute the aligned memory required for the texture.
     * Add texels to the texture if either dimension is not a factor of 4 (required for BC7 compressed formats).
     */
    bool FormatTexture(Texture& texture)
    {
        // BC7 compressed textures require 4x4 texel blocks
        // Add texels to the texture if its original dimensions aren't factors of 4
        if (texture.width % 4 != 0 || texture.height % 4 != 0)
        {
            // Get original row stride
            uint32_t rowSize = (texture.width * texture.stride);
            uint32_t numRows = texture.height;

            // Align the new texture to 4x4
            texture.width = ALIGN(4, texture.width);
            texture.height = ALIGN(4, texture.height);

            uint32_t alignedRowSize = (texture.width * texture.stride);
            uint32_t size = alignedRowSize * texture.height;

            // Copy the original texture into the new one
            size_t offset = 0;
            size_t alignedOffset = 0;
            uint8_t* texels = new uint8_t[size];
            memset(texels, 0, size);
            for(uint32_t row = 0; row < numRows; row++)
            {
                memcpy(&texels[alignedOffset], &texture.texels[offset], rowSize);
                alignedOffset += alignedRowSize;
                offset += rowSize;
            }

            // Release the memory of the original texture
            delete[] texture.texels;
            texture.texels = texels;
        }

        // Compute the texture's aligned memory size
        uint32_t rowSize = (texture.width * texture.stride);
        uint32_t rowPitch = ALIGN(256, rowSize);          // 256 == D3D12_TEXTURE_DATA_PITCH_ALIGNMENT
        texture.texelBytes = (rowPitch * texture.height);

        return (texture.texelBytes > 0);
    }

#if defined(__x86_64__) || defined(_M_X64)
    /**
     * Copy a compressed BC7 texture into our format, aligned for GPU use.
     */
    bool FormatCompressedTexture(ScratchImage& src, Texture& dst)
    {
        bool result = false;

        // Get the texture's metadata
        const TexMetadata metadata = src.GetMetadata();

        // Check if the texture's format is supported
        if (metadata.format != DXGI_FORMAT_BC7_UNORM && metadata.format != DXGI_FORMAT_BC7_UNORM_SRGB && metadata.format != DXGI_FORMAT_BC7_TYPELESS)
        {
            std::string msg = "Error: unsupported compressed texture format for: \'" + dst.name + "\' \'" + dst.filepath + "\'\n. Compressed textures must be in BC7 format";
            Graphics::UI::MessageBox(msg);
            return false;
        }

        // Set texture data
        dst.width = static_cast<int>(metadata.width);
        dst.height = static_cast<int>(metadata.height);
        dst.stride = 1;
        dst.mips = static_cast<int>(metadata.mipLevels);
        dst.texelBytes = 0;

        // Compute the total size of the texture in bytes (including alignment).
        // Note: BC7 uses fixed block sizes of 4x4 texels with 16 bytes per block, 1 byte per texel.
        for (uint32_t mipIndex = 0; mipIndex < dst.mips; mipIndex++)
        {
            // Compute the size of the mip level and add it to the total aligned memory size
            const Image* image = src.GetImage(mipIndex, 0, 0);

            uint32_t alignedWidth = ALIGN(4, static_cast<uint32_t>(image->width));
            uint32_t alignedHeight = ALIGN(4, static_cast<uint32_t>(image->height));

            // Add the size of the last mip (one texel)
            if (dst.mips > 1 && (mipIndex + 1) == dst.mips)
            {
                dst.texelBytes += 16; // BC7 blocks are 16 bytes
                break;
            }

            // Get the aligned memory size in bytes of the mip level and add it to the texture memory total
            dst.texelBytes += GetBC7TextureSizeInBytes(alignedWidth, alignedHeight);
        }

        if (dst.texelBytes > 0)
        {
            // Delete existing texels
            if(dst.texels)
            {
                delete[] dst.texels;
                dst.texels = nullptr;
            }

            // Copy each aligned mip level to the texel array
            size_t alignedOffset = 0;
            dst.texels = new uint8_t[dst.texelBytes];
            memset(dst.texels, 0, dst.texelBytes);
            for (uint32_t mipIndex = 0; mipIndex < dst.mips; mipIndex++)
            {
                size_t offset = 0;
                const Image* image = src.GetImage(mipIndex, 0, 0);

                uint32_t alignedHeight = ALIGN(4, static_cast<uint32_t>(image->height));

                // Copy the last mip level / block
                if (dst.mips > 1 && (mipIndex + 1) == dst.mips)
                {
                    memcpy(&dst.texels[alignedOffset], &image->pixels[offset], image->rowPitch);
                    alignedOffset += image->rowPitch;
                    assert(dst.texelBytes == alignedOffset);
                    break;
                }

                // Copy each row of the mip texture, padding for alignment
                size_t numRows = alignedHeight / 4;
                for (uint32_t rowIndex = 0; rowIndex < numRows; rowIndex++)
                {
                    memcpy(&dst.texels[alignedOffset], &image->pixels[offset], image->rowPitch);
                    offset += image->rowPitch;
                    alignedOffset += ALIGN(256, image->rowPitch);
                }

                alignedOffset = ALIGN(512, alignedOffset);
            }
            result =  true;
        }

        src.Release();
        return result;
    }
#endif

    //----------------------------------------------------------------------------------------------------------
    // Public Functions
    //----------------------------------------------------------------------------------------------------------

#if defined(GPU_COMPRESSION)
    /**
     * Creates a D3D11Device for use with DirectXTex to compress textures with the GPU.
     */
    bool Initialize()
    {
        D3D_FEATURE_LEVEL requested = D3D_FEATURE_LEVEL_11_1;
        D3D_FEATURE_LEVEL supported;
        if(FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &requested, 1, D3D11_SDK_VERSION, &d3d11Device, &supported, nullptr))) return false;
        return true;
    }

    void Cleanup()
    {
        SAFE_RELEASE(d3d11Device);
    }
#endif

    /**
     * Load a texture file from disk.
     * Supports uncompressed R8G8B8A8_UNORM textures without mipmaps and BC7 compressed textures with or without mipmaps.
     */
    bool Load(Texture& texture)
    {
        if(texture.format == ETextureFormat::UNCOMPRESSED)
        {
            // Load the uncompressed texture with stb_image (require 4 component RGBA)
            texture.texels = stbi_load(texture.filepath.c_str(), (int*)&(texture.width), (int*)&texture.height, (int*)&texture.stride, STBI_rgb_alpha);
            if (!texture.texels)
            {
                std::string msg = "Error: failed to load texture: \'" + texture.name + "\' \'" + texture.filepath + "\'";
                Graphics::UI::MessageBox(msg);
                return false;
            }

            texture.stride = 4;
            texture.mips = 1;

            // Prep the texture for compression and use on the GPU
            return FormatTexture(texture);
        }
    #if defined(__x86_64__) || defined(_M_X64)
        else if(texture.format == ETextureFormat::BC7)
        {
            // Load the compressed texture from a DDS file
            ScratchImage dds = {};
            if(FAILED(LoadFromDDSFile(std::wstring(texture.filepath.begin(), texture.filepath.end()).c_str(), DDS_FLAGS_NONE, nullptr, dds)))
            {
                std::string msg = "Error: failed to load texture: \'" + texture.name + "\' \'" + texture.filepath + "\'\n.";
                Graphics::UI::MessageBox(msg);
                return false;
            }

            // Ensure the texture format is compressed as BC7
            if(dds.GetMetadata().format != DXGI_FORMAT_BC7_UNORM)
            {
                std::string msg = "Error: loaded texture is not in BC7 (UNORM) format!";
                Graphics::UI::MessageBox(msg);
                return false;
            }

            // Copy the texture into our format, prepping it for upload to the GPU
            return FormatCompressedTexture(dds, texture);
        }
    #endif
        return false;
    }

#if defined(__x86_64__) || defined(_M_X64)
    /**
     * Covert a R8G8B8A8_UNORM texture to BC7 format.
     * CAUTION: CPU-only compression is very slow, use GPU_COMPRESSION whenever possible.
     */
    bool Compress(Texture& texture, bool quick)
    {
        // Only compressed uncompressed textures
        if(texture.format != ETextureFormat::UNCOMPRESSED) return false;

        // B7 textures must be aligned to pixel 4x4 blocks
        assert(texture.width % 4 == 0);
        assert(texture.height % 4 == 0);

        Image source = {};
        source.width = texture.width;
        source.height = texture.height;
        source.rowPitch = (texture.width * texture.stride);
        source.slicePitch = (source.rowPitch * source.height);
        source.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        source.pixels = texture.texels;

        TEX_COMPRESS_FLAGS flags = TEX_COMPRESS_DEFAULT;
        if(quick) flags = TEX_COMPRESS_BC7_QUICK;

        // Compress the source image to BC7 format
        ScratchImage destination;
    #ifdef GPU_COMPRESSION
        if (FAILED(DirectX::Compress(d3d11Device, source, DXGI_FORMAT_BC7_UNORM, flags, 1.f, destination))) return false;
    #else
        flags |= TEX_COMPRESS_PARALLEL;
        if (FAILED(DirectX::Compress(source, DXGI_FORMAT_BC7_UNORM, flags, TEX_THRESHOLD_DEFAULT, destination))) return false;
    #endif

        // The image is now compressed, change the format descriptor
        texture.format = ETextureFormat::BC7;

        // Format the compressed texture into our format, prepping it for use on the GPU
        return FormatCompressedTexture(destination, texture);
    }

    /**
     * Generate the mipmap chain for the given texture, then compress the mipmap chain to BC7 format.
     * CAUTION: CPU-only compression is very slow, use GPU_COMPRESSION whenever possible.
     */
    bool MipmapAndCompress(Texture& texture, bool quick)
    {
        // DirectX::GenerateMipMaps does not support block compressed images
        if (texture.format != ETextureFormat::UNCOMPRESSED) return false;

        // B7 textures must be aligned to pixel 4x4 blocks
        assert(texture.width % 4 == 0);
        assert(texture.height % 4 == 0);

        Image source = {};
        source.width = texture.width;
        source.height = texture.height;
        source.rowPitch = (texture.width * texture.stride);
        source.slicePitch = (source.rowPitch * source.height);
        source.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        source.pixels = texture.texels;

        // Generate the mipmap chain
        ScratchImage mips;
        if (FAILED(DirectX::GenerateMipMaps(source, TEX_FILTER_DEFAULT, 0, mips))) return false;

        TEX_COMPRESS_FLAGS flags = TEX_COMPRESS_DEFAULT;
        if (quick) flags = TEX_COMPRESS_BC7_QUICK;

        // Compress the mip chain to BC7 format
        ScratchImage compressed;
    #ifdef GPU_COMPRESSION
        if (FAILED(DirectX::Compress(d3d11Device, mips.GetImages(), mips.GetImageCount(), mips.GetMetadata(), DXGI_FORMAT_BC7_UNORM, flags, 1.f, compressed))) return false;
    #else
        flags |= TEX_COMPRESS_PARALLEL;
        if (FAILED(DirectX::Compress(mips.GetImages(), mips.GetImageCount(), mips.GetMetadata(), DXGI_FORMAT_BC7_UNORM, flags, TEX_THRESHOLD_DEFAULT, compressed))) return false;
    #endif

        mips.Release();
        texture.format = ETextureFormat::BC7;

        // Format the compressed image into our format, prepping it for use on the GPU
        return FormatCompressedTexture(compressed, texture);
    }

#endif

    /**
     * Release texture memory (CPU).
     */
    void Unload(Texture& texture)
    {
        delete[] texture.texels;
        texture = {};
    }

    /**
     * Get the size (in bytes) of an aligned BC7 compressed texture.
     * This matches the size returned by D3D12Device->GetCopyableFootprints(...).
     * https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-getcopyablefootprints
     */
    uint32_t GetBC7TextureSizeInBytes(uint32_t width, uint32_t height)
    {
        uint32_t numRows = height / 4;
        uint32_t rowPitch = ALIGN(16, width * 4);
        return ALIGN(512, numRows * ALIGN(256, rowPitch));
    }

} //namespace Textures
