#include "Engine/Graphics/ImageLoader.h"

#include <filesystem>
#include <string>
#include <SDL_image.h>

#include "Engine/Log.h"

#include "Engine/Engine.h"
#include "Engine/ZlibWrapper.h"

#include "Engine/ErrorHandling.h"
#include "Engine/Graphics/HWLContainer.h"
#include "Engine/Graphics/IRender.h"
#include "Engine/Graphics/ImageFormatConverter.h"
#include "Engine/Graphics/PCX.h"
#include "Engine/Graphics/Sprites.h"

#include "Platform/Api.h"


uint32_t *MakeImageSolid(unsigned int width, unsigned int height,
                         uint8_t *pixels, uint8_t *palette) {
    uint32_t *res = new uint32_t[width * height];

    for (unsigned int y = 0; y < height; ++y) {
        for (unsigned int x = 0; x < width; ++x) {
            int index = pixels[y * width + x];
            auto r = palette[(index * 3) + 0];
            auto g = palette[(index * 3) + 1];
            auto b = palette[(index * 3) + 2];
            res[y * width + x] = Color32(r, g, b);
        }
    }

    return res;
}

uint32_t *MakeImageAlpha(unsigned int width, unsigned int height,
                         uint8_t *pixels, uint8_t *palette) {
    uint32_t *res = new uint32_t[width * height];

    for (unsigned int y = 0; y < height; ++y) {
        for (unsigned int x = 0; x < width; ++x) {
            int index = pixels[y * width + x];
            auto r = palette[(index * 3) + 0];
            auto g = palette[(index * 3) + 1];
            auto b = palette[(index * 3) + 2];
            if (index == 0) {
                res[y * width + x] = 0x00000000;
            } else {
                res[y * width + x] = Color32(r, g, b);
            }
        }
    }

    return res;
}

uint32_t *MakeImageColorKey(unsigned int width, unsigned int height,
                            uint8_t *pixels, uint8_t *palette,
                            uint16_t color_key) {
    uint32_t *res = new uint32_t[width * height];

    for (unsigned int y = 0; y < height; ++y) {
        for (unsigned int x = 0; x < width; ++x) {
            int index = pixels[y * width + x];
            auto r = palette[(index * 3) + 0];
            auto g = palette[(index * 3) + 1];
            auto b = palette[(index * 3) + 2];
            if (Color16(r, g, b) == color_key) {
                res[y * width + x] = 0x00000000;
            } else {
                res[y * width + x] = Color32(r, g, b);
            }
        }
    }

    return res;
}

uint32_t* MakeImageOverride24(uint width, uint height, uint8_t* pixels) {
    // pallette override
    uint32_t* res = new uint32_t[width * height];

    for (unsigned int y = 0; y < height; ++y) {
        for (unsigned int x = 0; x < width; ++x) {
            uint8_t b = pixels[(y * width * 3) + (x * 3)];
            uint8_t g = pixels[(y * width * 3) + (x * 3) + 1];
            uint8_t r = pixels[(y * width * 3) + (x * 3) + 2];
            res[y * width + x] = (0xFF << 24 | r << 16 | g << 8 | b);
        }
    }
    return res;
}

uint32_t* MakeImageOverride24Alpha(uint width, uint height, uint8_t* pixels) {
    // pallette override
    uint32_t* res = new uint32_t[width * height];

    for (unsigned int y = 0; y < height; ++y) {
        for (unsigned int x = 0; x < width; ++x) {
            uint8_t b = pixels[(y * width * 3) + (x * 3)];
            uint8_t g = pixels[(y * width * 3) + (x * 3) + 1];
            uint8_t r = pixels[(y * width * 3) + (x * 3) + 2];
            uint8_t a = 0xFF;
            if ((b == 0) & (g == 0) & (r == 0)) a = 0;
            res[y * width + x] = (a << 24 | r << 16 | g << 8 | b);
        }
    }
    return res;
}

uint32_t* MakeImageOverride32(uint width, uint height, uint8_t* pixels) {
    // pallette override
    uint32_t* res = new uint32_t[width * height];
    memcpy(res, pixels, width * height * 4);
    return res;
}


bool ColorKey_LOD_Loader::Load(unsigned int *out_width, unsigned int *out_height, void **out_pixels, IMAGE_FORMAT *out_format) {
    *out_width = 0;
    *out_height = 0;
    *out_pixels = nullptr;
    *out_format = IMAGE_INVALID_FORMAT;

    Texture_MM7 *tex = lod->GetTexture(
        lod->LoadTexture(resource_name, TEXTURE_24BIT_PALETTE));
    if ((tex == nullptr) || (tex->pPalette24 == nullptr) ||
        (tex->paletted_pixels == nullptr)) {
        return false;
    }

    if (tex->header.pBits & 0x1000) {  // pallette override 32
        *out_pixels = MakeImageOverride32(tex->header.uTextureWidth, tex->header.uTextureHeight, tex->paletted_pixels);
    } else if (tex->header.pBits & 0x0800) {  // pallette override 24
        *out_pixels = MakeImageOverride24(tex->header.uTextureWidth, tex->header.uTextureHeight, tex->paletted_pixels);
    } else if (tex->header.pBits & 512) {
        *out_pixels = MakeImageAlpha(tex->header.uTextureWidth, tex->header.uTextureHeight,
                                     tex->paletted_pixels, tex->pPalette24);
    } else {
        *out_pixels = MakeImageColorKey(tex->header.uTextureWidth, tex->header.uTextureHeight,
            tex->paletted_pixels, tex->pPalette24, colorkey);
    }

    if (*out_pixels == nullptr) {
        return false;
    }

    *out_width = tex->header.uTextureWidth;
    *out_height = tex->header.uTextureHeight;
    *out_format = IMAGE_FORMAT_A8R8G8B8;

    return true;
}

bool Image16bit_LOD_Loader::Load(unsigned int *out_width, unsigned int *out_height, void **out_pixels, IMAGE_FORMAT *out_format) {
    *out_width = 0;
    *out_height = 0;
    *out_pixels = nullptr;
    *out_format = IMAGE_INVALID_FORMAT;

    Texture_MM7 *tex = lod->GetTexture(
        lod->LoadTexture(resource_name, TEXTURE_24BIT_PALETTE));
    if ((tex == nullptr) || (tex->pPalette24 == nullptr) ||
        (tex->paletted_pixels == nullptr)) {
        return false;
    }

    if (tex->header.pBits & 0x1000) {  // pallette override 32
        *out_pixels = MakeImageOverride32(tex->header.uTextureWidth, tex->header.uTextureHeight, tex->paletted_pixels);
    } else if (tex->header.pBits & 0x0800) {  // pallette override 24
        *out_pixels = MakeImageOverride24(tex->header.uTextureWidth, tex->header.uTextureHeight, tex->paletted_pixels);
    } else if (tex->header.pBits & 512) {
        *out_pixels = MakeImageAlpha(tex->header.uTextureWidth, tex->header.uTextureHeight,
                                     tex->paletted_pixels, tex->pPalette24);
    } else {
        *out_pixels = MakeImageSolid(tex->header.uTextureWidth, tex->header.uTextureHeight,
                                     tex->paletted_pixels, tex->pPalette24);
    }

    if (*out_pixels == nullptr) {
        return false;
    }

    *out_width = tex->header.uTextureWidth;
    *out_height = tex->header.uTextureHeight;
    *out_format = IMAGE_FORMAT_A8R8G8B8;

    return true;
}

bool Alpha_LOD_Loader::Load(unsigned int *out_width, unsigned int *out_height,
                            void **out_pixels, IMAGE_FORMAT *out_format) {
    *out_width = 0;
    *out_height = 0;
    *out_pixels = nullptr;
    *out_format = IMAGE_INVALID_FORMAT;

    Texture_MM7 *tex = lod->GetTexture(
        lod->LoadTexture(resource_name, TEXTURE_24BIT_PALETTE));
    if ((tex == nullptr) || (tex->pPalette24 == nullptr) ||
        (tex->paletted_pixels == nullptr)) {
        return false;
    }

    if (tex->header.pBits & 0x1000) {  // pallette override 32
        *out_pixels = MakeImageOverride32(tex->header.uTextureWidth, tex->header.uTextureHeight, tex->paletted_pixels);
    } else if (tex->header.pBits & 0x0800) {  // pallette override 24
        *out_pixels = MakeImageOverride24(tex->header.uTextureWidth, tex->header.uTextureHeight, tex->paletted_pixels);
    } else if ((tex->header.pBits == 0) || (tex->header.pBits & 512)) {
        *out_pixels = MakeImageAlpha(tex->header.uTextureWidth, tex->header.uTextureHeight,
                                     tex->paletted_pixels, tex->pPalette24);
    } else {
        *out_pixels = MakeImageColorKey(
            tex->header.uTextureWidth, tex->header.uTextureHeight,
            tex->paletted_pixels, tex->pPalette24, render->teal_mask_16);
    }

    if (*out_pixels == nullptr) {
        return false;
    }

    *out_width = tex->header.uTextureWidth;
    *out_height = tex->header.uTextureHeight;
    *out_format = IMAGE_FORMAT_A8R8G8B8;

    return true;
}

bool PCX_Loader::InternalLoad(void *file, size_t filesize,
                                   unsigned int *width, unsigned int *height,
                                   void **pixels, IMAGE_FORMAT *format) {
    IMAGE_FORMAT request_format = IMAGE_FORMAT_R8G8B8A8;
    if (engine->config->renderer_name == "DirectDraw")
        request_format = IMAGE_FORMAT_R5G6B5;

    *pixels = PCX::Decode(file, filesize, width, height, format, request_format);
    if (*pixels)
        return true;

    return false;
}

bool PCX_File_Loader::Load(unsigned int *width, unsigned int *height,
                           void **pixels, IMAGE_FORMAT *format) {
    *width = 0;
    *height = 0;
    *pixels = nullptr;
    *format = IMAGE_INVALID_FORMAT;

    FILE *file = fopen(this->resource_name.c_str(), "rb");
    if (!file) {
        log->Warning("Unable to load %s", this->resource_name.c_str());
        return false;
    }

    fseek(file, 0, SEEK_END);
    size_t filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    uint8_t *buffer = (uint8_t *)malloc(filesize);

    fread(buffer, filesize, 1, file);

    bool res = InternalLoad(buffer, filesize, width, height, pixels, format);

    free(buffer);

    fclose(file);

    return res;
}

bool PCX_LOD_Raw_Loader::Load(unsigned int *width, unsigned int *height,
                              void **pixels, IMAGE_FORMAT *format) {
    *width = 0;
    *height = 0;
    *pixels = nullptr;
    *format = IMAGE_INVALID_FORMAT;

    size_t size;
    void *data = lod->LoadRaw(resource_name, &size);
    if (data == nullptr) {
        log->Warning("Unable to load %s", this->resource_name.c_str());
        return false;
    }

    bool res = InternalLoad(data, size, width, height, pixels, format);

    free(data);

    return res;
}

bool PCX_LOD_Compressed_Loader::Load(unsigned int *width, unsigned int *height,
                                     void **pixels, IMAGE_FORMAT *format) {
    *width = 0;
    *height = 0;
    *pixels = nullptr;
    *format = IMAGE_INVALID_FORMAT;

    size_t data_size = 0;
    void *pcx_data = lod->LoadCompressedTexture(resource_name, &data_size);
    if (pcx_data == nullptr) {
        log->Warning("Unable to load %s", resource_name.c_str());
        return false;
    }

    bool res = InternalLoad(pcx_data, data_size, width, height, pixels, format);

    free(pcx_data);

    return res;
}

static void ProcessTransparentPixel(uint8_t* pixels, uint8_t* palette,
                                    size_t x, size_t y, size_t w, size_t h, uint8_t* bgra) {
    size_t count = 0;
    size_t r = 0, g = 0, b = 0;

    auto processPixel = [&](size_t x, size_t y) {
        int pal = pixels[y * w + x];
        if (pal != 0) {
            count++;
            r += palette[3 * pal + 0];
            g += palette[3 * pal + 1];
            b += palette[3 * pal + 2];
        }
    };

    bool canDecX = x > 0;
    bool canIncX = x < w - 1;
    bool canDecY = y > 0;
    bool canIncY = y < h - 1;

    if (canDecX & canDecY)
        processPixel(x - 1, y - 1);
    if (canDecX)
        processPixel(x - 1, y);
    if (canDecX & canIncY)
        processPixel(x - 1, y + 1);
    if (canDecY)
        processPixel(x, y - 1);
    if (canIncY)
        processPixel(x, y + 1);
    if (canIncX & canDecY)
        processPixel(x + 1, y - 1);
    if (canIncX)
        processPixel(x + 1, y);
    if (canIncX & canIncY)
        processPixel(x + 1, y + 1);

    if (count != 0) {
        r /= count;
        g /= count;
        b /= count;
    }

    bgra[0] = static_cast<uint8_t>(b);
    bgra[1] = static_cast<uint8_t>(g);
    bgra[2] = static_cast<uint8_t>(r);
    bgra[3] = 0;
}

bool Bitmaps_LOD_Loader::Load(unsigned int *width, unsigned int *height,
                              void **out_pixels, IMAGE_FORMAT *format) {
    Texture_MM7 *tex = lod->GetTexture(lod->LoadTexture(this->resource_name));
    int num_pixels = tex->header.uTextureWidth * tex->header.uTextureHeight;

    if (tex->header.pBits & 2) {  // hardware bitmap
        if (!this->use_hwl) {
            Assert(tex->paletted_pixels);
            Assert(tex->pPalette24);

            uint8_t* pixels = new uint8_t[num_pixels * 4];
            size_t w = tex->header.uTextureWidth;
            size_t h = tex->header.uTextureHeight;

            for (size_t y = 0; y < h; y++) {
                for (size_t x = 0; x < w; x++) {
                    size_t p = y * w + x;

                    int pal = tex->paletted_pixels[p];
                    if (pal != 0) {
                        pixels[p * 4 + 0] = tex->pPalette24[3 * pal + 2];
                        pixels[p * 4 + 1] = tex->pPalette24[3 * pal + 1];
                        pixels[p * 4 + 2] = tex->pPalette24[3 * pal + 0];
                        pixels[p * 4 + 3] = 255;
                    } else {
                        ProcessTransparentPixel(tex->paletted_pixels, tex->pPalette24, x, y, w, h, &pixels[p * 4]);
                    }
                }
            }

            *format = IMAGE_FORMAT_A8R8G8B8;
            *width = tex->header.uTextureWidth;
            *height = tex->header.uTextureHeight;
            *out_pixels = pixels;
            return true;
        } else {
            uint16_t* pixels = new uint16_t[num_pixels];

            HWLTexture* hwl = render->LoadHwlBitmap(this->resource_name);
            if (hwl) {
                // linear scaling
                for (int s = 0; s < tex->header.uTextureHeight; ++s) {
                    for (int t = 0; t < tex->header.uTextureWidth; ++t) {
                        unsigned int resampled_x = t * hwl->uWidth / tex->header.uTextureWidth,
                            resampled_y = s * hwl->uHeight / tex->header.uTextureHeight;
                        unsigned short sample = hwl->pPixels[resampled_y * hwl->uWidth + resampled_x];

                        pixels[s * tex->header.uTextureWidth + t] = sample;
                    }
                }

                delete[] hwl->pPixels;
                delete hwl;
            }

            *format = IMAGE_FORMAT_A1R5G5B5;
            *width = tex->header.uTextureWidth;
            *height = tex->header.uTextureHeight;
            *out_pixels = pixels;
            return true;
        }
    }

    *width = 0;
    *height = 0;
    *format = IMAGE_INVALID_FORMAT;
    *out_pixels = nullptr;
    return false;
}

bool Sprites_LOD_Loader::Load(unsigned int *width, unsigned int *height,
                              void **out_pixels, IMAGE_FORMAT *format) {
    *width = 0;
    *height = 0;
    *out_pixels = nullptr;
    *format = IMAGE_INVALID_FORMAT;


    SDL_Surface* surf_temp = NULL;

    std::string asspath = MakeDataPath("new assets");
    // scan through directory
    for (const auto& entry : std::filesystem::directory_iterator(asspath)) {
        if (entry.exists()) {
            // check against file
            std::string file = entry.path().filename().string();
            if (file.find(this->resource_name.c_str()) != std::string::npos) {
                // try to load
                std::string filepath = entry.path().string();
                surf_temp = IMG_Load(filepath.c_str());

                if (surf_temp == NULL) logger->Info(SDL_GetError());

                if (surf_temp) {
                    if (surf_temp->format->BytesPerPixel == 4)
                        *out_pixels = MakeImageOverride32(surf_temp->w, surf_temp->h, (uint8_t*)surf_temp->pixels);

                    if (surf_temp->format->BytesPerPixel == 3)
                        *out_pixels = MakeImageOverride24Alpha(surf_temp->w, surf_temp->h, (uint8_t*)surf_temp->pixels);


                    *width = surf_temp->w;
                    *height = surf_temp->h;
                    *format = IMAGE_FORMAT_A8R8G8B8;
                    SDL_FreeSurface(surf_temp);
                    return true;
                }
            }
        }
    }

    // if not continue as usuall..


    HWLTexture *hwl = render->LoadHwlSprite(this->resource_name.c_str());
    if (hwl) {
        int dst_width = hwl->uWidth;
        int dst_height = hwl->uHeight;

        int num_pixels = dst_width * dst_height;
        auto pixels = new uint16_t[num_pixels];
        if (pixels) {
            // linear scaling
            for (int s = 0; s < dst_height; ++s) {
                for (int t = 0; t < dst_width; ++t) {
                    unsigned int resampled_x = t * hwl->uWidth / dst_width,
                                 resampled_y = s * hwl->uHeight / dst_height;

                    unsigned short sample =
                        hwl->pPixels[resampled_y * hwl->uWidth + resampled_x];

                    pixels[s * dst_width + t] = sample;
                }
            }

            delete[] hwl->pPixels;
            delete hwl;

            *width = dst_width;
            *height = dst_height;
            *format = IMAGE_FORMAT_A1R5G5B5;
        }

        *out_pixels = pixels;
        return true;
    }

    return false;
}
