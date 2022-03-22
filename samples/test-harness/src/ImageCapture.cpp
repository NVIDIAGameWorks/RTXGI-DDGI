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
#include <png.h>
#include <vector>

namespace ImageCapture
{
    bool CapturePng(std::string file, uint32_t width, uint32_t height, std::vector<unsigned char*>& rows)
    {
        FILE* fp = nullptr;
        errno_t ferror = fopen_s(&fp, file.c_str(), "wb");
        if (ferror != 0)
        {
            return false;
        }

        png_structp pngWrite = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!pngWrite)
        {
            return false;
        }
        png_infop pngInfo = png_create_info_struct(pngWrite);
        if (!pngInfo)
        {
            png_destroy_write_struct(&pngWrite, (png_infopp)nullptr);
            return false;
        }

        if (setjmp(png_jmpbuf(pngWrite)))
        {
            png_destroy_write_struct(&pngWrite, &pngInfo);
            fclose(fp);
            return false;
        }

        png_init_io(pngWrite, fp);
        png_set_IHDR(
            pngWrite,
            pngInfo,
            width,
            height,
            8,
            PNG_COLOR_TYPE_RGB,
            PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT,
            PNG_FILTER_TYPE_DEFAULT
        );
        png_write_info(pngWrite, pngInfo); 
        // stripping alpha channel for captured images
        // too bad if there's useful data there...
        png_set_filler(pngWrite, 0, PNG_FILLER_AFTER);
        png_write_image(pngWrite, rows.data());
        png_write_end(pngWrite, NULL);
        fclose(fp);
        png_destroy_write_struct(&pngWrite, &pngInfo);

        return true;
    }
}