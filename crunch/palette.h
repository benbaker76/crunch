/*
 * palette.hpp
 *
 *  Created on: Feb 24, 2024
 *      Author: bbaker
 */

#ifndef PALETTE_H_
#define PALETTE_H_

#include <fstream>

typedef struct {
    unsigned char R;
    unsigned char G;
    unsigned char B;
    unsigned char A;
} Color;

typedef enum {
    Act,
    MSPal,
    JASC,
    GIMP,
    PaintNET
} PaletteFormat;

class Palette {
private:
    int StartsWith(const unsigned char* thisBytes, const unsigned char* thatBytes, int thisLength, int thatLength);
    int ReadPng(const char* fileName, Color** colorPalette, int* paletteCount);
    int ReadMsPal(std::ifstream& file, Color** colorPalette, int* paletteCount);
    int ReadActPal(std::ifstream& file, Color** colorPalette, int* paletteCount, int *transparentIndex);
    int ReadJascPal(std::ifstream& file, Color** colorPalette, int* paletteCount);
    int ReadGimpPal(std::ifstream& file, Color** colorPalette);
    int ReadPaintNetPal(std::ifstream& file, Color** colorPalette);
    int WriteActPal(const char *fileName, Color *colorPalette, int paletteCount, int transparentIndex);
    int WriteMsPal(const char* fileName, Color* colorPalette, int paletteCount);
    int WriteJascPal(const char* fileName, Color* colorPalette, int paletteCount);
    int WriteGimpPal(const char* fileName, Color* colorPalette, int paletteCount);
    int WritePaintNetPal(const char* fileName, Color* colorPalette, int paletteCount);

public:
    int ReadPalette(const char* fileName, Color** colorPalette, int* paletteCount, int* transparentIndex);
    int WritePalette(const char* fileName, Color* colorPalette, int paletteCount, int transparentIndex, PaletteFormat paletteFormat);
};

#endif /* PALETTE_H_ */