/*
 
 MIT License
 
 Copyright (c) 2017 Chevy Ray Johnston
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 
*/

#include "bitmap.hpp"
#include <iostream>
#define LODEPNG_NO_COMPILE_CPP
#include "lodepng.h"
#include <algorithm>
#include "hash.hpp"
#include "time.hpp"

using namespace std;

Bitmap::Bitmap(const string& file, const string& name, bool premultiply, bool trim, bool verbose)
    : frameIndex(0), name(name), label(""), loopDirection(0), duration(0), palette(nullptr), paletteSize(0), paletteSlot(0)
{
    LodePNGState state;
    unsigned char* png = NULL;
    size_t size = 0;

    lodepng_state_init(&state);

    state.decoder.color_convert = 0;

    if (lodepng_load_file(&png, &size, file.data()))
    {
        cerr << "failed to load png: " << file << endl;
        ::exit(EXIT_FAILURE);
    }

    if (!DecodePng(&state, png, size, premultiply, trim, verbose))
    {
        cerr << "failed to load png: " << file << endl;
        ::exit(EXIT_FAILURE);
    }

    lodepng_state_cleanup(&state);
}

Bitmap::Bitmap(int frameIndex, const string& name, const string& label, int loopDirection, int duration, LodePNGState* state, unsigned char* png, size_t size, bool premultiply, bool trim, bool verbose)
    : frameIndex(frameIndex), name(name), label(label), loopDirection(loopDirection), duration(duration), palette(nullptr), paletteSize(0), paletteSlot(0)
{
    if (!DecodePng(state, png, size, premultiply, trim, verbose))
    {
		cerr << "failed to load png: " << name << endl;
		::exit(EXIT_FAILURE);
	}
}

Bitmap::Bitmap(int width, int height, uint32_t * palette, int paletteSize)
    : frameIndex(0), name(""), label(""), loopDirection(0), duration(0), width(width), height(height), palette(nullptr), paletteSize(paletteSize), paletteSlot(0)
{
    if (this->paletteSize > 0)
    {
        this->palette = reinterpret_cast<uint32_t*>(calloc(paletteSize, sizeof(uint32_t)));

        if (this->palette)
        {
            memcpy(this->palette, palette, this->paletteSize * sizeof(uint32_t));
        }
        else
        {
            cerr << "failed to allocate palette" << endl;
            exit(EXIT_FAILURE);
        }

        data = reinterpret_cast<uint8_t*>(calloc(width * height, sizeof(uint8_t)));
    }
    else
        data = reinterpret_cast<uint8_t*>(calloc(width * height, sizeof(uint32_t)));
}

bool Bitmap::DecodePng(LodePNGState *state, unsigned char* png, size_t size, bool premultiply, bool trim, bool verbose)
{
    unsigned char* buffer;
    unsigned int pw, ph;
    int result;

    result = lodepng_decode(&buffer, &pw, &ph, state, png, size);

    if (result)
    {
		cerr << "failed to decode png: " << lodepng_error_text(result) << endl;
		return false;
    }

    int w = static_cast<int>(pw);
    int h = static_cast<int>(ph);

    LodePNGColorMode* color = &state->info_png.color;
    bool isIndexed = (color->colortype == LCT_PALETTE);

    if (isIndexed)
    {
        paletteSize = color->palettesize;
        palette = reinterpret_cast<uint32_t*>(calloc(paletteSize, sizeof(uint32_t)));

        memcpy(palette, color->palette, paletteSize * sizeof(uint32_t));

        if (color->bitdepth == 4)
        {
            LodePNGColorMode mode = lodepng_color_mode_make(LCT_PALETTE, 8);
            mode.palettesize = paletteSize;
            mode.palette = color->palette;

            int bpp = lodepng_get_bpp(&mode);
            data = reinterpret_cast<uint8_t*>(calloc((w * h * bpp + 7) / 8, sizeof(uint8_t)));

            lodepng_convert(data, buffer, &mode, &state->info_raw, w, h);

            free(buffer);
            buffer = data;
        }

        free(png);
    }
    else
    {
        //Premultiply all the pixels by their alpha
        if (premultiply)
        {
            int count = w * h;
            uint32_t c, a, r, g, b;
            float m;
            for (int i = 0; i < count; ++i)
            {
                c = reinterpret_cast<uint32_t*>(buffer)[i];
                a = c >> 24;
                m = static_cast<float>(a) / 255.0f;
                r = static_cast<uint32_t>((c & 0xff) * m);
                g = static_cast<uint32_t>(((c >> 8) & 0xff) * m);
                b = static_cast<uint32_t>(((c >> 16) & 0xff) * m);
                reinterpret_cast<uint32_t*>(buffer)[i] = (a << 24) | (b << 16) | (g << 8) | r;
            }
        }
    }

    //TODO: skip if all corners contain opaque pixels?

    //Get pixel bounds
    int minX = w - 1;
    int minY = h - 1;
    int maxX = 0;
    int maxY = 0;
    if (trim)
    {
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                int index = y * w + x;
                uint32_t p = (isIndexed ? reinterpret_cast<uint8_t*>(buffer)[index] : reinterpret_cast<uint32_t*>(buffer)[index]);
                uint8_t a = (isIndexed ? p != 0 : p >> 24);

                if (a)
                {
                    minX = min(x, minX);
                    minY = min(y, minY);
                    maxX = max(x, maxX);
                    maxY = max(y, maxY);
                }
            }
        }
        if (maxX < minX || maxY < minY)
        {
            minX = 0;
            minY = 0;
            maxX = w - 1;
            maxY = h - 1;
            if (verbose) cout << "image is completely transparent!" << endl;
        }
    }
    else
    {
        minX = 0;
        minY = 0;
        maxX = w - 1;
        maxY = h - 1;
    }

    //Calculate our trimmed size
    width = (maxX - minX) + 1;
    height = (maxY - minY) + 1;
    frameW = w;
    frameH = h;

    if (width == w && height == h)
    {
        //If we aren't trimmed, use the loaded image data
        frameX = 0;
        frameY = 0;
        data = buffer;
    }
    else
    {
        //Create the trimmed image data
        data = reinterpret_cast<uint8_t*>(calloc(width * height, isIndexed ? sizeof(uint8_t) : sizeof(uint32_t)));
        frameX = -minX;
        frameY = -minY;

        //Copy trimmed pixels over to the trimmed pixel array
        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                int srcIndex = y * w + x;
                int dstIndex = (y - minY) * width + (x - minX);
                if (isIndexed)
                    reinterpret_cast<uint8_t*>(data)[dstIndex] = reinterpret_cast<uint8_t*>(buffer)[srcIndex];
                else
                    reinterpret_cast<uint32_t*>(data)[dstIndex] = reinterpret_cast<uint32_t*>(buffer)[srcIndex];
            }
        }

        //Free the untrimmed pixels
        free(buffer);
    }

    //Generate a hash for the bitmap
    hashValue = 0;
    HashCombine(hashValue, static_cast<size_t>(width));
    HashCombine(hashValue, static_cast<size_t>(height));
    HashData(hashValue, reinterpret_cast<char*>(data), (isIndexed ? sizeof(uint8_t) : sizeof(uint32_t)) * width * height);
    
    return true;
}

Bitmap::~Bitmap()
{
    if (paletteSize)
		free(palette);
    free(data);
}

void Bitmap::SaveAs(const string& file)
{
    if (paletteSize > 0)
    {
        int result;

        unsigned int pw = static_cast<unsigned int>(width);
        unsigned int ph = static_cast<unsigned int>(height);

        LodePNGState state;

        lodepng_state_init(&state);

        for (int i = 0; i < paletteSize; i++)
        {
            lodepng_palette_add(&state.info_png.color, (palette[i] >> 0) & 0xff, (palette[i] >> 8) & 0xff, (palette[i] >> 16) & 0xff, 0xff);
            lodepng_palette_add(&state.info_raw, (palette[i] >> 0) & 0xff, (palette[i] >> 8) & 0xff, (palette[i] >> 16) & 0xff, 0xff);
       }

        state.info_png.color.colortype = LCT_PALETTE;
        state.info_png.color.bitdepth = 8;
        state.info_raw.colortype = LCT_PALETTE;
        state.info_raw.bitdepth = 8;
        state.encoder.auto_convert = 0;

        size_t pngSize;
        unsigned char* pngData = NULL;
        result = lodepng_encode(&pngData, &pngSize, data, pw, ph, &state);

        if (result)
        {
            cerr << "failed to encode png: " << file << " (" << lodepng_error_text(result) << ") " << endl;
            exit(EXIT_FAILURE);
        }

        if (lodepng_save_file(pngData, pngSize, file.data())) {
            cerr << "failed to save png: " << file << endl;
            exit(EXIT_FAILURE);
        }

        free(pngData);
    }
    else
    {
        unsigned int pw = static_cast<unsigned int>(width);
        unsigned int ph = static_cast<unsigned int>(height);

        if (lodepng_encode32_file(file.data(), data, pw, ph))
        {
            cout << "failed to save png: " << file << endl;
            exit(EXIT_FAILURE);
        }
    }
}

void Bitmap::FindPaletteSlot(Bitmap* dst)
{
    if (paletteSize != 256 || dst->paletteSize < 16)
        return;

    for (int i = 0; i < 16; i++)
    {
        int colorCount = 0;

        for (int j = 0; j < 16; j++)
        {
            int index = (i * 16) + j;

            if ((palette[index] & 0xffffff) == (dst->palette[j] & 0xffffff))
                colorCount++;
        }

        if (colorCount == 16)
        {
            dst->SetPaletteSlot(i);
            break;
        }
    }
}

void Bitmap::CopyPixels(const Bitmap* src, int tx, int ty)
{
    if (paletteSize > 0)
    {
        if (src->paletteSize == 0)
            return;

        for (int y = 0; y < src->height; ++y)
            for (int x = 0; x < src->width; ++x)
                data[(ty + y) * width + (tx + x)] = src->data[y * src->width + x];
    }
    else
    {
        if (src->paletteSize > 0)
            return;

        uint32_t* srcPixels = reinterpret_cast<uint32_t*>(src->data);
        uint32_t* dstPixels = reinterpret_cast<uint32_t*>(data);

        for (int y = 0; y < src->height; ++y)
            for (int x = 0; x < src->width; ++x)
                dstPixels[(ty + y) * width + (tx + x)] = srcPixels[y * src->width + x];
    }
}

void Bitmap::CopyPixelsRot(const Bitmap* src, int tx, int ty)
{
    if (paletteSize > 0)
    {
        if (src->paletteSize == 0)
            return;

        int r = src->height - 1;
        for (int y = 0; y < src->width; ++y)
            for (int x = 0; x < src->height; ++x)
                data[(ty + y) * width + (tx + x)] = src->data[(r - x) * src->width + y];
    }
    else
    {
        if (src->paletteSize > 0)
            return;

        uint32_t* srcPixels = reinterpret_cast<uint32_t*>(src->data);
        uint32_t* dstPixels = reinterpret_cast<uint32_t*>(data);

        int r = src->height - 1;
        for (int y = 0; y < src->width; ++y)
            for (int x = 0; x < src->height; ++x)
                dstPixels[(ty + y) * width + (tx + x)] = srcPixels[(r - x) * src->width + y];
    }
}

bool Bitmap::Equals(const Bitmap* other) const
{
    if (width != other->width || height != other->height)
        return false;

    return memcmp(data, other->data, (paletteSize > 0 ? sizeof(uint8_t) : sizeof(uint32_t)) * width * height) == 0;
}
