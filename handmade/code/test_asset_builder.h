#ifndef TEST_ASSET_BUILDER
#define TEST_ASSET_BUILDER

#include "stdio.h"
#include "stdlib.h"
#include "handmade_platform.h"
#include "handmade_file_formats.h"
#include "handmade_intrinsics.h"
#include "handmade_math.h"

#define MAX_BITMAP_HEIGHT 1024
#define MAX_BITMAP_WIDTH 1024

#define ONE_PAST_MAX_CODE_POINT (0x10FFFF + 1)

#define USING_WINDOWS_FONTS 1
#ifdef USING_WINDOWS_FONTS
#include <Windows.h>
#else
#define STB_TRUETYPE_IMPLEMENTATION 1
#include "stb_truetype.h"
#endif


enum asset_type
{
    AssetType_Bitmap,
    AssetType_Sound,
    AssetType_Font,
    AssetType_FontGlyph,
    AssetType_Count,
};

struct loaded_font
{
    HFONT Win32Handle;
    TEXTMETRICW TextMetric;
    
    hha_font_glyph * Glyphs;
    r32 * KerningTable;
    
    u32 FirstGLyph;
    u32 LastGlyph;
    u32 MaxGlyphCount;
    u32 GlyphCount;
    
    u32 * GlyphIndexFromCodePoint;
    
    u32 OnePastLastCodePoint;
};

struct asset_source_font
{
    loaded_font * LoadedFont;
};

struct asset_source_font_glyph
{
    loaded_font * LoadedFont;
    u32 CodePoint;
};

struct asset_source_bitmap
{
    char * FileName;
};

struct asset_source_sound
{
    char * FileName;
    u32 FirstSampleIndex;
};

//NOTE: Asset is a form to relate the tags to a specific asset entrance/slot   
struct asset_source
{
    asset_type AssetType;
    union
    {
        asset_source_bitmap Bitmap;
        asset_source_sound Sound;
        asset_source_font Font;
        asset_source_font_glyph FontGlyph;
    };
};

//TODO: Static size array check if bigger size necessarry 
#define LARGE_NUMBER 4096

struct game_assets
{
    uint32 TagCount;
    hha_tag Tags[LARGE_NUMBER];
    
    uint32 AssetCount;
    asset_source Sources[LARGE_NUMBER];
    hha_asset Assets[LARGE_NUMBER];
    
    u32 AssetTypeCount;
    hha_asset_type AssetTypes[Asset_Count];
    
    hha_asset_type * DEBUGAssetType; 
    u32 AssetIndex; 
};

#endif