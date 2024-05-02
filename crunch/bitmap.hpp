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

#ifndef bitmap_hpp
#define bitmap_hpp

#include <string>
#include <cstdint>
#include <vector>
#include "lodepng.h"

using namespace std;

struct Bitmap
{
    int frameIndex;
    string name;
    string label;
    int loopDirection;
    int duration;
    int width;
    int height;
    int frameX;
    int frameY;
    int frameW;
    int frameH;
    uint8_t* data;
    uint32_t* palette;
    size_t hashValue;
    int paletteSize;
    int paletteSlot;

    Bitmap(const string& file, const string& name, bool premultiply, bool trim, bool verbose);
    Bitmap(int frameIndex, const string& name, const string& label, int loopDirection, int duration, LodePNGState* state, unsigned char* png, size_t size, bool premultiply, bool trim, bool verbose);
    Bitmap(int width, int height, uint32_t* palette, int paletteSize);
    ~Bitmap();
    bool DecodePng(LodePNGState* state, unsigned char* png, size_t size, bool premultiply, bool trim, bool verbose);
    void SaveAs(const string& file);
    void FindPaletteSlot(Bitmap* dst);
    void SetPaletteSlot(int paletteSlot) { this->paletteSlot = paletteSlot; }
    void CopyPixels(const Bitmap* src, int tx, int ty);
    void CopyPixelsRot(const Bitmap* src, int tx, int ty);
    bool Equals(const Bitmap* other) const;
};

#endif
