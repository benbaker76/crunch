/*
 * palette.cpp
 *
 *  Created on: Feb 24, 2024
 *      Author: bbaker
 */

#include <iostream>
#include <fstream>
#include <cstring>
#include <iomanip>
#include <cstdlib>
#include "palette.h"
#include "lodepng.h"

static unsigned char msPalHeader[] = { 'R', 'I', 'F', 'F' };
static unsigned char jascPalHeader[] = { 'J', 'A', 'S', 'C', '-', 'P', 'A', 'L' };
static unsigned char gimpPalHeader[] = { 'G', 'I', 'M', 'P', ' ', 'P', 'a', 'l', 'e', 't', 't', 'e' };
static unsigned char paintNetPalHeader[] = { ';' };
static unsigned char pngHeader[] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };

#define SWAP_SHORT(x) ((unsigned short)(((x) << 8) | ((x) >> 8)))

int Palette::StartsWith(const unsigned char* thisBytes, const unsigned char* thatBytes, int thisLength, int thatLength)
{
    if (thatLength > thisLength)
        return 0;

    for (int i = 0; i < thatLength; i++)
    {
        if (thisBytes[i] != thatBytes[i])
            return 0;
    }

    return 1;
}

int Palette::ReadPng(const char* fileName, Color** colorPalette, int* paletteCount)
{
    *colorPalette = new Color[256];

    if (*colorPalette == nullptr)
        return EXIT_FAILURE;

    LodePNGState state;
    unsigned char* png = NULL;
    size_t size;
    unsigned char* buffer;
    unsigned width, height;

    lodepng_state_init(&state);

    state.decoder.color_convert = 0;

    lodepng_load_file(&png, &size, fileName);
    int result = lodepng_decode(&buffer, &width, &height, &state, png, size);

    if(result)
    {
        printf("Decoder error %u: %s\n", result, lodepng_error_text(result));
        delete[] *colorPalette;
        *colorPalette = nullptr;
        return EXIT_FAILURE;
    }

    LodePNGColorMode* color = &state.info_png.color;
    
    if(color->colortype != LCT_PALETTE)
    {
        printf("Error: PNG is not paletted\n");
        delete[] *colorPalette;
        *colorPalette = nullptr;
        return EXIT_FAILURE;
    }

    *paletteCount = color->palettesize;

    for(size_t i = 0; i < color->palettesize; i++)
    {
        (*colorPalette)[i].R = color->palette[i * 4 + 0];
        (*colorPalette)[i].G = color->palette[i * 4 + 1];
        (*colorPalette)[i].B = color->palette[i * 4 + 2];
        (*colorPalette)[i].A = color->palette[i * 4 + 3];
    }

    return EXIT_SUCCESS;
}

int Palette::ReadMsPal(std::ifstream& file, Color** colorPalette, int* paletteCount)
{
    int fileLength;
    unsigned char riffType[4];
    unsigned char riffChunkSignature[4];
    unsigned char chunkSize[4];
    unsigned char paletteVersion[2];
    short palCount;

    file.read(reinterpret_cast<char*>(&fileLength), sizeof(int));
    fileLength -= 16;
    file.read(reinterpret_cast<char*>(riffType), sizeof(unsigned char) * 4);
    file.read(reinterpret_cast<char*>(riffChunkSignature), sizeof(unsigned char) * 4);
    file.read(reinterpret_cast<char*>(chunkSize), sizeof(unsigned char) * 4);
    file.read(reinterpret_cast<char*>(paletteVersion), sizeof(unsigned char) * 2);
    file.read(reinterpret_cast<char*>(&palCount), sizeof(short));
    
    *colorPalette = new Color[palCount];
    *paletteCount = palCount;

    if (*colorPalette == nullptr)
        return EXIT_FAILURE;

    for (int i = 0; i < palCount; i++)
    {
        unsigned char colorArray[4];
        file.read(reinterpret_cast<char*>(colorArray), sizeof(unsigned char) * 4);
        (*colorPalette)[i].R = colorArray[0];
        (*colorPalette)[i].G = colorArray[1];
        (*colorPalette)[i].B = colorArray[2];
        (*colorPalette)[i].A = colorArray[3];
    }

    return EXIT_SUCCESS;
}

int Palette::ReadActPal(std::ifstream& file, Color** colorPalette, int* paletteCount, int *transparentIndex)
{
    *colorPalette = new Color[256];

    if (*colorPalette == nullptr)
        return EXIT_FAILURE;

    for (int i = 0; i < 256; i++)
    {
        if (!file.read(reinterpret_cast<char*>(&(*colorPalette)[i].R), sizeof(unsigned char)) ||
            !file.read(reinterpret_cast<char*>(&(*colorPalette)[i].G), sizeof(unsigned char)) ||
            !file.read(reinterpret_cast<char*>(&(*colorPalette)[i].B), sizeof(unsigned char)))
        {
            delete[] *colorPalette;
            *colorPalette = nullptr;
            return EXIT_FAILURE;
        }
    }

    long currentPosition = file.tellg();
    file.seekg(0, std::ios::end);
    long endPosition = file.tellg();
    file.seekg(currentPosition, std::ios::beg);

    if (currentPosition == endPosition - 4)
    {
        short palCount, alphaIndex;
        if (!file.read(reinterpret_cast<char*>(&palCount), sizeof(short)) ||
            !file.read(reinterpret_cast<char*>(&alphaIndex), sizeof(short)))
        {
            delete[] *colorPalette;
            *colorPalette = nullptr;
            return EXIT_FAILURE;
        }

        *colorPalette = (Color*)realloc(*colorPalette, SWAP_SHORT(palCount) * sizeof(Color));
        *paletteCount = SWAP_SHORT(palCount);
        *transparentIndex = SWAP_SHORT(alphaIndex);
    }
    else
        *paletteCount = 256;
    
    return EXIT_SUCCESS;
}

int Palette::ReadJascPal(std::ifstream& file, Color** colorPalette, int* paletteCount)
{
    char* tempString = nullptr;
    char* versionString = nullptr;
    tempString = new char[256];
    versionString = new char[256];
    file.getline(tempString, 256);
    file.getline(versionString, 256);
    file >> *paletteCount;

    *colorPalette = new Color[*paletteCount];

    if (*colorPalette == nullptr)
    {
        delete[] tempString;
        delete[] versionString;
        return EXIT_FAILURE;
    }

    for (int i = 0; i < *paletteCount; i++)
    {
        char colorString[256];
        file.getline(colorString, 256);

        if (colorString == nullptr)
            break;

        int red, green, blue;
        sscanf(colorString, "%d %d %d", &red, &green, &blue);

        (*colorPalette)[i].R = red;
        (*colorPalette)[i].G = green;
        (*colorPalette)[i].B = blue;
    }

    delete[] tempString;
    delete[] versionString;

    return EXIT_SUCCESS;
}

int Palette::ReadGimpPal(std::ifstream& file, Color** colorPalette)
{
    *colorPalette = new Color[256];

    if (*colorPalette == nullptr)
        return EXIT_FAILURE; 

    char lineString[256];

    while (1)
    {
        file.getline(lineString, 256);

        if (lineString == nullptr)
            break;

        if (strcmp(lineString, "") == 0 ||
            strncmp(lineString, "Name:", 5) == 0 ||
            strncmp(lineString, "Columns:", 8) == 0 ||
            strncmp(lineString, "#", 1) == 0) {
            continue;
        }

        char* colorArray[3];
        int palCount = 0;
        char* token = strtok(lineString, " \t");

        while (token != nullptr)
        {
            colorArray[palCount++] = token;
            token = strtok(nullptr, " \t");
        }

        if (palCount < 3)
            continue;

        int red = atoi(colorArray[0]);
        int green = atoi(colorArray[1]);
        int blue = atoi(colorArray[2]);

        *colorPalette = (Color*)realloc(*colorPalette, (palCount + 1) * sizeof(Color));

        (*colorPalette)[palCount].R = red;
        (*colorPalette)[palCount].G = green;
        (*colorPalette)[palCount].B = blue;
    }

    return EXIT_SUCCESS;
}

int Palette::ReadPaintNetPal(std::ifstream& file, Color** colorPalette)
{
    *colorPalette = new Color[256];

    if (*colorPalette == nullptr)
        return EXIT_FAILURE;

    char lineString[256];
    int palCount = 0;

    while (1)
    {
        file.getline(lineString, 256);

        if (lineString == nullptr)
            break;

        if (strcmp(lineString, "") == 0 ||
            strncmp(lineString, ";", 1) == 0)
            continue;

        int result = 0;
        sscanf(lineString, "%x", &result);

        (*colorPalette)[palCount].R = (result >> 16) & 0xFF;
        (*colorPalette)[palCount].G = (result >> 8) & 0xFF;
        (*colorPalette)[palCount].B = result & 0xFF;

        palCount++;
    }

    *colorPalette = (Color*)realloc(*colorPalette, (palCount + 1) * sizeof(Color));

    return EXIT_SUCCESS;
}

int Palette::ReadPalette(const char* fileName, Color** colorPalette, int* paletteCount, int* transparentIndex)
{
    int result = EXIT_FAILURE;

    std::ifstream file(fileName, std::ios::binary);

    if (!file)
        return EXIT_FAILURE;

    int maxMagicBytesLength = 0;
    unsigned char magicBytes[256];
    int bytesRead = file.read(reinterpret_cast<char*>(magicBytes), sizeof(unsigned char) * 256).gcount();

    for (int i = 0; i < bytesRead; i++)
    {
        if (magicBytes[i] == '\0')
        {
            maxMagicBytesLength = i;
            break;
        }
    }

    if (StartsWith(magicBytes, msPalHeader, bytesRead, 4))
    {
        file.seekg(4, std::ios::cur);
        file.seekg(4, std::ios::cur);
        file.seekg(4, std::ios::cur);
        file.seekg(2, std::ios::cur);
        file.read(reinterpret_cast<char*>(paletteCount), sizeof(short));
        result = ReadMsPal(file, colorPalette, paletteCount);
    }
    else if (StartsWith(magicBytes, jascPalHeader, bytesRead, 8))
    {
        file.seekg(8, std::ios::cur);
        file >> *paletteCount;
        result = ReadJascPal(file, colorPalette, paletteCount);
    }
    else if (StartsWith(magicBytes, gimpPalHeader, bytesRead, 12))
    {
        file.seekg(12, std::ios::cur);
        result = ReadGimpPal(file, colorPalette);
    }
    else if (StartsWith(magicBytes, paintNetPalHeader, bytesRead, 1))
    {
        file.seekg(1, std::ios::cur);
        result = ReadPaintNetPal(file, colorPalette);
    }
    else if (StartsWith(magicBytes, pngHeader, bytesRead, 8))
    {
        file.close();
        result = ReadPng(fileName, colorPalette, paletteCount);
    }
    else
    {
        file.seekg(0, std::ios::beg);
        result = ReadActPal(file, colorPalette, paletteCount, transparentIndex);
    }

    if (file.is_open())
        file.close();
    
    return result;
}

int Palette::WriteActPal(const char *fileName, Color *colorPalette, int paletteCount, int transparentIndex)
{
    std::ofstream file(fileName, std::ios::binary);

    if (!file)
        return EXIT_FAILURE;

    for (int i = 0; i < 256; i++)
    {
        if (i < paletteCount)
        {
            file.write(reinterpret_cast<const char*>(&colorPalette[i].R), sizeof(unsigned char));
            file.write(reinterpret_cast<const char*>(&colorPalette[i].G), sizeof(unsigned char));
            file.write(reinterpret_cast<const char*>(&colorPalette[i].B), sizeof(unsigned char));
        }
        else
        {
            unsigned char zero = 0;
            file.write(reinterpret_cast<const char*>(&zero), sizeof(unsigned char));
            file.write(reinterpret_cast<const char*>(&zero), sizeof(unsigned char));
            file.write(reinterpret_cast<const char*>(&zero), sizeof(unsigned char));
        }
    }

    if (transparentIndex != -1 || paletteCount < 256)
    {
        unsigned char paletteCountHigh = (unsigned char)(paletteCount >> 8);
        unsigned char paletteCountLow = (unsigned char)paletteCount;
        file.write(reinterpret_cast<const char*>(&paletteCountHigh), sizeof(unsigned char));
        file.write(reinterpret_cast<const char*>(&paletteCountLow), sizeof(unsigned char));
        unsigned char transparentIndexHigh = (transparentIndex == -1) ? 0xFF : (unsigned char)(transparentIndex >> 8);
        unsigned char transparentIndexLow = (transparentIndex == -1) ? 0xFF : (unsigned char)transparentIndex;
        file.write(reinterpret_cast<const char*>(&transparentIndexHigh), sizeof(unsigned char));
        file.write(reinterpret_cast<const char*>(&transparentIndexLow), sizeof(unsigned char));
    }

    file.close();

    return EXIT_SUCCESS;
}

int Palette::WriteMsPal(const char* fileName, Color* colorPalette, int paletteCount)
{
    std::ofstream file(fileName, std::ios::binary);

    if (!file)
        return EXIT_FAILURE;

    unsigned char riffSig[4] = { 'R', 'I', 'F', 'F' };
    unsigned char riffType[4] = { 'P', 'A', 'L', ' ' };
    unsigned char riffChunkSig[4] = { 'd', 'a', 't', 'a' };
    unsigned char chunkSize[4];
    unsigned char palVer[2] = { 0x00, 0x03 };

    short palCount = paletteCount;

    file.write(reinterpret_cast<const char*>(riffSig), sizeof(unsigned char) * 4);
    file.write(reinterpret_cast<const char*>(chunkSize), sizeof(unsigned char) * 4);
    file.write(reinterpret_cast<const char*>(riffType), sizeof(unsigned char) * 4);
    file.write(reinterpret_cast<const char*>(riffChunkSig), sizeof(unsigned char) * 4);
    file.write(reinterpret_cast<const char*>(chunkSize), sizeof(unsigned char) * 4);
    file.write(reinterpret_cast<const char*>(palVer), sizeof(unsigned char) * 2);
    file.write(reinterpret_cast<const char*>(&palCount), sizeof(short));

    for (int i = 0; i < palCount; i++)
    {
        file.write(reinterpret_cast<const char*>(&colorPalette[i].R), sizeof(unsigned char));
        file.write(reinterpret_cast<const char*>(&colorPalette[i].G), sizeof(unsigned char));
        file.write(reinterpret_cast<const char*>(&colorPalette[i].B), sizeof(unsigned char));
        file.write("\0", sizeof(unsigned char));
    }

    file.close();

    return EXIT_SUCCESS;
}

int Palette::WriteJascPal(const char* fileName, Color* colorPalette, int paletteCount)
{
    std::ofstream file(fileName, std::ios::binary);

    if (!file)
        return EXIT_FAILURE;

    file << "JASC-PAL\n";
    file << "0100\n";
    file << paletteCount << "\n";

    for (int i = 0; i < paletteCount; i++)
        file << colorPalette[i].R << " " << colorPalette[i].G << " " << colorPalette[i].B << "\n";

    file.close();

    return EXIT_SUCCESS;
}

int Palette::WriteGimpPal(const char* fileName, Color* colorPalette, int paletteCount) 
{
    std::ofstream file(fileName, std::ios::binary);

    if (!file)
        return EXIT_FAILURE;

    file << "GIMP Palette\n";
    file << "Name: " << fileName << "\n";
    file << "Columns: 0\n";
    file << "#\n";

    for (int i = 0; i < paletteCount; i++)
    {
        file << std::setw(3) << std::setfill(' ') << colorPalette[i].R << " ";
        file << std::setw(3) << std::setfill(' ') << colorPalette[i].G << " ";
        file << std::setw(3) << std::setfill(' ') << colorPalette[i].B << "\tUntitled\n";
    }

    file.close();

    return EXIT_SUCCESS;
}

int Palette::WritePaintNetPal(const char* fileName, Color* colorPalette, int paletteCount)
{
    std::ofstream file(fileName, std::ios::binary);

    if (!file)
        return EXIT_FAILURE;

    file << "; Paint.NET Palette\n";
    file << "; " << fileName << "\n";

    for (int i = 0; i < paletteCount; i++)
        file << std::hex << std::setfill('0') << std::setw(8) << ((colorPalette[i].R << 16) | (colorPalette[i].G << 8) | colorPalette[i].B) << "\n";

    file.close();

    return EXIT_SUCCESS;
}

int Palette::WritePalette(const char* fileName, Color* colorPalette, int paletteCount, int transparentIndex, PaletteFormat paletteFormat)
{
    switch (paletteFormat)
    {
        case Act:
            return WriteActPal(fileName, colorPalette, paletteCount, transparentIndex);
        case MSPal:
            return WriteMsPal(fileName, colorPalette, paletteCount);
        case JASC:
            return WriteJascPal(fileName, colorPalette, paletteCount);
        case GIMP:
            return WriteGimpPal(fileName, colorPalette, paletteCount);
        case PaintNET:
            return WritePaintNetPal(fileName, colorPalette, paletteCount);
    }

    return EXIT_FAILURE;
}
