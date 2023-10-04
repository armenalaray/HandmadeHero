#ifndef ASSET_H
#define ASSET_H

struct loaded_sound
{
    int16 * Samples[2];
    uint32 SampleCount;  
    uint32 ChannelCount;
};

struct loaded_font
{
    hha_font_glyph * Glyphs;
    r32 * KerningTable;
    u16 * GlyphIndexFromCodePoint;
    
    s32 BitmapIDOffset;
};

enum asset_state
{
    AssetState_Unloaded,
    AssetState_Queued,
    AssetState_Loaded,
};


enum debug_text_operation
{
    DebugTextOperation_DrawText,
    DebugTextOperation_SizeText,
};


struct asset_memory_header
{
    asset_memory_header * Prev;
    asset_memory_header * Next;
    u32 AssetIndex;
    u64 TotalSize;
    u32 GenerationIndex;
    union
    {
        loaded_bitmap Bitmap;
        loaded_sound Sound;
        loaded_font Font;
    };
};


struct asset
{
    u32 State;
    asset_memory_header * Header;
    
    hha_asset HHA;
    u32 FileIndex;
};

struct asset_vector
{
    real32 E[Tag_Count];
};

struct asset_type
{
    uint32 FirstAssetIndex;
    uint32 OnePastLastAssetIndex;
};

struct asset_file
{
    platform_file_handle Handle;
    hha_header Header;
    hha_asset_type * AssetTypeArray;
    u32 TagBase;
    s32 FontBitmapIDOffset;
};

enum asset_memory_flags
{
    AssetMemory_Used = 0x1
};

struct asset_memory_block
{
    asset_memory_block * Prev;
    asset_memory_block * Next;
    
    memory_index MemoryFlags;
    memory_index Size;
};

//NOTE: Asset is a form to relate the tags to a specific asset entrance/slot   
struct game_assets
{
    struct transient_state * TranState;
    asset_memory_block MemorySentinel;
    asset_memory_header HeaderSentinel;
    
    u32 NextGenerationIndex;
    
    u32 FileCount;
    asset_file * AssetFiles;
    
    real32 TagRange[Tag_Count];
    uint32 TagCount;
    hha_tag * Tags;
    
    uint32 AssetCount;
    asset * Assets;
    
    asset_type AssetTypes[Asset_Count];
    
    u32 OperationLock;
    
    u32 InflightGenerationCount;
    u32 InflightGenerations[16];
};

inline bool32
IsValid(font_id ID)
{
    bool32 Result = (ID.Value != 0);
    return Result;
}
inline bool32
IsValid(bitmap_id ID)
{
    bool32 Result = (ID.Value != 0);
    return Result;
}

inline bool32
IsValid(sound_id ID)
{
    bool32 Result = (ID.Value != 0);
    return Result;
}



internal void 
AddMemoryHeaderToList(game_assets * Assets, asset_memory_header * Header)
{
    asset_memory_header * Sentinel = &Assets->HeaderSentinel; 
    
    Header->Next = Sentinel->Next;
    Header->Prev = Sentinel;
    
    //TODO: This will need a lock
    Header->Next->Prev = Header;
    Header->Prev->Next = Header;
    
    
    
}

internal void
RemoveMemoryHeaderFromList(asset_memory_header * Header)
{
    Assert(Header);
    Assert(Header->Prev);
    
    Header->Prev->Next = Header->Next;
    Header->Next->Prev = Header->Prev;
}


internal void
BeginOperationLock(game_assets * Assets)
{
    if(Assets)
    {
        for(;;)
        {
            if(AtomicCompareExchangeUint32(&Assets->OperationLock, 1,0) == 0)
            {
                break;
            }
        }
    }
}

internal void 
EndOperationLock(game_assets * Assets)
{
    CompletePreviousWritesBeforeFututeWrites;
    if(Assets)
    {
        Assets->OperationLock = 0;
    }
}

inline asset_memory_header * 
GetAsset(game_assets * Assets, u32 ID , u32 GenerationIndex)
{
    Assert(ID < Assets->AssetCount);
    asset * Asset = Assets->Assets + ID;
    
    asset_memory_header * Result = 0;
    
    BeginOperationLock(Assets);
    
    asset_state State = (asset_state)Asset->State;
    if(Asset->State == AssetState_Loaded)
    {
        Result = Asset->Header;
        RemoveMemoryHeaderFromList(Result);
        AddMemoryHeaderToList(Assets, Result);
        
        if(Asset->Header->GenerationIndex < GenerationIndex)
        {
            Asset->Header->GenerationIndex = GenerationIndex;
        }
        
        CompletePreviousReadsBeforeFutureReads;
    }
    
    EndOperationLock(Assets);
    
    return Result;
}

inline loaded_bitmap * 
GetBitmap(game_assets * Assets, bitmap_id ID, u32 GenerationIndex)
{
    asset_memory_header * Header = GetAsset(Assets, ID.Value, GenerationIndex);
    loaded_bitmap * Result =  (Header ? &Header->Bitmap : 0);
    return Result;
}

inline loaded_sound * 
GetSound(game_assets * Assets, sound_id ID, u32 GenerationIndex)
{
    asset_memory_header * Header = GetAsset(Assets, ID.Value, GenerationIndex);  
    loaded_sound * Result = Header ? &Header->Sound : 0;
    return Result;
}

inline loaded_font * 
GetFont(game_assets * Assets, font_id ID, u32 GenerationIndex)
{
    asset_memory_header * Header = GetAsset(Assets, ID.Value, GenerationIndex);  
    loaded_font * Result = Header ? &Header->Font : 0;
    return Result;
}


inline hha_bitmap *
GetBitmapInfo(game_assets * Assets, bitmap_id ID)
{
    Assert(ID.Value < Assets->AssetCount);
    hha_bitmap * Bitmap = &Assets->Assets[ID.Value].HHA.Bitmap;
    return Bitmap;
}

inline hha_sound *
GetSoundInfo(game_assets * Assets, sound_id ID)
{
    Assert(ID.Value < Assets->AssetCount);
    hha_sound * Sound = &Assets->Assets[ID.Value].HHA.Sound;
    return Sound;
}


inline hha_font *
GetFontInfo(game_assets * Assets, font_id ID)
{
    Assert(ID.Value < Assets->AssetCount);
    hha_font * Font = &Assets->Assets[ID.Value].HHA.Font;
    return Font;
}

internal void LoadBitmap(game_assets * Assets, bitmap_id ID, b32 Immediate);
inline void PrefetchBitmap(game_assets * Assets, bitmap_id ID) 
{
    LoadBitmap(Assets, ID, false);
}

internal void LoadSound(game_assets * Assets, sound_id ID);
inline void PrefetchSound(game_assets * Assets, sound_id ID) 
{
    LoadSound(Assets, ID);
}

inline sound_id
GetNextSoundInChain(game_assets * Assets, sound_id ID)
{
    sound_id Result = {};
    hha_sound * Sound = GetSoundInfo(Assets, ID);
    
    switch(Sound->Chain)
    {
        case HHASound_None:
        {
            //NOTE: Nothing to do
        }break;
        case HHASound_Advance:
        {
            Result.Value = ID.Value + 1;
        }break;
        case HHASound_Loop:
        {
            Result = ID;
        }break;
        default:
        {
            InvalidCodePath;
        }break;
    }
    
    return Result;
}

#if 1
internal task_with_memory * BeginTaskWithMemory(transient_state * TranState);
internal void EndTaskWithMemory(task_with_memory * Task);
#endif

#endif
