#include "handmade_asset.h"


inline b32
MemoryIsUsed(asset_memory_block * Block)
{
    b32 Result = (Block->MemoryFlags & AssetMemory_Used);
    return Result;
}

internal void
InsertBlock(asset_memory_block * Prev, memory_index MemorySize, void * Dest)
{
    
    Assert(MemorySize >= sizeof(asset_memory_block));
    asset_memory_block * MemoryBlock = (asset_memory_block *)Dest; 
    MemoryBlock->MemoryFlags = 0;
    MemoryBlock->Size = MemorySize - sizeof(asset_memory_block);
    //NOTE: This will add memory blocks to the list's end
    MemoryBlock->Prev = Prev;
    MemoryBlock->Next = Prev->Next;
    
    MemoryBlock->Prev->Next = MemoryBlock;
    MemoryBlock->Next->Prev = MemoryBlock;
}

internal asset_memory_block * 
LookForBlockOfSize(game_assets * Assets, memory_index Size)
{
    TIMED_FUNCTION();
    
    //NOTE: This in the future probably would need to be accelerated once our 
    //resident asset count grows 8000
    //TODO: Find the best match of block size
    asset_memory_block * Result = 0;
    for(asset_memory_block * Block = Assets->MemorySentinel.Next;
        Block != &Assets->MemorySentinel;
        Block = Block->Next)
    {
        if(Size <= Block->Size)
        {
            if(!MemoryIsUsed(Block))
            {
                Result = Block;
            }
        }
    }
    
    return Result;
}

internal b32
MergeIfPossible(game_assets * Assets, asset_memory_block * First, asset_memory_block * Second)
{
    TIMED_FUNCTION();
    
    b32 Result = false;
    
    if(First != &Assets->MemorySentinel && 
       Second != &Assets->MemorySentinel)
    {
        if(!MemoryIsUsed(First) &&
           !MemoryIsUsed(Second))
        {
            //NOTE: Check if memory is contiguous between First and second block
            u8 * ExpectedSecond = (u8*)First + sizeof(asset_memory_block) + First->Size;
            if(ExpectedSecond == (u8*)Second)
            {
                Second->Prev->Next =  Second->Next;
                Second->Next->Prev =  Second->Prev;
                First->Size += (Second->Size + sizeof(asset_memory_block));
                Result = true;
            }
        }
    }
    return Result;
}

internal b32
GenerationHasCompleted(game_assets * Assets, u32 GenerationIndex)
{
    TIMED_FUNCTION();
    
    b32 Result = true;
    
    for(u32 Index = 0; 
        Index < Assets->InflightGenerationCount;
        ++Index)
    {
        //NOTE: We know that some RenderGeneration is using the asset yet
        if(Assets->InflightGenerations[Index] == GenerationIndex)
        {
            Result = false;
            break;
        }
    }
    
    return Result;
}

inline asset_memory_header * 
AcquireAssetMemory(game_assets * Assets, memory_index Size, u32 AssetIndex)
{
    TIMED_FUNCTION();
    
    asset_memory_header * Result = 0;
    
    BeginOperationLock(Assets);
    
    asset_memory_block * Block = LookForBlockOfSize(Assets, Size);
    for(;;)
    {
        if(Block)
        {
            Block->MemoryFlags |= AssetMemory_Used;
            Result = (asset_memory_header *)(Block + 1);
            
            Assert(Size <= Block->Size);
            
            u64 RemainingSize = Block->Size - Size;
            u64 MinimumBlockSize = 4096;//TODO: Compute this based on the smallest asset size
            
            if(RemainingSize > MinimumBlockSize)
            {
                Block->Size = Size;
                InsertBlock(Block, RemainingSize, (u8*)Result + Size);
            }
            else
            {
                //TODO: Record free space inside blocks so we can do block merging once a neighgbor 
                //blocks get freed 
            }
            
            break;
        }
        else
        {
            //NOTE: this will crash
            //TODO : check for blocks that can fit for size
            for(asset_memory_header * Header = Assets->HeaderSentinel.Prev;
                Header != &Assets->HeaderSentinel; 
                Header = Header->Prev)
            {
                asset * Asset  = Assets->Assets + Header->AssetIndex;
                //NOTE: Prevents Freeing  Memory of asset data noloaded yet
                //TODO: Check if asset is operating before removing it
                if(Asset->State >= AssetState_Loaded && 
                   GenerationHasCompleted(Assets, Asset->Header->GenerationIndex))
                {
                    Assert(Asset->State == AssetState_Loaded);
                    RemoveMemoryHeaderFromList(Asset->Header);
                    
                    Block = (asset_memory_block * )Asset->Header - 1;
                    Block->MemoryFlags &= ~AssetMemory_Used;
                    
                    if(MergeIfPossible(Assets, Block->Prev, Block))
                    {
                        Block = Block->Prev;
                    }
                    
                    MergeIfPossible(Assets, Block, Block->Next);
                    
                    Asset->Header = 0;
                    Asset->State = AssetState_Unloaded;
                    
                    if(Size <= Block->Size)
                    {
                        break;
                    }
                }
            }
        }
    }
    
    if(Result)
    {
        Result->TotalSize = Size;
        Result->AssetIndex = AssetIndex;
        Result->GenerationIndex = 0;
        AddMemoryHeaderToList(Assets, Result);
    }
    
    EndOperationLock(Assets);
    
    return Result;
}


internal asset_file *  
GetFile(game_assets * Assets, u32 FileIndex)
{
    Assert(FileIndex < Assets->FileCount);
    asset_file * File  = Assets->AssetFiles + FileIndex;
    return File;
}


internal platform_file_handle *  
GetFileHandleFor(game_assets * Assets, u32 FileIndex)
{
    return &GetFile(Assets, FileIndex)->Handle;
}

enum finalize_asset_work
{
    FinalizeAsset_None,
    FinalizeAsset_Font
};

struct load_asset_work
{
    task_with_memory * Task;
    platform_file_handle * Handle;
    asset * Asset;
    
    u64 Offset;
    u64 Size;
    
    void * Dest;
    u32 FinalState;
    
    finalize_asset_work FinalizeAssetWork;
};

internal void 
LoadAssetWorkDirectly(load_asset_work * Work)
{
    TIMED_FUNCTION();
    
    //NOTE: This is threaded 
    Platform.ReadDataFromFile(Work->Handle, Work->Offset, Work->Size, Work->Dest);
    if(PlatformNoFileErrors(Work->Handle))
    {
        switch(Work->FinalizeAssetWork)
        {
            case FinalizeAsset_None:
            {
                //NOTE: Nothing to do
            }break;
            
            case FinalizeAsset_Font:
            {
                //NOTE: build mapping table
                loaded_font * Font = &Work->Asset->Header->Font;
                hha_font * HHAFont = &Work->Asset->HHA.Font;
                
                for(u32 GlyphIndex = 0;
                    GlyphIndex < HHAFont->GlyphCount;
                    ++GlyphIndex)
                {
                    hha_font_glyph * Glyph = Font->Glyphs + GlyphIndex;
                    Assert(Glyph->UnicodeCodePoint < HHAFont->OnePastLastCodePoint);
                    Assert((u32)((u16)GlyphIndex) == GlyphIndex);
                    Font->GlyphIndexFromCodePoint[Glyph->UnicodeCodePoint] = (u16)GlyphIndex;
                }
            }break;
            default:
            {
                InvalidCodePath;
            }break;
        }
    }
    
    CompletePreviousWritesBeforeFututeWrites;
    
    if(!PlatformNoFileErrors(Work->Handle))
    {
        ZeroSize(Work->Size, Work->Dest);
    }
    
    Work->Asset->State = Work->FinalState;
}

internal PLATFORM_WORK_QUEUE_CALLBACK(LoadAssetWork)
{
    load_asset_work * Work  = (load_asset_work *)Data;
    
    LoadAssetWorkDirectly(Work);
    EndTaskWithMemory(Work->Task);
}

struct asset_memory_size
{
    u32 Total;
    u32 Data;
    u32 Section;
};

internal void 
LoadBitmap(game_assets * Assets, bitmap_id ID, b32 Immediate = false) 
{
    TIMED_FUNCTION();
    
    asset * Asset = Assets->Assets + ID.Value;
    if(IsValid(ID))
    {
        if(AtomicCompareExchangeUint32((uint32 *)&Asset->State, AssetState_Queued, AssetState_Unloaded) 
           == AssetState_Unloaded)
        {
            task_with_memory * Task = 0;
            if(!Immediate)
            {
                Task = BeginTaskWithMemory(Assets->TranState);
            }
            
            if(Task || Immediate)
            {
                hha_asset * HHA = &Asset->HHA;
                asset_memory_size Size = {};  
                
                s32 Width = HHA->Bitmap.Dim[0];
                s32 Height = HHA->Bitmap.Dim[1];
                Size.Section = Width * BITMAP_BYTES_PER_PIXEL;
                Size.Data = Size.Section * Height;
                Size.Total = Size.Data + sizeof(asset_memory_header); 
                
                Asset->Header = AcquireAssetMemory(Assets, Size.Total, ID.Value);
                
                loaded_bitmap * LoadedBitmap = &Asset->Header->Bitmap;
                
                LoadedBitmap->AlignPorcentage = Vector2(HHA->Bitmap.AlignPorcentage[0], 
                                                        HHA->Bitmap.AlignPorcentage[1]);
                LoadedBitmap->Width = Width;
                LoadedBitmap->Height = Height;
                LoadedBitmap->WidthOverHeight = (r32)LoadedBitmap->Width / (r32)LoadedBitmap->Height;
                LoadedBitmap->Pitch = Size.Section;
                LoadedBitmap->Memory = (Asset->Header + 1);
                
                load_asset_work Work = {};
                Work.Task = Task;
                Work.Handle = GetFileHandleFor(Assets, Asset->FileIndex);
                Work.Asset = Asset;
                Work.Offset = HHA->DataOffset;
                Work.Size = Size.Data;
                Work.Dest = LoadedBitmap->Memory;
                Work.FinalState = (AssetState_Loaded);
                Work.FinalizeAssetWork = FinalizeAsset_None;
                
                if(Task)
                {
                    load_asset_work * TaskWork = PushStruct(&Task->Arena, load_asset_work);
                    *TaskWork = Work;
                    Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadAssetWork, TaskWork);
                }
                else
                {
                    Assert(Immediate);
                    LoadAssetWorkDirectly(&Work);
                }
                
            }
            else
            {
                Asset->State = AssetState_Unloaded;
            }
        }
        else if(Immediate)
        {
            //TODO: Do we want to have an ordered process when a pair of threads 
            //collides on the same bitmap? 
            asset_state volatile * State = (asset_state volatile *)&Asset->State;
            while(*State == AssetState_Queued){}
        }
    }
}

internal void 
LoadSound(game_assets * Assets, sound_id ID) 
{
    TIMED_FUNCTION();
    
    asset * Asset = Assets->Assets + ID.Value;
    
    if(IsValid(ID) && 
       (AtomicCompareExchangeUint32((uint32 *)&Asset->State, 
                                    AssetState_Queued, AssetState_Unloaded) 
        == AssetState_Unloaded))
    {
        task_with_memory * Task = BeginTaskWithMemory(Assets->TranState);
        if(Task)
        {
            hha_asset * HHA = &Asset->HHA;
            
            asset_memory_size Size = {};
            Size.Section = HHA->Sound.ChannelCount;
            Size.Data = HHA->Sound.SampleCount * Size.Section * sizeof(16);
            Size.Total = Size.Data + sizeof(asset_memory_header);
            
            Asset->Header = AcquireAssetMemory(Assets, Size.Total, ID.Value);
            
            loaded_sound * LoadedSound = &Asset->Header->Sound;
            
            LoadedSound->SampleCount = HHA->Sound.SampleCount;
            LoadedSound->ChannelCount = Size.Section;
            
            Assert(LoadedSound->ChannelCount < ArrayCount(LoadedSound->Samples));
            
            void * Memory = (Asset->Header + 1);
            
            u32 Offset = 0;
            for(u32 ChannelIndex = 0;
                ChannelIndex < LoadedSound->ChannelCount;
                ++ChannelIndex)
            {
                LoadedSound->Samples[ChannelIndex] = (s16*)Memory + Offset;    
                Offset += LoadedSound->SampleCount;
            }
            
            load_asset_work * Work = PushStruct(&Task->Arena, load_asset_work);
            Work->Task = Task;
            Work->Handle = GetFileHandleFor(Assets, Asset->FileIndex);
            Work->Asset = Asset;
            Work->Offset = HHA->DataOffset;
            Work->Size = Size.Data;
            Work->Dest = Memory;
            
            Work->FinalState = (AssetState_Loaded);
            Work->FinalizeAssetWork = FinalizeAsset_None;
            
            Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadAssetWork, Work);
        }
        else
        {
            Asset->State = AssetState_Unloaded;
        }
    }
}


internal void 
LoadFont(game_assets * Assets, font_id ID) 
{
    TIMED_FUNCTION();
    
    asset * Asset = Assets->Assets + ID.Value;
    //TODO: Merge this boilerplate
    if(IsValid(ID) && 
       (AtomicCompareExchangeUint32((uint32 *)&Asset->State, 
                                    AssetState_Queued, AssetState_Unloaded) 
        == AssetState_Unloaded))
    {
        task_with_memory * Task = BeginTaskWithMemory(Assets->TranState);
        if(Task)
        {
            hha_asset * HHA = &Asset->HHA;
            
            u32 GlyphsSize = HHA->Font.GlyphCount * sizeof(hha_font_glyph); 
            u32 KerningTableSize = HHA->Font.GlyphCount * HHA->Font.GlyphCount * sizeof(r32);
            u32 DataSize =  GlyphsSize + KerningTableSize;
            
            //NOTE: This data is not stored inside the file, we will construct it on the fly
            u32 GlyphIndexFromCodePointSize = sizeof(u16) * HHA->Font.OnePastLastCodePoint;  
            u32 TotalSize = DataSize + sizeof(asset_memory_header) + GlyphIndexFromCodePointSize;
            
            Asset->Header = AcquireAssetMemory(Assets, TotalSize, ID.Value);
            
            loaded_font * LoadedFont = &Asset->Header->Font;
            
            LoadedFont->BitmapIDOffset = GetFile(Assets, Asset->FileIndex)->FontBitmapIDOffset;
            
            LoadedFont->Glyphs = (hha_font_glyph *)(Asset->Header + 1);
            LoadedFont->KerningTable = (r32*)((u8*)LoadedFont->Glyphs + GlyphsSize);
            LoadedFont->GlyphIndexFromCodePoint = (u16*)((u8*)LoadedFont->KerningTable + KerningTableSize);
            
            //NOTE: Need to erase memory so we don't have trash data
            ZeroSize(GlyphIndexFromCodePointSize, LoadedFont->GlyphIndexFromCodePoint);
            
            load_asset_work * Work = PushStruct(&Task->Arena, load_asset_work);
            Work->Task = Task;
            Work->Handle = GetFileHandleFor(Assets, Asset->FileIndex);
            Work->Asset = Asset;
            Work->Offset = HHA->DataOffset;
            Work->Size = DataSize;
            Work->Dest = LoadedFont->Glyphs;
            
            Work->FinalState = (AssetState_Loaded);
            Work->FinalizeAssetWork = FinalizeAsset_Font;
            
            Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadAssetWork, Work);
        }
        else
        {
            Asset->State = AssetState_Unloaded;
        }
    }
}

internal uint32  
GetBestMatchAssetFrom(game_assets * Assets, asset_type_id TypeID, asset_vector * MatchVector, 
                      asset_vector * WeightVector)
{
    TIMED_FUNCTION();
    
    uint32 Result = {};
    
    real32 BestDiff = Real32Max;
    
    asset_type * Type = Assets->AssetTypes + TypeID;
    for(uint32 AssetIndex = Type->FirstAssetIndex;
        AssetIndex < Type->OnePastLastAssetIndex; 
        ++AssetIndex)
    {
        asset * Asset = Assets->Assets + AssetIndex;
        
        real32 TotalWeightedDiff = 0;
        
        //NOTE: Here we iterate on the tags the asset has only 
        for(uint32 TagIndex = Asset->HHA.FirstTagIndex;
            TagIndex <  Asset->HHA.OnePastLastTagIndex;
            ++TagIndex)
        {
            
            hha_tag * Tag = Assets->Tags + TagIndex;
            
            real32 A = Tag->Value;
            real32 B = MatchVector->E[Tag->ID];
            
            real32 D0 = AbsValue(A-B);
            real32 D1 = AbsValue((A - Assets->TagRange[Tag->ID] * SignOf(A)) - B);
            
            real32 Difference = Minimum(D0, D1);
            
            real32 Weighted = WeightVector->E[Tag->ID] * AbsValue(Difference); 
            
            TotalWeightedDiff += Weighted;
        }
        
        if(BestDiff > TotalWeightedDiff)
        {
            BestDiff = TotalWeightedDiff;
            Result = AssetIndex;
        }
    }
    
    return Result;
}


internal uint32   
GetRandomAssetFrom(game_assets * Assets, random_series * Series, asset_type_id TypeID)
{
    TIMED_FUNCTION();
    
    uint32 Result = {};
    
    asset_type * Type = Assets->AssetTypes + TypeID;
    int32 AssetCountOfType = Type->OnePastLastAssetIndex - Type->FirstAssetIndex;
    if(AssetCountOfType)
    {
        uint32 IndexChoice = Type->FirstAssetIndex + RandomChoice(Series, AssetCountOfType);
        Result = IndexChoice;
    }
    
    return Result;
}



inline uint32
GetFirstAssetFrom(game_assets * Assets, asset_type_id TypeID)
{
    TIMED_FUNCTION();
    
    uint32 Result = 0;
    
    asset_type * Type = Assets->AssetTypes + TypeID;
    if(Type->FirstAssetIndex != Type->OnePastLastAssetIndex)
    {
        Result = Type->FirstAssetIndex;
    }
    
    return Result;
}

internal font_id 
GetBestMatchFontFrom(game_assets * Assets, asset_type_id TypeID, asset_vector * MatchVector, 
                     asset_vector * WeightVector)
{
    Assert(TypeID < Asset_BitmapCount);
    font_id Result = {GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector)};
    return Result;
}


internal bitmap_id 
GetBestMatchBitmapFrom(game_assets * Assets, asset_type_id TypeID, asset_vector * MatchVector, 
                       asset_vector * WeightVector)
{
    Assert(TypeID < Asset_BitmapCount);
    bitmap_id Result = {GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector)};
    return Result;
}

internal bitmap_id  
GetRandomBitmapFrom(game_assets * Assets,random_series * Series, asset_type_id TypeID)
{
    Assert(TypeID < Asset_BitmapCount);
    bitmap_id Result = {GetRandomAssetFrom(Assets, Series, TypeID)};
    return Result;
}


inline bitmap_id
GetFirstBitmapFrom(game_assets * Assets, asset_type_id TypeID)
{
    Assert(TypeID < Asset_BitmapCount);
    bitmap_id Result = {GetFirstAssetFrom(Assets, TypeID)};
    return Result;
}



internal sound_id 
GetBestMatchSoundFrom(game_assets * Assets, asset_type_id TypeID, asset_vector * MatchVector, 
                      asset_vector * WeightVector)
{
    Assert(TypeID < Asset_Count && TypeID > Asset_BitmapCount);
    sound_id Result = {GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector)};
    return Result;
}


internal sound_id  
GetRandomSoundFrom(game_assets * Assets,random_series * Series, asset_type_id TypeID)
{
    Assert(TypeID < Asset_Count && TypeID > Asset_BitmapCount);
    sound_id Result = {GetRandomAssetFrom(Assets, Series, TypeID)};
    return Result;
}


inline sound_id
GetFirstSoundFrom(game_assets * Assets, asset_type_id TypeID)
{
    Assert(TypeID < Asset_Count && TypeID > Asset_BitmapCount);
    sound_id Result = {GetFirstAssetFrom(Assets, TypeID)};
    return Result;
}



internal game_assets * 
AllocateGameAssets(memory_arena * TranArena, transient_state * TranState, memory_index MemorySize)
{
    TIMED_FUNCTION();
    
    game_assets * Assets = PushStruct(TranArena, game_assets);
    
    Assets->NextGenerationIndex = 0;
    Assets->InflightGenerationCount = 0;
    Assets->OperationLock = 0;
    
    Assets->MemorySentinel.Prev = &Assets->MemorySentinel;
    Assets->MemorySentinel.Next = &Assets->MemorySentinel;
    Assets->MemorySentinel.MemoryFlags = 0;
    Assets->MemorySentinel.Size = 0;
    
    InsertBlock(&Assets->MemorySentinel, MemorySize, PushSize(TranArena, MemorySize));
    
    Assets->TranState = TranState;
    
    Assets->HeaderSentinel.Prev = &Assets->HeaderSentinel;
    Assets->HeaderSentinel.Next = &Assets->HeaderSentinel;
    Assets->HeaderSentinel.AssetIndex = 0;
    
    for(u32 TagIndex = 0;
        TagIndex < Tag_Count;
        ++TagIndex)
    {
        Assets->TagRange[TagIndex] = 1000000; 
    }
    Assets->TagRange[Tag_FacingDirection] = Tau32;
    
    {
        //TODO: Look for different asset file names an merge 'em  
        platform_file_group FileGroup  = Platform.BeginGiveMeAllFilesOfExtension(PlatformFileType_AssetFile);
        Assets->FileCount = FileGroup.FileCount;
        Assets->AssetFiles = PushArray(TranArena, Assets->FileCount, asset_file);
        
        Assets->AssetCount = 1;
        Assets->TagCount = 1;
        
        for(u32 FileIndex = 0;
            FileIndex < Assets->FileCount;
            ++FileIndex)
        {
            asset_file * File = Assets->AssetFiles + FileIndex;
            
            File->FontBitmapIDOffset = 0;
            File->TagBase = Assets->TagCount;
            
            ZeroStruct(File->Header);
            File->Handle = Platform.OpenNextFile(&FileGroup);
            Platform.ReadDataFromFile(&File->Handle, 0, sizeof(File->Header), &File->Header);
            
            u64 AssetTypeArraySize = File->Header.AssetTypeCount * sizeof(hha_asset_type);
            
            File->AssetTypeArray = (hha_asset_type *)PushSize(TranArena, AssetTypeArraySize);
            Platform.ReadDataFromFile(&File->Handle, File->Header.AssetTypeOffset, 
                                      AssetTypeArraySize, File->AssetTypeArray);
            
            if(File->Header.MagicValue != HHA_MAGIC_VALUE)
            {
                Platform.FileError(&File->Handle, "HHA File has an invalid magic value");
            }
            if(File->Header.Version > HHA_VERSION)
            {
                Platform.FileError(&File->Handle, "HHA File is of a later version");
            }
            
            if(PlatformNoFileErrors(&File->Handle))
            {
                //NOTE: The first asset and tag slot in every HHA is a null(reserved) so we don't count it.
                Assets->TagCount += (File->Header.TagCount - 1);
                Assets->AssetCount += (File->Header.AssetCount - 1);
            }
            else
            {
                //TODO: Notify users of bogus files
                InvalidCodePath;
            }
            
        }
        
        Platform.EndGiveMeAllFilesOfExtension(&FileGroup);
    }
    
    Assets->Assets = PushArray(TranArena, Assets->AssetCount, asset);
    Assets->Tags = PushArray(TranArena, Assets->TagCount, hha_tag);
    
    //Zeroed first tag in TagArray 
    ZeroStruct(*(Assets->Tags));
    //NOTE: Tag loading
    for(u32 FileIndex = 0;
        FileIndex < Assets->FileCount;
        ++FileIndex)
    {
        asset_file * File = Assets->AssetFiles + FileIndex;
        
        if(PlatformNoFileErrors(&File->Handle))
        {
            Platform.ReadDataFromFile(&File->Handle, File->Header.TagOffset + sizeof(hha_tag), 
                                      (File->Header.TagCount - 1) * sizeof(hha_tag), 
                                      Assets->Tags + File->TagBase);
        }
    }
    
    u32 AssetCount = 0;
    ZeroStruct(*(Assets->Assets + AssetCount));
    ++AssetCount;
    
    b32 AssetFontBaseFound = false;
    u32 AssetFontBase = 0;
    //TODO: Exercise - How could you speed up this? 
    //NOTE: Asset loading 
    for(u32 DestTypeID = 0;
        DestTypeID < Asset_Count;
        ++DestTypeID)
    {
        asset_type * DestType  = Assets->AssetTypes + DestTypeID;
        DestType->FirstAssetIndex = AssetCount;
        
        for(u32 FileIndex = 0;
            FileIndex < Assets->FileCount;
            ++FileIndex)
        {
            asset_file * File = Assets->AssetFiles + FileIndex;
            
            if(PlatformNoFileErrors(&File->Handle))
            {
                for(u32 FileTypeID = 0;
                    FileTypeID < File->Header.AssetTypeCount;
                    ++FileTypeID)
                {
                    hha_asset_type * Source = File->AssetTypeArray + FileTypeID;
                    if(Source->TypeID == DestTypeID)
                    {
                        if(Source->TypeID  == Asset_FontGlyph)
                        {
                            File->FontBitmapIDOffset = AssetCount - Source->FirstAssetIndex;
                        }
                        
                        u64 AssetOffsetInType = Source->FirstAssetIndex * sizeof(hha_asset);
                        u32 AssetCountForType = Source->OnePastLastAssetIndex - Source->FirstAssetIndex;  
                        
                        temp_memory TempMemory = BeginTempMemory(TranArena);
                        u32 AssetArraySize = AssetCountForType * sizeof(hha_asset);
                        hha_asset * AssetArray = (hha_asset*)PushSize(TranArena, AssetArraySize);
                        
                        Platform.ReadDataFromFile(&File->Handle,
                                                  File->Header.AssetOffset + AssetOffsetInType, 
                                                  AssetCountForType * sizeof(hha_asset),
                                                  AssetArray);
                        
                        for(u32 AssetIndex = 0;
                            AssetIndex < AssetCountForType;
                            ++AssetIndex)
                        {
                            hha_asset * HHA = AssetArray + AssetIndex;
                            
                            Assert(AssetCount <= Assets->AssetCount);
                            asset * Asset = Assets->Assets + AssetCount++;
                            
                            Asset->HHA = *HHA;
                            Asset->FileIndex = FileIndex;
                            
                            Asset->HHA.FirstTagIndex += (File->TagBase - 1);
                            Asset->HHA.OnePastLastTagIndex += (File->TagBase - 1);
                            
                        }
                        
                        EndTempMemory(TempMemory);
                    }
                }
            }
        }
        
        DestType->OnePastLastAssetIndex = AssetCount;
    }
    
    Assert(AssetCount == Assets->AssetCount);
    
    return Assets;
}

internal u32 
BeginGenerationIndex(game_assets * Assets)
{
    TIMED_FUNCTION();
    BeginOperationLock(Assets);
    
    u32 Result = Assets->NextGenerationIndex++;
    Assert(Assets->InflightGenerationCount < ArrayCount(Assets->InflightGenerations));
    Assets->InflightGenerations[Assets->InflightGenerationCount++] = Result;
    
    EndOperationLock(Assets);
    
    return Result;
}


internal void
EndGenerationIndex(game_assets * Assets, u32 GenerationIndex)
{
    TIMED_FUNCTION();
    BeginOperationLock(Assets);
    
    for(u32 Index = 0; 
        Index < Assets->InflightGenerationCount;
        ++Index)
    {
        if(Assets->InflightGenerations[Index] == GenerationIndex)
        {
            Assert(Assets->InflightGenerationCount > 0);
            Assets->InflightGenerations[Index] = Assets->InflightGenerations[--Assets->InflightGenerationCount];
            break;
        }
    }
    
    EndOperationLock(Assets);
    
}


internal u16 
GetGlyphIndex(hha_font * HHA, loaded_font * LoadedFont, u32 DesiredCodePoint)
{
    Assert(LoadedFont);
    u16 Result = 0;
    if(DesiredCodePoint < HHA->OnePastLastCodePoint)
    {
        Result = LoadedFont->GlyphIndexFromCodePoint[DesiredCodePoint];
        Assert(Result < HHA->GlyphCount);
    }
    return Result;
}

internal r32  
GetKerningForTwoCodePoints(hha_font * HHAFont, loaded_font * LoadedFont, u32 DesiredPrevCodePoint, u32 DesiredCodePoint)
{
    u16 GlyphIndex = GetGlyphIndex(HHAFont, LoadedFont, DesiredCodePoint);
    u32 PrevGlyphIndex = GetGlyphIndex(HHAFont, LoadedFont, DesiredPrevCodePoint);
    
    r32 Kerning  = LoadedFont->KerningTable[PrevGlyphIndex * HHAFont->GlyphCount + GlyphIndex];
    return Kerning;
}

internal bitmap_id
GetBitmapForGlyph(hha_font * HHAFont,  loaded_font * LoadedFont, u32 DesiredCodePoint)
{
    //NOTE: Clamp CodePoint values to valid range
    u16 GlyphIndex = GetGlyphIndex(HHAFont, LoadedFont, DesiredCodePoint);
    
    bitmap_id BitmapID = LoadedFont->Glyphs[GlyphIndex].BitmapID;
    BitmapID.Value += LoadedFont->BitmapIDOffset;
    
    Assert(BitmapID.Value > 0);
    
    return BitmapID;
}

internal r32
GetLineAdvanceFor(hha_font * HHAFont)
{
    r32 Result = (HHAFont->AscenderHeight + HHAFont->DescenderHeight + HHAFont->ExternalLeading);
    return Result;
}




