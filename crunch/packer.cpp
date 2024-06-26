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

#include "packer.hpp"
#include "MaxRectsBinPack.h"
#include "GuillotineBinPack.h"
#include "binary.hpp"
#include <iostream>
#include <algorithm>

using namespace std;
using namespace rbp;

Packer::Packer(int width, int height, int pad)
: width(width), height(height), pad(pad)
{
    
}

void Packer::Pack(vector<Bitmap*>& bitmaps, bool verbose, bool unique, bool rotate)
{
    MaxRectsBinPack packer(width, height);

    int ww = 0;
    int hh = 0;
    while (!bitmaps.empty())
    {
        auto bitmap = bitmaps.back();

        if (verbose)
            cout << '\t' << bitmaps.size() << ": " << bitmap->name << endl;

        //Check to see if this is a duplicate of an already packed bitmap
        if (unique)
        {
            auto di = dupLookup.find(bitmap->hashValue);
            if (di != dupLookup.end() && bitmap->Equals(this->bitmaps[di->second]))
            {
                Bitmap* dupBitmap = this->bitmaps[di->second];
                dupBitmap->pos = bitmap->pos;
                bitmaps.pop_back();
                continue;
            }
        }

        //If it's not a duplicate, pack it into the atlas
        {
            Rect rect = packer.Insert(bitmap->width + pad, bitmap->height + pad, rotate, MaxRectsBinPack::RectBestShortSideFit);

            if (rect.width == 0 || rect.height == 0)
                break;

            if (unique)
                dupLookup[bitmap->hashValue] = static_cast<int>(this->bitmaps.size());

            //Check if we rotated it
            Point p;
            p.x = rect.x;
            p.y = rect.y;
            p.dupID = -1;
            p.rot = rotate && bitmap->width != (rect.width - pad);

            bitmap->pos = p;
            this->bitmaps.push_back(bitmap);
            bitmaps.pop_back();

            ww = max(rect.x + rect.width, ww);
            hh = max(rect.y + rect.height, hh);
        }
    }

    while (width / 2 >= ww)
        width /= 2;
    while (height / 2 >= hh)
        height /= 2;
}

void Packer::SavePng(const string& file, uint32_t* palette, int paletteSize)
{
    Bitmap bitmap(width, height, palette, paletteSize);

    for (size_t i = 0, j = bitmaps.size(); i < j; ++i)
    {
        if (bitmaps[i]->pos.dupID < 0)
        {
            bitmap.FindPaletteSlot(bitmaps[i]);

            if (bitmaps[i]->pos.rot)
                bitmap.CopyPixelsRot(bitmaps[i], bitmaps[i]->pos.x, bitmaps[i]->pos.y);
            else
                bitmap.CopyPixels(bitmaps[i], bitmaps[i]->pos.x, bitmaps[i]->pos.y);
        }
    }
    bitmap.SaveAs(file);
}

void Packer::SaveXml(const string& name, ofstream& xml, int format, bool trim, bool rotate)
{
    xml << "\t<tex n=\"" << name << "\" ";
    xml << "w=\"" << width << "\" ";
    xml << "h=\"" << height << "\" ";
    xml << "format=\"" << format << "\">" << endl;
    for (size_t i = 0, j = bitmaps.size(); i < j; ++i)
    {
        xml << "\t\t<img fi=\"" << bitmaps[i]->frameIndex << "\" ";
        xml << "n=\"" << bitmaps[i]->name << "\" ";
        xml << "l=\"" << bitmaps[i]->label << "\" ";
        xml << "ld=\"" << bitmaps[i]->loopDirection << "\" ";
        xml << "d=\"" << bitmaps[i]->duration << "\" ";
        xml << "x=\"" << bitmaps[i]->pos.x << "\" ";
        xml << "y=\"" << bitmaps[i]->pos.y << "\" ";
        xml << "w=\"" << bitmaps[i]->width << "\" ";
        xml << "h=\"" << bitmaps[i]->height << "\" ";
        if (trim)
        {
            xml << "fx=\"" << bitmaps[i]->frameX << "\" ";
            xml << "fy=\"" << bitmaps[i]->frameY << "\" ";
            xml << "fw=\"" << bitmaps[i]->frameW << "\" ";
            xml << "fh=\"" << bitmaps[i]->frameH << "\" ";
        }
        if (rotate)
            xml << "r=\"" << (bitmaps[i]->pos.rot ? 1 : 0) << "\" ";
        xml << "ps=\"" << bitmaps[i]->paletteSlot << "\" ";
        xml << "/>" << endl;
    }
    xml << "\t</tex>" << endl;
}

void Packer::SaveBin(const string& name, ofstream& bin, int format, bool trim, bool rotate, int length)
{
    WriteString(bin, name, length);
    WriteShort(bin, width);
    WriteShort(bin, height);
    WriteShort(bin, (int16_t)format);
    WriteShort(bin, (int16_t)bitmaps.size());
    for (size_t i = 0, j = bitmaps.size(); i < j; ++i)
    {
        WriteShort(bin, (int16_t)bitmaps[i]->frameIndex);
        WriteString(bin, bitmaps[i]->name, length);
        WriteString(bin, bitmaps[i]->label, length);
        WriteByte(bin, bitmaps[i]->loopDirection);
        WriteShort(bin, (int16_t)bitmaps[i]->duration);
        WriteShort(bin, (int16_t)bitmaps[i]->pos.x);
        WriteShort(bin, (int16_t)bitmaps[i]->pos.y);
        WriteShort(bin, (int16_t)bitmaps[i]->width);
        WriteShort(bin, (int16_t)bitmaps[i]->height);
        if (trim)
        {
            WriteShort(bin, (int16_t)bitmaps[i]->frameX);
            WriteShort(bin, (int16_t)bitmaps[i]->frameY);
            WriteShort(bin, (int16_t)bitmaps[i]->frameW);
            WriteShort(bin, (int16_t)bitmaps[i]->frameH);
        }
        if (rotate)
            WriteByte(bin, bitmaps[i]->pos.rot ? 1 : 0);
        WriteByte(bin, bitmaps[i]->paletteSlot);
        std::cout << "Saved " << bitmaps[i]->name << " slot " << bitmaps[i]->paletteSlot << std::endl;
    }
}

void Packer::SaveJson(const string& name, ofstream& json, int format, bool trim, bool rotate)
{
    json << "\t\t\t\"name\":\"" << name << "\"," << endl;
    json << "\t\t\t\"width\":" << width << "," << endl;
    json << "\t\t\t\"height\":" << height << "," << endl;
    json << "\t\t\t\"format\":\"" << format << "\"," << endl;
    json << "\t\t\t\"images\":[" << endl;
    for (size_t i = 0, j = bitmaps.size(); i < j; ++i)
    {
        json << "\t\t\t\t{ ";
        json << "\"fi\":" << bitmaps[i]->frameIndex << ", ";
        json << "\"n\":\"" << bitmaps[i]->name << "\", ";
        json << "\"l\":\"" << bitmaps[i]->label << "\", ";
        json << "\"ld\":" << bitmaps[i]->loopDirection << ", ";
        json << "\"d\":" << bitmaps[i]->duration << ", ";
        json << "\"x\":" << bitmaps[i]->pos.x << ", ";
        json << "\"y\":" << bitmaps[i]->pos.y << ", ";
        json << "\"w\":" << bitmaps[i]->width << ", ";
        json << "\"h\":" << bitmaps[i]->height;
        if (trim)
        {
            json << ", \"fx\":" << bitmaps[i]->frameX << ", ";
            json << "\"fy\":" << bitmaps[i]->frameY << ", ";
            json << "\"fw\":" << bitmaps[i]->frameW << ", ";
            json << "\"fh\":" << bitmaps[i]->frameH;
        }
        if (rotate)
            json << ", \"r\":" << (bitmaps[i]->pos.rot ? "true" : "false");
        json << ", \"ps\":" << bitmaps[i]->paletteSlot;
        json << " }";
        if(i != bitmaps.size() -1)
            json << ",";
        json << endl;
    }
    json << "\t\t\t]" << endl;
}
