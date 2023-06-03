#include "PCX.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>

enum {
    PCX_VERSION_2_5 = 0,
    PCX_VERSION_NOT_VALID = 1,
    PCX_VERSION_2_8_WITH_PALLETE = 2,
    PCX_VERSION_2_8_WITHOUT_PALLETE = 3,
    PCX_VERSION_WINDOWS = 4,
    PCX_VERSION_3_0 = 5
};

#pragma pack(push, 1)
struct PCXHeader {
    int8_t manufacturer;
    int8_t version;
    int8_t compression;
    int8_t bpp;
    int16_t xmin;
    int16_t ymin;
    int16_t xmax;
    int16_t ymax;
    int16_t hdpi;
    int16_t vdpi;
    int8_t pallete[48];
    int8_t reserved;
    int8_t nplanes;
    int16_t bytes_per_row;
    int16_t palette_info;
    int16_t hres;
    int16_t vres;
    int8_t filler[54];
};
#pragma pack(pop)

typedef struct bstreamer {
    const uint8_t *buffer, *buffer_end, *buffer_start;
} bstreamer;

static inline void bs_init(bstreamer *bs, const uint8_t *buf, int buf_size) {
    bs->buffer = bs->buffer_start = buf;
    bs->buffer_end = buf + buf_size;
}

static inline int bs_get_bytes_left(bstreamer *bs) {
    return bs->buffer_end - bs->buffer;
}

static inline unsigned int bs_get_byte(bstreamer *bs) {
    unsigned int byte;
    if (bs_get_bytes_left(bs) > 0) {
        byte = bs->buffer[0];
        bs->buffer++;
        return byte;
    }

    return 0;
}

static inline unsigned int bs_get_buffer(bstreamer *bs, uint8_t *dst, unsigned int size) {
    int size_min = std::min((unsigned int)(bs->buffer_end - bs->buffer), size);
    memcpy(dst, bs->buffer, size_min);
    bs->buffer += size_min;
    return size_min;
}

static int pcx_rle_decode(bstreamer *bs, uint8_t *dst, unsigned int bytes_per_scanline, int compressed) {
    unsigned int i = 0;
    unsigned char run, value;

    if (bs_get_bytes_left(bs) < 1)
        return -1;

    if (compressed) {
        while (i < bytes_per_scanline && bs_get_bytes_left(bs) > 0) {
            run = 1;
            value = bs_get_byte(bs);
            if (value >= 0xc0 && bs_get_bytes_left(bs) > 0) {
                run = value & 0x3f;
                value = bs_get_byte(bs);
            }
            while (i < bytes_per_scanline && run--)
                dst[i++] = value;
        }
    } else {
        bs_get_buffer(bs, dst, bytes_per_scanline);
    }
    return 0;
}

std::unique_ptr<Color[]> PCX::Decode(const Blob &data, size_t *width, size_t *height) {
    if (data.size() < sizeof(PCXHeader))
        return nullptr;

    const PCXHeader *header = static_cast<const PCXHeader *>(data.data());

    // check that's PCX and its version
    if (header->manufacturer != 0x0a || header->version < PCX_VERSION_2_5 || header->version == PCX_VERSION_NOT_VALID || header->version > PCX_VERSION_3_0) {
        return nullptr;
    }

    *width = header->xmax - header->xmin + 1;
    *height = header->ymax - header->ymin + 1;

    unsigned int bytes_per_scanline = header->nplanes * header->bytes_per_row;

    //corruption check
    if (bytes_per_scanline < (*width * header->bpp * header->nplanes + 7) / 8 ||
        (!header->compression && bytes_per_scanline > (data.size() - sizeof(PCXHeader)) / *height)) {
        return nullptr;
    }

    switch ((header->nplanes << 8) + header->bpp) {
    case 0x0308:
        break;
    case 0x0108:
    case 0x0104:
    case 0x0102:
    case 0x0101:
    case 0x0401:
    case 0x0301:
    case 0x0201:
        // TODO: PAL8 color mode
        // fall through
    default:
        return nullptr;
    }

    size_t pixel_count = *width * *height;

    std::unique_ptr<Color[]> pixels(new Color[pixel_count]);
    if (!pixels)
        return nullptr;

    memset(pixels.get(), 0, pixel_count * sizeof(Color));

    bstreamer bs;
    unsigned int stride = 0;
    std::unique_ptr<uint8_t[], FreeDeleter> scanline(static_cast<uint8_t *>(malloc(bytes_per_scanline + 32)));
    bs_init(&bs, static_cast<const uint8_t *>(data.data()) + sizeof(PCXHeader), data.size() - sizeof(PCXHeader));

    if (header->nplanes == 3 && header->bpp == 8) {
        for (unsigned int y = 0; y < *height; y++) {
            int ret = pcx_rle_decode(&bs, scanline.get(), bytes_per_scanline, header->compression);
            if (ret < 0)
                return nullptr;

            for (unsigned int x = 0; x < *width; x++)
                pixels[stride + x] = Color(scanline[x], scanline[x + header->bytes_per_row], scanline[x + (header->bytes_per_row << 1)]);

            stride += *width;
        }
    } else {
        // TODO: other planes/bpp variants
        return nullptr;
    }

    return pixels;
}

void *WritePCXHeader(void *pcx_data, int width, int height) {
    int pitch = width;
    if (width & 1) {
        pitch = width + 1;
    }

    PCXHeader *header = static_cast<PCXHeader *>(pcx_data);
    memset(header, 0, sizeof(PCXHeader));
    header->xmin = 0;
    header->ymin = 0;
    header->xmax = width - 1;
    header->ymax = height - 1;
    header->bytes_per_row = pitch;
    header->manufacturer = 10;
    header->version = 5;
    header->compression = 1;
    header->bpp = 8;
    header->hdpi = 75;
    header->vdpi = 75;
    header->nplanes = 3;
    header->palette_info = 1;

    return static_cast<uint8_t *>(pcx_data) + sizeof(PCXHeader);
}

void *EncodeOneLine(void *pcx_data, void *line, size_t line_size) {
    uint8_t *input = (uint8_t *)line;
    uint8_t *end = input + line_size;
    uint8_t *output = (uint8_t *)pcx_data;

    while (input < end) {
        uint8_t value = *input++;

        int count = 1;
        while (count < 63 && input < end && *input == value) {
            input++;
            count++;
        }

        if (count > 1 || (value & 0xC0) != 0)
            *output++ = 0xC0 + count;
        *output++ = value;
    }

    return output;
}

Blob PCX::Encode(const Color *data, size_t width, size_t height) {
    assert(data != nullptr && width != 0 & height != 0);

    // pcx lines are padded to next even byte boundary
    int pitch = width;
    if (width & 1) {
        pitch = width + 1;
    }

    // pcx file can be larger than uncompressed
    // pcx header and no compression @24bit worst case doubles in size
    size_t worstCase = sizeof(PCXHeader) + 3 * pitch * height * 2;
    std::unique_ptr<uint8_t[], FreeDeleter> pcx_data(static_cast<uint8_t *>(malloc(worstCase)));

    uint8_t *output = (uint8_t *)WritePCXHeader(pcx_data.get(), width, height);

    std::unique_ptr<uint8_t[]> lineRGB(new uint8_t[3 * pitch]);
    uint8_t *lineR = lineRGB.get();
    uint8_t *lineG = lineRGB.get() + pitch;
    uint8_t *lineB = lineRGB.get() + 2 * pitch;
    const Color *input = data;

    for (int y = 0; y < height; y++) {
        for (unsigned int x = 0; x < width; x++) {
            Color pixel = *input++;
            lineR[x] = pixel.r;
            lineG[x] = pixel.g;
            lineB[x] = pixel.b;
        }
        uint8_t *line = lineRGB.get();
        for (int p = 0; p < 3; p++) {
            output = (uint8_t *)EncodeOneLine(output, line, pitch);
            line += pitch;
        }
    }

    size_t packed_size = output - pcx_data.get();
    assert(packed_size <= worstCase);
    return Blob::fromMalloc(pcx_data.release(), packed_size);
}