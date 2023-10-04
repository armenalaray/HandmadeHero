#ifndef HANDMADE_H
#define HANDMADE_H

/*TODO: 

- Flush out all threads before reloading code dynamically. 

 Debug Code
  -Fonts
 -Logging
  -Diagramming
  -(A LITTLE GUI,  but only a Little) - Switches, sliders, etc.
  -Draw tile chunks so we can verify that things are aligned  in the chunks we want hem to be in 
  -Thread Visualization
  
  Audio
  -Fix clicking bug in audio!
  
 -Rendering
   -   Real projections with solid concept of project / unproject
-   Straighten up coordinate systems
    -   Screen
    -   World
     -   Textures
     -   Particle Systems
     -   Lighting
    -   Final Optimization
    
  ARCHITECTURE EXPLORATION
 Z!
 - Need to make a solide concept of ground levels so the camera can be freely placed in Z 
 and have multiple ground levels in one sim region
 -Concept of ground in the collision loop, so that we can handle coming onto and off of stairwells, 
 per example.
 
 -Debug drawing of Z leves and inclusion of Z so there are no bugs
 -Need to define how tall entities will be!
 -Make sure flying entities can go over low walls
 
 - how is this rendered?
 
 Collision Detection
    - Fix Sword Collisions
 - Clean up predicate proliferation  can we make a nice clean set of rules/flags
    so that's eaasy t understand how things work in terms of special handling?
    This may involve makig the iteration handle everything instead of handling overla outside.
    
 Transient collision rules! Clear based on flag 
 Allow non transient rules to override transient ones
 
  Entry / Exit?
  - What's the plan for robustness / shape definition?	
  - Implement reprojection to handle interpenetration
  - Things pushing other things
  
 Animation
  - Skeletal animation
  - Particle systems
  
 Implement Multiple SimRegions per frame
  - Per-entity Clocking 
  - Sim Region merging? For multiple players?
  - Add Zoom-out for testing
  
  
 PRODUCTION
 
 -GAME
 - Entity System
  - Rudimentary world gen (no quality, just to show what sorts of things we do)
  -  Placement of background things.
   -  Connectivity?
  -  Non-Overlapping?
  -  Map Display 
   -  Magnets - how do they work?
   
   
 AI
  - Rudimentary monstar behaviour example
   - Pathfinding
   - AI -Storage?
   
 Metagame / Save Game?
  - Can you have multiple saved games?
  - Persistent unlocks, etc.
  - Do we allow saved games? Probably yes, only for pausing.
  * Continuous save for crash recovery?
  
  
*/


#include "handmade_platform.h"
#include "handmade_intrinsics.h"
#include "handmade_math.h"
#include "handmade_file_formats.h"
#include "handmade_meta.h"


#define Minimum(A,B) ((A < B) ? A : B)
#define Maximum(A,B) ((A > B) ? A : B)

inline b32 
StringsAreEqual(char * A, char * B)
{
    b32 Result = (A == B);
    
    if(A && B)
    {
        while((*A && *B) && (*A == *B))
        {
            ++A; ++B;
        }
        
        Result = (*A == 0) && (*B == 0); 
    }
    
    return Result;
}

struct memory_arena
{
    memory_index Size;
    uint8 * Base;
    memory_index Used;
    int32 TempCount;
};

struct temp_memory
{
    memory_index Used;
    memory_arena * Arena;
};

inline void 
InitializeArena(memory_arena * Arena , memory_index Size, void * Base) 
{
    Arena->Size = Size;
    Arena->Base = (uint8 * )Base;
    Arena->Used = 0;
}

inline memory_index 
GetAlignOffset(memory_arena * Arena, memory_index Alignment)
{
    memory_index AlignOffset = 0;
    memory_index ResultPointer = (memory_index)Arena->Base + Arena->Used;
    
    memory_index AlignMask = Alignment - 1;
    
    if(ResultPointer & AlignMask)
    {
        AlignOffset = Alignment - (ResultPointer & AlignMask); 
    }
    
    return AlignOffset;
}

inline memory_index
GetRemainingArenaSize(memory_arena * Arena, memory_index Alignment = 4)
{
    memory_index Result = Arena->Size - (Arena->Used + GetAlignOffset(Arena, Alignment));
    return Result;
}

//TODO: Optional clear parameters
#define PushStruct(Arena, type, ...) (type *)PushSize_(Arena, sizeof(type), __VA_ARGS__)
#define PushArray(Arena, count, type, ...) (type *)PushSize_(Arena, (count) * sizeof(type), __VA_ARGS__)
#define PushSize(Arena, Size, ...) (void *)PushSize_(Arena, Size, __VA_ARGS__)

inline void *
PushSize_(memory_arena * Arena, memory_index Size, memory_index Alignment = 4)
{
    //NOTE: IMPORTANT Clear to zero!
    memory_index AlignOffset = GetAlignOffset(Arena, Alignment);
    
    Size += AlignOffset;
    Assert((Arena->Used + Size)  <= Arena->Size);
    void * Result = (Arena->Base + Arena->Used + AlignOffset);
    Arena->Used += Size;
    
    return Result;
}

inline char *
PushString(memory_arena * Arena, char * String)
{
    char * Result = 0;
    if(String)
    {
        u32 Size = 1;
        for(char * At = String;
            *At;
            ++At)
        {
            ++Size;
        }
        
        
        Result = (char *)PushSize_(Arena, Size);
        
        char * Dest = Result;
        char * Source = String; 
        for(u32 LetterIndex = 0;
            LetterIndex < Size;
            ++LetterIndex)
        {
            *Dest++ = *Source++;
        }
        
    }
    return Result;
}


#define ZeroStruct(Instance) ZeroSize(sizeof(Instance), &(Instance))
#define ZeroArray(Count, Ptr) ZeroSize(Count * sizeof(Ptr[0]), Ptr)

inline void 
ZeroSize(memory_index Size, void * Ptr)
{
    uint8 * Byte =  (uint8 *)Ptr;
    while(Size--)
    {
        *Byte++ = 0;
    }
}


inline void 
CheckArena(memory_arena * Arena)
{
    Assert(Arena->TempCount == 0);
}

inline void 
SubArena(memory_arena * SubArena, memory_arena * Arena, memory_index Size, uint32 Alignment = 16)
{
    SubArena->Size = Size;
    SubArena->Base = (uint8 * )PushSize_(Arena, Size, Alignment);
    SubArena->Used = 0;
    SubArena->TempCount = 0;
}


struct task_with_memory
{
    bool32 BeingUsed;
    memory_arena Arena;
    temp_memory MemoryFlush;
};


#include "handmade_random.h"
#include "handmade_world.h"
#include "handmade_simulation_region.h"
#include "handmade_entity.h"
#include "handmade_render_group.h"
#include "handmade_asset.h"
#include "handmade_audio.h"


inline temp_memory 
BeginTempMemory(memory_arena * Arena)
{
    temp_memory Result = {};
    Result.Arena = Arena;
    Result.Used = Arena->Used;
    
    ++Arena->TempCount;
    return Result;
}

//TODO: Shall we put a lock here?
inline void
EndTempMemory(temp_memory TempMemory)
{
    memory_arena * Arena = TempMemory.Arena;
    Assert(Arena->Used >= TempMemory.Used);
    
    CompletePreviousWritesBeforeFututeWrites;
    Arena->Used = TempMemory.Used;
    //NOTE:This is for balancing the locks we put when BeginTempMemory
    Assert(Arena->TempCount > 0);
    --Arena->TempCount;
}


struct low_entity
{
    //TODO: It's kind of busted that positions can be invalid here.
    // and we store whether they would be invalid in the flags field also.
    sim_entity SimEntity;
    world_position Position;
    //TODO: Generation index so we know how up to date this entity is
};	


struct low_entity_chunk_referece
{
    world_chunk * TileChunk;
    uint32 EntityIndexInChunk;
};


struct controlled_hero
{
    uint32 EntityIndex;
    //NOTE: these are the controller requests for simulation
    vector_2 PlayerAcceleration;
    vector_2 SwordVelocity;
    real32 ZVelocity;
};


struct pairwise_collision_rule
{
    bool32 CanCollide;
    uint32 StorageIndexA;
    uint32 StorageIndexB;
    
    pairwise_collision_rule * NextInHash;
};


struct ground_buffer 
{
    world_position Position;//NOTE: If Nullposition , means Memory hasn't been filled yet
    loaded_bitmap Bitmap;
};


struct hero_bitmap_ids
{
    bitmap_id Head;
    bitmap_id Cape;
    bitmap_id Torso;
};

struct particle_cell
{
    r32 Density;
    vector_3 VelocityTimesDensity;
};

struct particle
{
    vector_3 Position;
    vector_3 Velocity;
    vector_3 Acceleration;
    
    vector_4 dColor;
    vector_4 Color;
    
    bitmap_id BitmapID;
};

struct game_state
{
    bool32 IsInitialized;
    
    //TODO: Shall we use other Arena for permanent storage, sounds, etc. 
    memory_arena MetaArena;
    memory_arena WorldArena;
    
    world * World;
    
    //TODO: Should we allow for splitscreen ?
    uint32 CameraFollowingEntityIndex;
    world_position CameraPosition;
    
    real32 TypicalFloorHeight;
    
    controlled_hero ControlledHeroes[ArrayCount(((game_input *)0)->Controllers)];
    
    //TODO: change the name to stored entities
    uint32 LowEntityCount;
    low_entity LowEntities[100000];
    
    
    //NOTE: This has to be a power of two
    pairwise_collision_rule * CollisionRuleHash[256];
    pairwise_collision_rule * FirstFreeCollisionRule;
    
    collision_volume_group * NullCollision;
    collision_volume_group * StairCollision;
    collision_volume_group * SwordCollision;
    collision_volume_group * PlayerCollision;
    collision_volume_group * MonsterCollision;
    collision_volume_group * WallCollision;
    collision_volume_group * FamiliarCollision;
    collision_volume_group * StandardRoomCollision;
    
    real32 Time;
    
    //TODO(Alex): Refill this guy with gray
    loaded_bitmap TestDiffuse;
    loaded_bitmap TestNormal;
    
    audio_state AudioState;
    playing_sound * MusicSound;
    
    random_series GeneralEntropy;
    //NOTE(Alex): This is a for non gameplay randomness
    random_series EffectsEntropy;
    
    u32 NextParticle;
    particle Particles[256];
    
#define PARTICLE_CELL_COUNT 16
    particle_cell ParticleCells[PARTICLE_CELL_COUNT][PARTICLE_CELL_COUNT];
    
};



struct transient_state
{
    bool32  IsInitialized;
    memory_arena TranArena;
    
    uint32 GroundBufferCount;
    ground_buffer * GroundBuffer;
    
    game_assets * Assets;
    
    platform_work_queue * HighPriorityQueue;
    platform_work_queue * LowPriorityQueue;
    //uint64 * Pad;
    
    uint32 EnvMapWidth;
    uint32 EnvMapHeight;
    //NOTE: EnvMaps[0] - Bottom ; EnvMaps[1] = Middle ; EnvMaps[2] = Top
    environment_map EnvMaps[3];
    
    loaded_font * DefaultFont;
    hha_font * HHADefualtFont;
    
    task_with_memory Tasks[4];
    
};


internal low_entity * 
GetLowEntity(game_state * GameState, uint32 LowIndex)
{
    low_entity * Result = {};
    if((LowIndex > 0) && (LowIndex < GameState->LowEntityCount))
    {
        Result = GameState->LowEntities + LowIndex;
    }
    
    return Result;
}


internal void 
ClearCollisionRulesFor(game_state * GameState, uint32 StorageIndex);

internal void 	 
AddCollisionRule(game_state * GameState, uint32 StorageIndexA,uint32 StorageIndexB, bool32 CanCollide);
global_variable platform_api Platform;

#endif









