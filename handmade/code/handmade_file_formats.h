#ifndef HANDMADE_FILE_FORMATS_H
#define  HANDMADE_FILE_FORMATS_H


enum asset_font_types
{
    FontType_Default = 0,
    FontType_Debug = 10,
};

enum asset_tag_id
{
    Tag_Smoothness,
    Tag_Size,
    Tag_Color,
    Tag_FacingDirection,
    Tag_UnicodeCodepoint,
    Tag_FontType, //0 Default Game Font, 10 DEBUG Font
    
    Tag_Count
};


enum asset_type_id
{
    //
    //NOTE: Bitmaps
    //
    
    Asset_Null,
    Asset_Shadow,
    Asset_Tree,
    Asset_Sword,
    
    Asset_Stone,
    Asset_Tuft,
    Asset_Grass,
    Asset_Stair,
    
    Asset_Head,
    Asset_Cape,
    Asset_Torso,
    
    Asset_Font,
    Asset_FontGlyph,
    Asset_BitmapCount,
    
    //
    //NOTE: Sounds
    //
    
    Asset_Bloop,
    Asset_Crack,
    
    Asset_Drop, //13
    Asset_Glide, //14
    
    Asset_Music,
    Asset_Puhp,
    
    Asset_Count
};


#define HHA_CODE(a,b,c,d) (((u32)(a) << 0) | ((u32)(b) << 8) | ((u32)(c) << 16) | ((u32)(d) << 24))

#pragma pack(push, 1)
struct hha_header
{
#define HHA_MAGIC_VALUE HHA_CODE('h','h','a','f')
    u32 MagicValue;
    
#define HHA_VERSION 0 
    u32 Version;
    
    u32 TagCount; 
    u32 AssetTypeCount;
    u32 AssetCount;
    
    u64 TagOffset;
    u64 AssetTypeOffset;
    u64 AssetOffset;
    
    //TODO: Primacy numbers for asset files? 
    /*
    TODO: 
    u32 RemoveCount;
    hha_asset_removal AssetRemoval;
    
    struct hha_asset_removal
    {
    -  u32 FileGUID[8];
    -  u32 AssetIndex;
    };
    
    */
};


struct hha_tag
{
    u32 ID;
    real32 Value;
};


struct hha_asset_type
{
    u32 TypeID; 
    u32 FirstAssetIndex;
    u32 OnePastLastAssetIndex;
};


struct hha_bitmap
{
    u32 Dim[2];
    r32 AlignPorcentage[2];
    /*
    hha_bitmap data:
    
    u32 Pixels[Width][Height];
    */
};

enum hha_sound_chain
{
    HHASound_None,
    HHASound_Advance,
    HHASound_Loop
};

struct hha_sound
{
    u32 SampleCount;
    u32 ChannelCount;
    hha_sound_chain Chain;
    /*
    hha_sound data:
    
    int16 Samples[ChannelCount][SampleCount];
    */
};

struct hha_font_glyph
{
    u32 UnicodeCodePoint;
    bitmap_id BitmapID;
};

struct hha_font
{
    u32 OnePastLastCodePoint;
    u32 GlyphCount;
    r32 ExternalLeading;
    r32 AscenderHeight;
    r32 DescenderHeight;
    /*
    hha_font data:
    
    hha_font_glyph Glyphs[GlyphCount];
     r32 KerningTable[GlyphCount][GlyphCount];
    */
};

struct hha_asset
{
    u64 DataOffset;
    u32 FirstTagIndex;
    u32 OnePastLastTagIndex;
    
    union
    {
        hha_bitmap Bitmap;
        hha_sound Sound;
        hha_font Font;
    };
};


#pragma pack(pop)
#endif
