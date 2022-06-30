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

#include <stdint.h>
#include <string>

#if defined(_WIN32) || defined(WIN32)
#include <wincodec.h>
#include <d3d12.h>
#include <vector>
#endif

namespace ImageCapture
{
    const static uint32_t NumChannels = 4;
    bool CapturePng(std::string file, uint32_t width, uint32_t height, const unsigned char* data);

#if defined(_WIN32) || defined(WIN32)
    IWICImagingFactory2* CreateWICImagingFactory();
    HRESULT ConvertTextureResource(const D3D12_RESOURCE_DESC desc, UINT64 imageSize, UINT64 dstRowPitch, unsigned char* pMappedMemory, std::vector<unsigned char>& converted);
#endif

}
