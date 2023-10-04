#ifndef HANDMADE_RENDER_GROUP_H

/*
NOTE:

. 1) Everywhere outside the renderer, y_always_goes upward, x to the right.

. 2) All the bitmaps, including the renderer target are assumed to be bottom up.
. (Meaning that the first row pointer points to the bottom must row when viewed on screen.)

. 3) It's mandatory that all the renderer inputs are in world coordinates ("METERS") 
.   NOT pixels.  If for some reason something needs to be in pixels, it will be  marked as such
.   in the API, but this should occur exceedingly sparingly.

. 4) Z is a special coordinate because it is broken up into discrete slices, and the renderer
.   understands this slices. Z slices are what control the scaling, whereas z offsets inside a slice 
-   are what cotrol y-offseting. 

. 5) All color values passed to the renderer as Vector4's have to be  NON - premultiplied alpha  

.  NON Premultiplied         Premultiplied
.  (1,1,1,0.5)      ->     (0.5,0.5,0.5,0.5)

. TODO: ZHANDLING

*/


struct loaded_bitmap
{
    void * Memory;
    vector_2 AlignPorcentage;
    r32 WidthOverHeight;
    s32 Width;
    s32 Height;
    //NOTE: 16 byte + 8 byte memory 
    s32 Pitch;
};

struct environment_map
{
    //NOTE: LOD[0] =  {2^WidthPower2 , 2^HeightPower2} starting form the highest resolution bitmap
    //uint32 WidthPower2;
    //uint32 HeightPower2;
    loaded_bitmap LOD[4];
    real32 Pz;
};

//NOTE: render_group_entry is a "compact discriminated union"
enum render_group_entry_type 
{
    RenderGroupEntryType_render_entry_clear,
    RenderGroupEntryType_render_entry_rectangle,
    RenderGroupEntryType_render_entry_bitmap,
    RenderGroupEntryType_render_entry_coordinate_system,
};

//TODO: Remove the header!
struct render_group_entry_header
{
    render_group_entry_type Type;
};

struct render_entry_clear
{
    vector_4 Color;
};

struct render_entry_rectangle
{
    vector_2 Position;
    vector_2 Dimension;
    vector_4 Color;
};

struct render_entry_bitmap
{
    loaded_bitmap * Bitmap;
    
    vector_2 Position;
    vector_2 Size;
    vector_4 Color;
};

//NOTE: This is a test for now
struct render_entry_coordinate_system
{
    vector_2 Origin;
    vector_2 XAxis;
    vector_2 YAxis;
    vector_4 Color;
    
    loaded_bitmap * Texture;
    loaded_bitmap * Normalmap;
    
    environment_map * Top;
    environment_map * Middle;
    environment_map * Bottom;
};

struct render_transform
{
    bool32 Perspective; 
    //NOTE: This are camera parameters
    real32 CameraToTargetZ;
    real32 CameraToMonitorZ;
    
    vector_2 ScreenCenter;
    real32 MetersToPixels;//NOTE: This translates from meters in the monitor to pixels in the monitor
    
    vector_3 OffsetP;
    real32 Scale;
};

Introspection(category:Render)struct render_group
{
    real32 GlobalAlpha;
    
    struct game_assets * Assets;
    
    u32 GenerationIndex;
    
    vector_2 MonitorHalfDimInMeters;
    render_transform Transform;
    
    uint32 MaxPushBufferSize;
    uint32 PushBufferSize;
    uint8 * PushBufferBase;
    
    uint32 MissingResourceCount;
    
    b32 IsBackgroundThread;
    b32 InsideRenderer;
    
    r32 LineY;
};

//NOTE: Debug Display will always be orthognoal
struct debug_render_group
{
    uint32 MaxPushBufferSize;
    uint32 PushBufferSize;
    uint8 * PushBufferBase;
};


struct render_entity_basis_result
{
    vector_2 Position;
    real32 Scale;
    bool32 IsDrawable;
};


struct used_bitmap_size_info
{
    vector_2 Size;
    vector_2 Align;
    vector_3 Position;
    
    render_entity_basis_result Basis;
};


void
DrawRectangleQuickly(loaded_bitmap * Buffer, vector_2 Origin , vector_2 XAxis, vector_2 YAxis, 
                     vector_4 Color, loaded_bitmap * Texture, real32 PixelsToMeters, 
                     rectangle_2i ClipRect, bool32 Even);

inline void
CheckGlobalAlphaVisible(render_group * RenderGroup)
{
    Assert(RenderGroup->GlobalAlpha != 0);
}


#define HANDMADE_RENDER_GROUP_H
#endif


