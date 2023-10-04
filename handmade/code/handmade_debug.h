#ifndef HANDMADE_DEBUG_H
#define HANDMADE_DEBUG_H 

#define SIZE_OF_INDENT 4
#define MAX_FRAME_COUNT (MAX_DEBUG_EVENT_ARRAY_COUNT * 4)
#define MAX_STACK_SIZE 64
//TODO(Alex): We probably want to have an arena here? 
#define MAX_LINK_COUNT 1024
#define MAX_NAME_COUNT 4096 * 4

struct render_group;
struct loaded_bitmap;
struct game_assets;
struct loaded_font;

struct debug_tree;
struct debug_link;
struct debug_state;

enum debug_variable_to_text_flags
{
    DebugVariableToText_NullTerminator = (1 << 0),
    DebugVariableToText_AddDebugUI = (1 << 1),
    DebugVariableToText_AddName = (1 << 2),
    DebugVariableToText_PrettyBool = (1 << 3),
    DebugVariableToText_Semicolon = (1 << 4),
};

enum debug_view_type
{
    DebugViewType_Basic,
    DebugViewType_InlineBlock,
    DebugViewType_Collapsible,
};

struct debug_view_inline_block
{
    vector_2 Offset;
    vector_2 Dim;
};

struct debug_view_collapsible
{
    b32 ExpandedAlways;
    b32 ExpandedAltView;
};


struct debug_game_interactive
{
    debug_id ID;
    b32 Highlighted;
    b32 DataShouldBePrinted;
    r32 ZLayer;
};


struct debug_view
{
    debug_view_type Type;  
    debug_id ID;
    
    union
    {
        debug_view_inline_block InlineBlock;
        debug_view_collapsible Collapsible;
        debug_game_interactive GameInteractive;
    };
    
    debug_view * NextInHash;
};

struct debug_bitmap_display
{
    bitmap_id ID;
};

struct debug_link
{
    debug_link * Next;
    debug_link * Prev;
    debug_link * Sentinel;
    
    debug_event * Event;
};

struct debug_tree
{
    vector_2 UIP;
    debug_link * RootLink;
    
    debug_tree * Next;
    debug_tree * Prev;
};

struct debug_frame_region
{
    u64 CycleCount;
    r32 MinX;
    r32 MaxX;
    u16 LaneIndex;
    u16 ColorIndex;
};

struct debug_frame
{
    u64 BeginClock;
    u64 EndClock;
    
    r32 SecondsElapsed;
    
    u32 LinkCount;
    debug_link Links[MAX_LINK_COUNT];
    debug_link * RootGroup;
    
    u32 RegionCount;
    debug_frame_region Regions[MAX_DEBUG_REGION_COUNT];
};

struct debug_open_block
{
    u32 StartingFrameIndex;
    
    debug_event * OpeningEvent;
    debug_link * Link;
    
    debug_open_block * Parent;
    debug_open_block * NextFree;
};

struct debug_thread
{
    u32 ID;
    debug_open_block * FirstOpenCodeBlock;
    debug_open_block * FirstOpenDataBlock;
    
    debug_thread * Next;
    u16 LaneIndex;
};


enum debug_interaction_type
{
    DebugInteraction_None = 0,
    
    DebugInteraction_NOP = (1 << 0),
    DebugInteraction_Drag = (1 << 1),
    DebugInteraction_Toggle = (1 << 2),
    DebugInteraction_Tear = (1 << 3),
    DebugInteraction_Resize = (1 << 4),
    DebugInteraction_Move = (1 << 5),
    DebugInteraction_Select = (1 << 6),
    DebugInteraction_SetAutomatic = (1 << 7),
    
    DebugInteraction_Count,
};

//TODO(Alex): We probably want this to be represented separately without data itself
//The data that we interact with is different for each ocassion

enum debug_interaction_flags
{
    DebugInteractionFlag_Generic,
    DebugInteractionFlag_Event,
};

struct debug_interaction
{
    debug_id ID;
    debug_interaction_type Type;
    
    debug_interaction_flags Flags;
    
    union
    {
        void * Generic;
        debug_event * Event;
        debug_tree * Tree;
        vector_2 * P;
    };
};

struct debug_state
{
    b32 IsInitialized;
    b32 FreezeCollation;
    platform_work_queue * HighPriorityQueue;
    
    b32 Compiling;
    debug_executing_process Compiler;
    
    debug_tree TreeSentinel;
    debug_view * ViewHash[4096];
    
    debug_interaction HotInteraction;
    debug_interaction NextHotInteraction;
    debug_interaction Interaction;
    
    font_id FontID;
    r32 AtY;
    r32 LeftEdge;
    r32 FontScale;
    
    vector_2 LastMouseP;
    vector_2 RelMouseP;
    vector_2 BeginInteractMouseP;
    
    //NOTE(Alex): This is for loop live code editing to preserve layout positioning after gameplay \
restart  
    vector_2 MenuPos;
    
    render_group * RenderGroup;
    loaded_font * Font;
    hha_font * HHA;
    
    vector_2 ScreenDim;
    
    memory_arena DebugArena;
    memory_arena CollateArena;
    
    temp_memory DebugTempMemory;
    
    u32 FrameBarLaneCount;
    r32 FrameBarScale;
    
    u32 FrameIndex;
    debug_frame * Frames;
    
    debug_thread * FirstThread;
    debug_open_block * FirstFreeBlock;
    
    debug_frame * CollationFrame;
    game_assets * Assets;
    
    u32 SelectedIDCount;
    debug_id SelectedIDs[64];
};

inline debug_interaction 
NullInteraction(void)
{
    debug_interaction Result = {};
    Result.ID = {};
    Result.Type = DebugInteraction_None;
    Result.Generic = 0;
    return Result;
}

inline debug_id GetDebugID(void * ptr1, void * ptr2);
//internal debug_view *  DEBUGGetViewFor(debug_state * DebugState, debug_id ID);
#endif

