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
 SOFTWARE. */

#include <iostream>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <getopt.h>
#include <unistd.h>
#include "tinydir.h"
#include "bitmap.hpp"
#include "packer.hpp"
#include "binary.hpp"
#include "hash.hpp"
#include "str.hpp"
#include "time.hpp"
#include "palette.h"
#define EXIT_SKIPPED 2

using namespace std;

const char *version = "v0.14";
const int binVersion = 0;

#define NAME_LENGTH 16

enum Format
{
    XML,
    BIN,
    JSON
};

enum StringType
{
    NULL_TERMINATED,
    PREFIXED,
    SEVEN_BIT_PREFIXED,
    FIXED_LENGTH
};

static struct options
{
    const char *inputFilename;
    const char *outputFilename;
    const char *paletteFilename;
    int size;
    int width;
    int height;
    int padding;
    StringType binstr;
    Format format;
    bool alpha;
    bool trim;
    bool verbose;
    bool ignore;
    bool unique;
    bool rotate;
    bool last;
    bool dirs;
    bool nozero;
} options;

static vector<Bitmap *> bitmaps;
static vector<Packer *> packers;

static const char *helpMessage =
    "usage:\n"
    "   crunch [options] <inputFilename1,inputFilename2,inputFilename3...> <outputFilename> [paletteFilename]\n"
    "\n"
    "example:\n"
    "   crunch bin/atlases/atlas assets/characters,assets/tiles -a -t -v -u -r\n"
    "\n"
    "options:\n"
    "   -f --format <xml|bin|json>  saves the atlas data in xml, binary or json format\n"
    "   -a --alpha                  premultiplies the pixels of the bitmaps by their alpha channel\n"
    "   -t --trim                   trims excess transparency off the bitmaps\n"
    "   -v --verbose                print to the debug console as the packer works\n"
    "   -i --ignore                 ignore the hash, forcing the packer to repack\n"
    "   -u --unique                 remove duplicate bitmaps from the atlas\n"
    "   -r --rotate                 enabled rotating bitmaps 90 degrees clockwise when packing\n"
    "   -s --size <n>               max atlas size (<n> can be 4096, 2048, 1024, 512, 256, 128, or 64)\n"
    "   -w --width <n>              max atlas width (overrides --size) (<n> can be 4096, 2048, 1024, 512, 256, 128, or 64)\n"
    "   -h --height <n>             max atlas height (overrides --size) (<n> can be 4096, 2048, 1024, 512, 256, 128, or 64)\n"
    "   -p --padding <n>            padding between images (<n> can be from 0 to 16)\n"
    "   -b --binstr <n|p|7|f>       string type in binary format (n: null-terminated, p: prefixed (int16), 7: 7-bit prefixed, f: fixed 8 bytes)\n"
    "   -l --last                   use file's last write time instead of its content for hashing\n"
    "   -d --dirs                   split output textures by subdirectories\n"
    "   -n --nozero                 if there's ony one packed texture, then zero at the end of its name will be omitted (ex. images0.png -> images.png)\n"
    "\n"
    "palette formats:\n"
    "  act, jasc, mspal, gimp, paint.net and png.\n"
    "binary format:\n"
    "crch (0x68637263 in hex or 1751347811 in decimal)\n"
    "[int16] version (current version is 0)"
    "[byte] --trim enabled\n"
    "[byte] --rotate enabled\n"
    "[byte] string type (0 - null-termainated, 1 - prefixed (int16), 2 - 7-bit prefixed)\n"
    "[int16] num_textures (below block is repeated this many times)\n"
    "  [string] name\n"
    "    [int16] num_images (below block is repeated this many times)\n"
    "      [string] img_name\n"
    "      [int16] img_x\n"
    "      [int16] img_y\n"
    "      [int16] img_width\n"
    "      [int16] img_height\n"
    "      [int16] img_frame_x         (if --trim enabled)\n"
    "      [int16] img_frame_y         (if --trim enabled)\n"
    "      [int16] img_frame_width     (if --trim enabled)\n"
    "      [int16] img_frame_height    (if --trim enabled)\n"
    "      [byte] img_rotated          (if --rotate enabled)\n"
    "      [byte] img_slot";

static void SplitFileName(const string &path, string *dir, string *name, string *ext)
{
    size_t si = path.rfind('/') + 1;
    if (si == string::npos)
        si = 0;
    size_t di = path.rfind('.');
    if (dir != nullptr)
    {
        if (si > 0)
            *dir = path.substr(0, si);
        else
            *dir = "";
    }
    if (name != nullptr)
    {
        if (di != string::npos)
            *name = path.substr(si, di - si);
        else
            *name = path.substr(si);
    }
    if (ext != nullptr)
    {
        if (di != string::npos)
            *ext = path.substr(di);
        else
            *ext = "";
    }
}

static string GetFileName(const string &path)
{
    string name;
    SplitFileName(path, nullptr, &name, nullptr);
    return name;
}

static void LoadBitmap(const string &prefix, const string &path)
{
    if (options.verbose)
        cout << '\t' << path << endl;

    bitmaps.push_back(new Bitmap(path, prefix + GetFileName(path), options.alpha, options.trim, options.verbose));
}

static void LoadBitmaps(const string &root, const string &prefix)
{
    static string dot1 = ".";
    static string dot2 = "..";

    tinydir_dir dir;
    tinydir_open_sorted(&dir, StrToPath(root).data());

    for (int i = 0; i < static_cast<int>(dir.n_files); ++i)
    {
        tinydir_file file;
        tinydir_readfile_n(&dir, &file, i);

        if (file.is_dir)
        {
            if (dot1 != PathToStr(file.name) && dot2 != PathToStr(file.name))
                LoadBitmaps(PathToStr(file.path), prefix + PathToStr(file.name) + "/");
        }
        else if (PathToStr(file.extension) == "png")
            LoadBitmap(prefix, PathToStr(file.path));
    }

    tinydir_close(&dir);
}

static void RemoveFile(string file)
{
    remove(file.data());
}

static const char *GetFormatString(Format format)
{
    switch (format)
    {
    case XML:
        return "xml";
    case BIN:
        return "bin";
    case JSON:
        return "json";
    }
    return "";
}

static int GetPackSize(const string &str)
{
    if (str == "4096")
        return 4096;
    if (str == "2048")
        return 2048;
    if (str == "1024")
        return 1024;
    if (str == "512")
        return 512;
    if (str == "256")
        return 256;
    if (str == "128")
        return 128;
    if (str == "64")
        return 64;
    cerr << "invalid size: " << str << endl;
    exit(EXIT_FAILURE);
    return 0;
}

static StringType GetBinStrType(const string &str)
{
    if (str == "n" || str == "N")
        return NULL_TERMINATED;
    if (str == "p" || str == "P")
        return PREFIXED;
    if (str == "7")
        return SEVEN_BIT_PREFIXED;
    if (str == "f" || str == "F")
        return FIXED_LENGTH;
    cerr << "invalid binary string type: " << str << endl;
    exit(EXIT_FAILURE);
    return NULL_TERMINATED;
}

static int GetPadding(const string &str)
{
    for (int i = 0; i <= 16; ++i)
        if (str == to_string(i))
            return i;
    cerr << "invalid padding value: " << str << endl;
    exit(EXIT_FAILURE);
    return 1;
}

static void GetSubdirs(const string &root, vector<string> &subdirs)
{
    static string dot1 = ".";
    static string dot2 = "..";

    tinydir_dir dir;
    tinydir_open_sorted(&dir, StrToPath(root).data());

    for (int i = 0; i < static_cast<int>(dir.n_files); ++i)
    {
        tinydir_file file;
        tinydir_readfile_n(&dir, &file, i);

        if (file.is_dir)
        {
            if (dot1 != PathToStr(file.name) && dot2 != PathToStr(file.name))
                subdirs.push_back(PathToStr(file.path));
        }
    }

    tinydir_close(&dir);
}

static void FindPackers(const string &root, const string &name, const string &ext, vector<string> &packers)
{
    static string dot1 = ".";
    static string dot2 = "..";

    tinydir_dir dir;
    tinydir_open_sorted(&dir, StrToPath(root).data());

    for (int i = 0; i < static_cast<int>(dir.n_files); ++i)
    {
        tinydir_file file;
        tinydir_readfile_n(&dir, &file, i);

        if (!file.is_dir && PathToStr(file.name).starts_with(name) && PathToStr(file.extension) == ext)
            packers.push_back(PathToStr(file.path));
    }

    tinydir_close(&dir);
}

static int Pack(size_t newHash, string &outputDir, string &name, vector<string> &inputs, string prefix = "")
{
    if (options.dirs) StartTimer(prefix);
    StartTimer("hashing input");
    for (size_t i = 0; i < inputs.size(); ++i)
    {
        if (inputs[i].rfind('.') == string::npos)
            HashFiles(newHash, inputs[i], options.last);
        else
            HashFile(newHash, inputs[i], options.last);
    }
    StopTimer("hashing input");
    // Load the old hash
    size_t oldHash;
    if (LoadHash(oldHash, outputDir + name + ".hash"))
    {
        if (!options.ignore && newHash == oldHash)
        {
            if (!options.dirs)
            {
                cout << "atlas is unchanged: " << name << endl;
                
                return EXIT_SUCCESS;
            }
            StopTimer(prefix);
            return EXIT_SKIPPED;
        }
    }

    // Remove old files
    RemoveFile(outputDir + name + ".hash");
    RemoveFile(outputDir + name + ".bin");
    RemoveFile(outputDir + name + ".xml");
    RemoveFile(outputDir + name + ".json");
    RemoveFile(outputDir + name + ".png");
    for (size_t i = 0; i < 16; ++i)
        RemoveFile(outputDir + name + to_string(i) + ".png");

    StartTimer("loading bitmaps");

    // Load the bitmaps from all the input files and directories
    if (options.verbose)
        cout << "loading images..." << endl;
    for (size_t i = 0; i < inputs.size(); ++i)
    {
        if (!options.dirs && inputs[i].rfind('.') != string::npos)
            LoadBitmap("", inputs[i]);
        else
            LoadBitmaps(inputs[i], prefix);
    }
    StopTimer("loading bitmaps");

    StartTimer("sorting bitmaps");
    // Sort the bitmaps by area
    stable_sort(bitmaps.begin(), bitmaps.end(), [](const Bitmap* a, const Bitmap* b)
        { return (a->width * a->height) < (b->width * b->height); });
    StopTimer("sorting bitmaps");

    StartTimer("packing bitmaps");
    // Pack the bitmaps
    while (!bitmaps.empty())
    {
        if (options.verbose)
            cout << "packing " << bitmaps.size() << " images..." << endl;
        auto packer = new Packer(options.width, options.height, options.padding);
        packer->Pack(bitmaps, options.verbose, options.unique, options.rotate);
        packers.push_back(packer);
        if (options.verbose)
            cout << "finished packing: " << name << (options.nozero && bitmaps.empty() ? "" : to_string(packers.size() - 1)) << " (" << packer->width << " x " << packer->height << ')' << endl;

        if (packer->bitmaps.empty())
        {
            cerr << "packing failed, could not fit bitmap: " << (bitmaps.back())->name << endl;
            return EXIT_FAILURE;
        }
    }
    StopTimer("packing bitmaps");

    bool noZero = options.nozero && packers.size() == 1;

    StartTimer("saving atlas png");
    // Save the atlas image
    for (size_t i = 0; i < packers.size(); ++i)
    {
        string pngName = outputDir + name + (noZero ? "" : to_string(i)) + ".png";
        if (options.verbose)
            cout << "writing png: " << pngName << endl;

        Color *colorPalette = nullptr;
        int paletteSize = 0;
        int transparentIndex = 0;

        if (options.paletteFilename)
        {
            Palette palette;
            palette.ReadPalette(options.paletteFilename, &colorPalette, &paletteSize, &transparentIndex);
            packers[i]->SavePng(pngName, reinterpret_cast<uint32_t*>(colorPalette), paletteSize);
            delete[] colorPalette;
        }
        else
        {
            packers[i]->SavePng(pngName, nullptr, 0);
        }
    }
    StopTimer("saving atlas png");

    StartTimer("saving atlas");
    // Save the atlas binary
    if (options.format == BIN)
    {
        SetStringType(options.binstr);
        if (options.verbose)
            cout << "writing bin: " << outputDir << name << ".bin" << endl;

        ofstream bin(outputDir + name + ".bin", ios::binary);
        
        if (!options.dirs)
        {
            WriteByte(bin, 'c');
            WriteByte(bin, 'r');
            WriteByte(bin, 'c');
            WriteByte(bin, 'h');
            WriteShort(bin, binVersion);
            WriteByte(bin, options.trim);
            WriteByte(bin, options.rotate);
            WriteByte(bin, options.binstr);
        }
        WriteShort(bin, (int16_t)packers.size());
        for (size_t i = 0; i < packers.size(); ++i)
            packers[i]->SaveBin(name + (noZero ? "" : to_string(i)), bin, options.trim, options.rotate, NAME_LENGTH);
        bin.close();
    }

    // Save the atlas xml
    if (options.format == XML)
    {
        if (options.verbose)
            cout << "writing xml: " << outputDir << name << ".xml" << endl;

        ofstream xml(outputDir + name + ".xml");
        if (!options.dirs)
        {
            xml << "<atlas>" << endl;
            xml << "\t<trim>" << (options.trim ? "true" : "false") << "</trim>" << endl;
            xml << "\t<rotate>" << (options.rotate ? "true" : "false") << "</trim>" << endl;
        }
        for (size_t i = 0; i < packers.size(); ++i)
            packers[i]->SaveXml(name + (noZero ? "" : to_string(i)), xml, options.trim, options.rotate);
        if (!options.dirs) xml << "</atlas>";
        xml.close();
    }

    // Save the atlas json
    if (options.format == JSON)
    {
        if (options.verbose)
            cout << "writing json: " << outputDir << name << ".json" << endl;

        ofstream json(outputDir + name + ".json");
        if (!options.dirs)
        {
            json << '{' << endl;
            json << "\t\"trim\":" << (options.trim ? "true" : "false") << ',' << endl;
            json << "\t\"rotate\":" << (options.rotate ? "true" : "false") << ',' << endl;
            json << "\t\"textures\":[" << endl;
        }
        for (size_t i = 0; i < packers.size(); ++i)
        {
            json << "\t\t{" << endl;
            packers[i]->SaveJson(name + (noZero ? "" : to_string(i)), json, options.trim, options.rotate);
            json << "\t\t}";
            if (!options.dirs)
            {
                if (i + 1 < packers.size())
                    json << ',';
                json << endl;
            }
        }
        if (!options.dirs)
        {
            json << "\t]" << endl;
            json << '}';
        }
        json.close();
    }
    StopTimer("saving atlas");

    // Save the new hash
    SaveHash(newHash, outputDir + name + ".hash");

    if (options.dirs) StopTimer(prefix);

    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    StartTimer("total");

    // Get the options
    options = (struct options)
    {
        .inputFilename = nullptr,
        .outputFilename = nullptr,
        .paletteFilename = nullptr,
        .size = 4096,
        .padding = 1,
        .binstr = NULL_TERMINATED,
        .format = XML,
        .alpha = true,
        .trim = false,
        .verbose = false,
        .ignore = false,
        .unique = false,
        .last = false,
        .dirs = false,
        .nozero = false
    };

    static option long_options[] = {
        {"format", required_argument, nullptr, 'f'},
        {"alpha", no_argument, nullptr, 'a'},
        {"trim", no_argument, nullptr, 't'},
        {"verbose", no_argument, nullptr, 'v'},
        {"ignore", no_argument, nullptr, 'i'},
        {"unique", no_argument, nullptr, 'u'},
        {"rotate", no_argument, nullptr, 'r'},
        {"last", no_argument, nullptr, 'l'},
        {"dirs", no_argument, nullptr, 'd'},
        {"nozero", no_argument, nullptr, 'n'},
        {"binstr", required_argument, nullptr, 'b'},
        {"size", required_argument, nullptr, 's'},
        {"width", required_argument, nullptr, 'w'},
        {"height", required_argument, nullptr, 'h'},
        {"padding", required_argument, nullptr, 'p'},
        {nullptr, 0, nullptr, 0}
    };

    int option;
    int option_index = 0;

    while ((option = getopt_long(argc, argv, "f:atvaiurldnb:s:w:h:p:", long_options, &option_index)) != -1) {
        switch (option) {
            case 'f':
                if (strcmp(optarg, "xml") == 0)
                    options.format = XML;
                else if (strcmp(optarg, "bin") == 0)
                    options.format = BIN;
                else if (strcmp(optarg, "json") == 0)
                    options.format = JSON;
                break;
            case 'a':
                options.alpha = true;
                break;
            case 't':
                options.trim = true;
                break;
            case 'v':
                options.verbose = true;
                break;
            case 'i':
                options.ignore = true;
                break;
            case 'u':
                options.unique = true;
                break;
            case 'r':
                options.rotate = true;
                break;
            case 'l':
                options.last = true;
                break;
            case 'd':
                options.dirs = true;
                break;
            case 'n':
                options.nozero = true;
                break;
            case 'b':
                options.binstr = GetBinStrType(optarg);
                break;
            case 's':
                options.size = GetPackSize(optarg);
                break;
            case 'w':
                options.width = GetPackSize(optarg);
                break;
            case 'h':
                options.height = GetPackSize(optarg);
                break;
            case 'p':
                options.padding = GetPadding(optarg);
                break;
            default:
                cout << helpMessage << endl;
                return EXIT_FAILURE;
        }
    }

    if (argc - optind < 2) {
        cout << helpMessage << endl;
        return EXIT_FAILURE;
    }

    options.inputFilename = argv[optind];
    options.outputFilename = argv[optind + 1];

    // Get the output directory and name
    string outputDir, name;
    SplitFileName(options.inputFilename, &outputDir, &name, nullptr);

    // Get all the input files and directories
    vector<string> inputs;
    stringstream ss(options.outputFilename);
    while (ss.good())
    {
        string inputStr;
        getline(ss, inputStr, ',');
        inputs.push_back(inputStr);
    }

    if (argc - optind > 2)
    {
        options.paletteFilename = argv[optind + 2];
        if (access(options.paletteFilename, F_OK) == -1)
        {
            std::cout << options.paletteFilename << " cannot be found" << std::endl;
            return EXIT_FAILURE;
        }
    }

    if (options.width == 0) options.width = options.size;
    if (options.height == 0) options.height = options.size;

    if (options.verbose)
    {
        cout << "options..." << endl;
        cout << "\t--format: " << string(GetFormatString(options.format)) << endl;
        cout << "\t--alpha: " << (options.alpha ? "true" : "false") << endl;
        cout << "\t--trim: " << (options.trim ? "true" : "false") << endl;
        cout << "\t--verbose: " << (options.verbose ? "true" : "false") << endl;
        cout << "\t--ignore: " << (options.ignore ? "true" : "false") << endl;
        cout << "\t--unique: " << (options.unique ? "true" : "false") << endl;
        cout << "\t--rotate: " << (options.rotate ? "true" : "false") << endl;
        if(options.width == options.height) cout << "\t--size: " << options.width << endl;
        else
        {
            cout << "\t--width: " << options.width << endl;
            cout << "\t--height: " << options.height << endl;
        }
        cout << "\t--padding: " << options.padding << endl;
        cout << "\t--binstr: " << (options.binstr == NULL_TERMINATED ? "n" : (options.binstr == PREFIXED ? "p" : "7")) << endl;
        cout << "\t--last: " << (options.last ? "true" : "false") << endl;
        cout << "\t--dirs: " << (options.dirs ? "true" : "false") << endl;
        cout << "\t--nozero: " << (options.nozero ? "true" : "false") << endl;
    }

    StartTimer("hashing input");
    // Hash the arguments and input directories
    size_t newHash = 0;
    for (int i = 1; i < argc; ++i)
        HashString(newHash, argv[i]);
    StopTimer("hashing input");
    
    if (!options.dirs)
    {
        int result = Pack(newHash, outputDir, name, inputs);

        if (result != EXIT_SUCCESS)
            return result;

        StopTimer("total");

        WriteAllTimers();

        return EXIT_SUCCESS;
    }

    string newInput, namePrefix;
    vector<string> subdirs;

    for (string &input : inputs)
    {
        if (!input.ends_with(".png"))
        {
            newInput = input;
            break;
        }
    }

    if (newInput.empty())
    {
        cerr << "could not find directories in input" << endl;
        return EXIT_FAILURE;
    }

    namePrefix = name + "_";

    GetSubdirs(newInput, subdirs);

    bool skipped = true;
    for (string& subdir : subdirs)
    {
        string newName = GetFileName(subdir), prefixedName = namePrefix + newName;
        vector<string> input{ subdir };
        int result = Pack(newHash, outputDir, prefixedName, input, newName + "/");
        if (result == EXIT_SUCCESS)
            skipped = false;
        else if(result != EXIT_SKIPPED)
            return result;

        packers.clear();
        bitmaps.clear();
    }

    if (skipped)
    {
        cout << "atlas is unchanged: " << name << endl;

        StopTimer("total");
        WriteAllTimers();
        return EXIT_SUCCESS;
    }

    RemoveFile(outputDir + name + ".bin");
    RemoveFile(outputDir + name + ".xml");
    RemoveFile(outputDir + name + ".json");

    StartTimer("saving atlas");
    vector<string> cachedPackers;
    if (options.format == BIN)
    {
        SetStringType(options.binstr);
        if (options.verbose)
            cout << "writing bin: " << outputDir << name << ".bin" << endl;

        vector<ifstream*> cacheFiles;

        FindPackers(outputDir, namePrefix, "bin", cachedPackers);

        ofstream bin(outputDir + name + ".bin", ios::binary);
        WriteByte(bin, 'c');
        WriteByte(bin, 'r');
        WriteByte(bin, 'c');
        WriteByte(bin, 'h');
        WriteShort(bin, binVersion);
        WriteByte(bin, options.trim);
        WriteByte(bin, options.rotate);
        WriteByte(bin, options.binstr);
        int16_t imageCount = 0;
        for (size_t i = 0; i < cachedPackers.size(); ++i)
        {
            ifstream binCache(cachedPackers[i], ios::binary);
            imageCount += ReadShort(binCache);
            binCache.close();
        }
        WriteShort(bin, imageCount);
        for (size_t i = 0; i < cachedPackers.size(); ++i)
        {
            ifstream binCache(cachedPackers[i], ios::binary);
            ReadShort(binCache);
            bin << binCache.rdbuf();
            binCache.close();
        }
        bin.close();
    }
     
    if (options.format == XML)
    {
        if (options.verbose)
            cout << "writing xml: " << outputDir << name << ".xml" << endl;

        cachedPackers.clear();

        FindPackers(outputDir, namePrefix, "xml", cachedPackers);

        ofstream xml(outputDir + name + ".xml");
        xml << "<atlas>" << endl;
        xml << "\t<trim>" << (options.trim ? "true" : "false") << "</trim>" << endl;
        xml << "\t<rotate>" << (options.rotate ? "true" : "false") << "</trim>" << endl;
        for (size_t i = 0; i < cachedPackers.size(); ++i)
        {
            ifstream xmlCache(cachedPackers[i]);
            xml << xmlCache.rdbuf();
            xmlCache.close();
        }
        xml << "</atlas>";
        xml.close();
    }

    if (options.format == JSON)
    {
        if (options.verbose)
            cout << "writing json: " << outputDir << name << ".json" << endl;

        cachedPackers.clear();

        FindPackers(outputDir, namePrefix, "json", cachedPackers);

        ofstream json(outputDir + name + ".json");
        json << '{' << endl;
        json << "\t\"trim\":" << (options.trim ? "true" : "false") << ',' << endl;
        json << "\t\"rotate\":" << (options.rotate ? "true" : "false") << ',' << endl;
        json << "\t\"textures\":[" << endl;
        for (size_t i = 0; i < cachedPackers.size(); ++i)
        {
            ifstream jsonCache(cachedPackers[i]);
            json << jsonCache.rdbuf();
            jsonCache.close();
            if (i + 1 < cachedPackers.size())
                json << ',';
            json << endl;
        }
        json << "\t]" << endl;
        json << '}';
        json.close();
    }

    StopTimer("saving atlas");

    StopTimer("total");

    WriteAllTimers();

    return EXIT_SUCCESS;
}
