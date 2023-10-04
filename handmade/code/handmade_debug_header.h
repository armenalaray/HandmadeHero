#ifndef HANDMADE_DEBUG_HEADER_H


#ifdef __cplusplus
extern "C" {
#endif
    
#define MAX_NUM_THREADS 64
    struct debug_table;
#define GAME_DEBUG_FRAME_END(name) debug_table * name (game_memory * Memory, game_input * Input, game_offscreen_buffer * Buffer)
    typedef GAME_DEBUG_FRAME_END(game_debug_frame_end);
    
    typedef struct thread_table
    {
        u32 Count;
        u32 IDs[MAX_NUM_THREADS];
    }thread_table;
    
#if HANDMADE_INTERNAL
    
    
    struct debug_id
    {
        void * Value[2]; 
    };
    
    inline debug_id
        NullID(void)
    {
        debug_id ID = {};
        return ID;
    }
    
    enum debug_type
    {
        DebugType_FrameMarker,
        DebugType_Begin,
        DebugType_End,
        
        DebugType_OpenDataBlock,
        DebugType_CloseDataBlock,
        
        DebugType_b32,
        DebugType_u32,
        DebugType_s32,
        DebugType_r32,
        DebugType_vector_2,
        DebugType_vector_3,
        DebugType_vector_4,
        DebugType_rectangle_2,
        DebugType_rectangle_3,
        DebugType_bitmap_id,
        DebugType_sound_id,
        DebugType_font_id,
        DebugType_debug_id,
        
        DebugType_ToggleGameData,
        DebugType_CounterThreadList,
        DebugType_Hierarchy,
    };
    
    enum debug_access_flags
    {
        DebugAccessFlag_None = 0,
        DebugAccessFlag_Read = (1 << 0),
        DebugAccessFlag_Write = (1 << 1), //NOTE(Alex): Permanent storage write
    };
    
    typedef struct debug_event
    {
        u64 Clock;
        //TODO(Alex): We could collapse Name and FileName adding them to a buffer one next to other.
        char * Name;
        char * FileName;
        u32 LineNumber;
        u16 ThreadID;
        u16 CoreIndex;
        
        u8 AccessFlags;
        u8 Type;
        
        union
        {
            debug_id ID;
            b32 B32;
            u32 U32;
            s32 S32;
            r32 R32;
            vector_2 V2;
            vector_3 V3;
            vector_4 V4;
            rectangle_2 Rect2;
            rectangle_3 Rect3;
            bitmap_id BitmapID;
            sound_id SoundID;
            font_id FontID;
        };
        
    }debug_event;
    
#define MAX_DEBUG_THREAD_COUNT 256
#define MAX_DEBUG_EVENT_ARRAY_COUNT 8
#define MAX_DEBUG_EVENT_COUNT (32* 65536)
#define MAX_DEBUG_REGION_COUNT 65536
    
    
    typedef struct debug_table
    {
        
#if 1        
        //TODO(Alex): these are gonna be debug_events since the beginning
        b32 DEBUGUI_SampleEnvironmentMap;
        b32 DEBUGUI_EnableDebugCamera;
        b32 DEBUGUI_UseRoomBasedCamera; 
        r32 DEBUGUI_DebugCameraDistance;
        // GroundChunks 
        b32 DEBUGUI_GroundChunkOutlines; 
        b32 DEBUGUI_GroundChunkCheckerBoard; 
        b32 DEBUGUI_GroundChunkReloading; 
        // Particles 
        b32 DEBUGUI_ParticlesTest; 
        b32 DEBUGUI_ParticlesGrid; 
        // EntitySystem 
        b32 DEBUGUI_UsedSpaceOutlines; 
        b32 DEBUGUI_EntityCollisionVolumes; 
        b32 DEBUGUI_FamiliarFollowsHero; 
#endif
        
        r32 ZLevel;
        u64 CurrentEventArrayIndex; 
        u64 volatile EventArrayIndex_EventIndex; 
        u32 EventCount[MAX_DEBUG_EVENT_ARRAY_COUNT];
        debug_event Events[MAX_DEBUG_EVENT_ARRAY_COUNT][MAX_DEBUG_EVENT_COUNT];
    }debug_table;
    
    //TODO(Alex): Check for dependencies
    extern debug_table * GlobalDebugTable;
    
    //NOTE: Really improbable bug, where a thread got its index back and in that moment get preempted, and 
    //held preempted throughout the whole writing of the logs. Till the swap has happened another time. the
    
    //TODO(Alex): I would like to switch away from the translation unit indexing and just go with a simple
    //one-time hash-function set because is causing problems now! 
    
    //Assert(((u64)&GlobalDebugTable->EventArrayIndex_EventIndex & 7) == 0);
#define RecordDebugEvent()\
    u64 DebugEventArrayIndex_DebugEventIndex = AtomicAddU64(&GlobalDebugTable->EventArrayIndex_EventIndex, 1);\
    u32 EventIndex = (u32)(DebugEventArrayIndex_DebugEventIndex & 0xFFFFFFFF);\
    Assert(EventIndex < MAX_DEBUG_EVENT_COUNT);\
    unsigned int CoreIndex;\
    unsigned __int64 Timestamp = __rdtscp(&CoreIndex);\
    debug_event * Event = GlobalDebugTable->Events[DebugEventArrayIndex_DebugEventIndex >> 32] + EventIndex;\
    Event->Clock = Timestamp;\
    Event->ThreadID = (u16)GetThreadID();\
    Event->CoreIndex = (u16)CoreIndex;\
    Event->FileName = (char*)__FILE__;\
    Event->LineNumber = __LINE__;\
    Event->ID = NullID();
    
#define FRAME_MARKER(SecondsElapsedInit)\
    {\
        RecordDebugEvent();\
        Event->R32 = (SecondsElapsedInit);\
        Event->Name = "Marked Frame";\
        Event->Type = DebugType_FrameMarker;\
    }
    
#define TIMED_BLOCK__(Name, LineNumber, ...) timed_block TimedBlock_##LineNumber(Name, ## __VA_ARGS__)
#define TIMED_BLOCK_(Name, LineNumber, ...) TIMED_BLOCK__(Name, LineNumber, ## __VA_ARGS__)
#define TIMED_BLOCK(Name, ...) TIMED_BLOCK_(#Name, __LINE__, ##  __VA_ARGS__)
    
#define TIMED_FUNCTION(...) TIMED_BLOCK_((char *)__FUNCTION__, ## __VA_ARGS__)
    
#define BEGIN_BLOCK_(NameInit, ...)\
    {\
        CompletePreviousWritesBeforeFututeWrites;\
        RecordDebugEvent();\
        Event->Name = (NameInit);\
        Event->Type = DebugType_Begin;\
    }
    
#define BEGIN_BLOCK(Name, ...)\
    BEGIN_BLOCK_(#Name, __VA_ARGS__);
    
    
#define END_BLOCK_()\
    {\
        RecordDebugEvent();\
        Event->Type = DebugType_End;\
    }
    
#define END_BLOCK(Name) END_BLOCK_()
    
    typedef struct timed_block
    {
        //NOTE: This is thread safe, since they are different instances
        timed_block(char* Name, u32 HitCountParam = 1)
        {
            //TODO: Save hitcount and cyclecount 
            BEGIN_BLOCK_(Name);
        }
        
        ~timed_block()
        {
            END_BLOCK_();
        }
    }timed_block;
    
#else
    
    //#define RecordDebugEvent(Counter, EventType)
#define FRAME_MARKER(...)
#define TIMED_BLOCK(...)
#define TIMED_FUNCTION(...)
#define BEGIN_BLOCK(...)
#define END_BLOCK(...)
    
#endif
    
#ifdef __cplusplus
}
#endif

//--------------------------------------------------------------------------

#if defined(__cplusplus) && HANDMADE_INTERNAL

inline void
DebugAddEventVariable(debug_event * Event, u32 Value)
{
    Event->Type = DebugType_u32;
    Event->U32 = Value;
}

inline void
DebugAddEventVariable(debug_event * Event, s32 Value)
{
    Event->Type = DebugType_s32;
    Event->S32 = Value;
}

inline void
DebugAddEventVariable(debug_event * Event, r32 Value)
{
    Event->Type = DebugType_r32;
    Event->R32 = Value;
}

inline void
DebugAddEventVariable(debug_event * Event, vector_2 Value)
{
    Event->Type = DebugType_vector_2;
    Event->V2 = Value;
}

inline void
DebugAddEventVariable(debug_event * Event, vector_3 Value)
{
    Event->Type = DebugType_vector_3;
    Event->V3 = Value;
}

inline void
DebugAddEventVariable(debug_event * Event, vector_4 Value)
{
    Event->Type = DebugType_vector_4;
    Event->V4 = Value;
}


inline void
DebugAddEventVariable(debug_event * Event, rectangle_2 Value)
{
    Event->Type = DebugType_rectangle_2;
    Event->Rect2 = Value;
}

inline void
DebugAddEventVariable(debug_event * Event, rectangle_3 Value)
{
    Event->Type = DebugType_rectangle_3;
    Event->Rect3 = Value;
}

inline void
DebugAddEventVariable(debug_event * Event, bitmap_id Value)
{
    Event->Type = DebugType_bitmap_id;
    Event->BitmapID = Value;
}

inline void
DebugAddEventVariable(debug_event * Event, sound_id Value)
{
    Event->Type = DebugType_sound_id;
    Event->SoundID = Value;
}

inline void
DebugAddEventVariable(debug_event * Event, font_id Value)
{
    Event->Type = DebugType_font_id;
    Event->FontID = Value;
}


inline void
DebugAddEventVariable(debug_event * Event, debug_id Value)
{
    Event->Type = DebugType_debug_id;
    Event->ID = Value;
}

#define DEBUG_SWITCH(Name) GlobalDebugTable->DEBUGUI_##Name 

#define DEBUG_OPEN_DATA_BLOCK_(NameInit) DEBUG_OPEN_DATA_BLOCK(NameInit, NullID())
#define DEBUG_OPEN_DATA_BLOCK(NameInit, IDInit)\
{\
    RecordDebugEvent();\
    Event->ID = (IDInit);\
    Event->Type = DebugType_OpenDataBlock;\
    Event->Name = #NameInit;\
}


#define DebugAddVariable(Value)\
{\
    RecordDebugEvent();\
    Event->Name = #Value;\
    DebugAddEventVariable(Event, (Value));\
}

#define DebugAddVariableSwitch(Value)\
{\
    debug_id ID = {&(DEBUG_SWITCH(Value)), 0};\
    RecordDebugEvent();\
    Event->Name = #Value;\
    DebugAddEventVariable(Event, ID);\
}

#define DEBUG_CLOSE_DATA_BLOCK()\
{\
    RecordDebugEvent();\
    Event->Type = DebugType_CloseDataBlock;\
}


inline debug_id 
GET_DEBUG_ID(void * Ptr0, void * Ptr1)
{
    debug_id Result = {};
    Result.Value[0] = (Ptr0);
    Result.Value[1] = (Ptr1);
    return Result;
}

#define SET_ACTIVE_ZLEVEL(...)\
{\
    for(uint32 EntityIndex = 0; \
    EntityIndex < SimRegion->EntityCount; \
    ++EntityIndex)\
    {\
        sim_entity * Entity = SimRegion->Entities + EntityIndex;\
        if((Entity->Updatable) && (Entity->Type != EntityType_Room))\
        {\
            RenderGroup->Transform.OffsetP = GetEntityGroundPoint(Entity);\
            debug_id DebugEntityID = GET_DEBUG_ID((GameState->LowEntities + Entity->StorageIndex), 0);\
            for(u32 VolumeIndex = 0; VolumeIndex < Entity->Collision->VolumeCount; ++VolumeIndex)\
            {\
                collision_volume * Volume = Entity->Collision->Volumes + VolumeIndex;\
                vector_3 WorldMouseP = Unproject(&RenderGroup->Transform, Vector2(Input->MouseX, Input->MouseY), RenderGroup->Transform.OffsetP.z);\
                rectangle_3 EntityRectangle = RectCenterDim(Entity->Position + Volume->OffsetP, Volume->Dimension);\
                if(IsInRectangle(EntityRectangle, WorldMouseP))\
                {\
                    DEBUG_TEST_HIT(DebugEntityID, EntityRectangle.Max.z);\
                }\
            }\
        }\
    }\
    RenderGroup->Transform.OffsetP = Vector3(0,0,0);\
}

internal b32 DEBUG_HIGHLIGHTED(debug_id ID, vector_4 * Color);
internal void DEBUG_TEST_HIT(debug_id ID, r32 MouseZ);
internal b32 DEBUG_PRINT_DATA(debug_id ID);

#else

inline debug_id 
CREATE_DEBUG_ID (void * Ptr0, void * Ptr1)
{
    debug_id NullID = {};
    return Result;
}

#define SET_ACTIVE_ZLEVEL(...)
#define DEBUG_TEST_HIT(...)
#define DEBUG_HIGHLIGHTED(...) 0
#define DEBUG_OPEN_OPTIONAL_DATA_BLOCK(...) 0 

#define NUllID(...)
#define DEBUG_OPEN_DATA_BLOCK(...)
#define DEBUG_ADD_VARIABLE(...)
#define DEBUG_BEGIN_ARRAY(...)
#define DEBUG_END_ARRAY(...)
#define DEBUG_END_ELEMENT(...)
#endif

#define HANDMADE_DEBUG_HEADER_H
#endif












