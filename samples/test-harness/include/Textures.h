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

#include "Common.h"

namespace Textures
{
    enum class ETextureType
    {
        ENGINE = 0,
        SCENE,
    };

    enum class ETextureFormat
    {
        UNCOMPRESSED = 0,
        BC7,
    };

    struct Texture
    {
        std::string name = "";
        std::string filepath = "";

        ETextureType type = ETextureType::SCENE;
        ETextureFormat format = ETextureFormat::UNCOMPRESSED;

        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t stride = 0;
        uint32_t mips = 0;

        uint64_t texelBytes = 0;    // the number of bytes (aligned, all mips)
        uint8_t* texels = nullptr;

        bool cached = false;

        void SetName(std::string n)
        {
            if (strcmp(name.c_str(), "") == 0) { name = n; }
        }
    };

#if defined(GPU_COMPRESSION)
    bool Initialize();
    void Cleanup();
#endif

    bool Load(Texture& texture);
    void Unload(Texture& texture);
    uint32_t GetBC7TextureSizeInBytes(uint32_t width, uint32_t height);
#if defined(__x86_64__) || defined(_M_X64)
    bool Compress(Texture& texture, bool quick = false);
    bool MipmapAndCompress(Texture& texture, bool quick = false);
#endif

}
