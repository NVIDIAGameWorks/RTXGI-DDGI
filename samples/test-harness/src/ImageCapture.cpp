/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "ImageCapture.h"
#include "Common.h"

#if defined(_WIN32) || defined(WIN32)
#define STBI_MSC_SECURE_CRT
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace ImageCapture
{

    /**
     * Write image data to a PNG format file.
     */
    bool CapturePng(std::string file, uint32_t width, uint32_t height, const unsigned char* data)
    {
        int result = stbi_write_png(file.c_str(), width, height, NumChannels, data, width * NumChannels);
        return result != 0;
    }

#if defined(_WIN32) || defined(WIN32)
    /**
     * Create a Windows Image Component (WIC) imaging factory.
     */
    IWICImagingFactory2* CreateWICImagingFactory()
    {
        static INIT_ONCE s_initOnce = INIT_ONCE_STATIC_INIT;

        IWICImagingFactory2* factory = nullptr;
        (void)InitOnceExecuteOnce(&s_initOnce,
            [](PINIT_ONCE, PVOID, PVOID* ifactory) -> BOOL
            {
                return SUCCEEDED(CoCreateInstance(
                    CLSID_WICImagingFactory2,
                    nullptr,
                    CLSCTX_INPROC_SERVER,
                    __uuidof(IWICImagingFactory2),
                    ifactory)) ? TRUE : FALSE;
            }, nullptr, reinterpret_cast<LPVOID*>(&factory));

        return factory;
    }

    /**
     * Convert the data format of a D3D resource using Windows Imaging Component (WIC).
     */
    HRESULT ConvertTextureResource(
        const D3D12_RESOURCE_DESC desc,
        UINT64 imageSize,
        UINT64 dstRowPitch,
        unsigned char* pMappedMemory,
        std::vector<unsigned char>& converted)
    {
        bool sRGB = false;
        WICPixelFormatGUID pfGuid;

        // Determine source format's WIC equivalent
        switch (desc.Format)
        {
            case DXGI_FORMAT_R32G32B32A32_FLOAT:            pfGuid = GUID_WICPixelFormat128bppRGBAFloat; break;
            case DXGI_FORMAT_R16G16B16A16_FLOAT:            pfGuid = GUID_WICPixelFormat64bppRGBAHalf; break;
            case DXGI_FORMAT_R16G16B16A16_UNORM:            pfGuid = GUID_WICPixelFormat64bppRGBA; break;
            case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:    pfGuid = GUID_WICPixelFormat32bppRGBA1010102XR; break;
            case DXGI_FORMAT_R10G10B10A2_UNORM:             pfGuid = GUID_WICPixelFormat32bppRGBA1010102; break;
            case DXGI_FORMAT_B5G5R5A1_UNORM:                pfGuid = GUID_WICPixelFormat16bppBGRA5551; break;
            case DXGI_FORMAT_B5G6R5_UNORM:                  pfGuid = GUID_WICPixelFormat16bppBGR565; break;
            case DXGI_FORMAT_R32_FLOAT:                     pfGuid = GUID_WICPixelFormat32bppGrayFloat; break;
            case DXGI_FORMAT_R16_FLOAT:                     pfGuid = GUID_WICPixelFormat16bppGrayHalf; break;
            case DXGI_FORMAT_R16_UNORM:                     pfGuid = GUID_WICPixelFormat16bppGray; break;
            case DXGI_FORMAT_R8_UNORM:                      pfGuid = GUID_WICPixelFormat8bppGray; break;
            case DXGI_FORMAT_A8_UNORM:                      pfGuid = GUID_WICPixelFormat8bppAlpha; break;
            case DXGI_FORMAT_R8G8B8A8_UNORM:                pfGuid = GUID_WICPixelFormat32bppRGBA; break;
            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:           pfGuid = GUID_WICPixelFormat32bppRGBA; sRGB = true; break;
            case DXGI_FORMAT_B8G8R8A8_UNORM:                pfGuid = GUID_WICPixelFormat32bppBGRA; break;
            case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:           pfGuid = GUID_WICPixelFormat32bppBGRA; sRGB = true; break;
            case DXGI_FORMAT_B8G8R8X8_UNORM:                pfGuid = GUID_WICPixelFormat32bppBGR; break;
            case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:           pfGuid = GUID_WICPixelFormat32bppBGR; sRGB = true; break;
                // WIC does not have two-channel formats, four-channel lets us output all data for bitwise comparisons
            case DXGI_FORMAT_R32G32_FLOAT:                  pfGuid = GUID_WICPixelFormat128bppRGBAFloat; break;
            default:
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        // Create an imaging factory
        IWICImagingFactory2* pWIC = CreateWICImagingFactory();

        // Create a WIC bitmap from the D3D resource
        IWICBitmap* bitmap = nullptr;
        HRESULT hr = pWIC->CreateBitmapFromMemory(
            static_cast<UINT>(desc.Width),
            static_cast<UINT>(desc.Height),
            pfGuid,
            static_cast<UINT>(dstRowPitch),
            static_cast<UINT>(imageSize),
            static_cast<BYTE*>(pMappedMemory),
            &bitmap);

        if(FAILED(hr)) return hr;

        // Create the WIC converter
        IWICFormatConverter* converter = nullptr;
        hr = pWIC->CreateFormatConverter(&converter);
        if(FAILED(hr))
        {
            SAFE_RELEASE(bitmap);
            return hr;
        }

        // Initialize the WIC converter
        hr = converter->Initialize(bitmap, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.f, WICBitmapPaletteTypeCustom);
        if(FAILED(hr))
        {
            SAFE_RELEASE(converter);
            SAFE_RELEASE(bitmap);
            return hr;
        }

        // Convert the texels
        WICRect rect = { 0, 0, static_cast<INT>(desc.Width), static_cast<INT>(desc.Height) };
        hr = converter->CopyPixels(&rect, static_cast<UINT>(desc.Width * 4), static_cast<UINT>(converted.size()), converted.data());

        // Clean up
        SAFE_RELEASE(converter);
        SAFE_RELEASE(bitmap);

        return hr;
    }

#endif

}
