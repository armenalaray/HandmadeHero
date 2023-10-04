#include "handmade_world.h"

//TODO: Think about what the real safe margin is 
//NOTE +- 67108864 is the max tile chunk in any direction
#define TILE_CHUNK_SAFE_MARGIN (67108864 / 64)
#define TILE_CHUNK_UNITIALIZED INT32_MAX
#define TILESPERCHUNK 8

inline world_position 
NullPosition()
{
    world_position Result = {};
    
    Result.ChunkX = TILE_CHUNK_UNITIALIZED;
    
    return Result;
}

inline bool32 
IsValidPosition(world_position * Position)
{
    if(Position)
    {
        bool32 Result = (Position->ChunkX != TILE_CHUNK_UNITIALIZED);
        return Result;
    }
    return false;
}


internal bool32
IsCannonical(real32 TileRelative, real32 ChunkDimInMeters)
{
    real32 Epsilon = 0.01f;
    bool32 Result = ((TileRelative >= -(0.5f * ChunkDimInMeters + Epsilon)) 
                     && (TileRelative <= (0.5f * ChunkDimInMeters + Epsilon)));
    return Result;
}

internal bool32
IsCannonical(world * World, vector_3 Offset)
{
    bool32 Result = ((IsCannonical(Offset.x, World->ChunkDimInMeters.x)) && (IsCannonical(Offset.y, World->ChunkDimInMeters.y)) && (IsCannonical(Offset.z, World->ChunkDimInMeters.z)));
    return Result;
}

internal bool32
AreInSameChunk(world * World, world_position * A, world_position * B)
{
    Assert(IsCannonical(World, A->Offset_));
    Assert(IsCannonical(World, B->Offset_));
    
    bool32 Result = ((A->ChunkX == B->ChunkX) && 
                     (A->ChunkY == B->ChunkY) && 
                     (A->ChunkZ == B->ChunkZ));
    return Result;
}

inline world_chunk *
GetWorldChunk(world * World, int32 WorldChunkX,  int32 WorldChunkY, int32 WorldChunkZ, memory_arena * Arena = 0)
{
    TIMED_FUNCTION();
    
    Assert(WorldChunkX > -TILE_CHUNK_SAFE_MARGIN);
    Assert(WorldChunkY > -TILE_CHUNK_SAFE_MARGIN);
    Assert(WorldChunkZ > -TILE_CHUNK_SAFE_MARGIN);
    Assert(WorldChunkX < TILE_CHUNK_SAFE_MARGIN);
    Assert(WorldChunkY < TILE_CHUNK_SAFE_MARGIN);
    Assert(WorldChunkZ < TILE_CHUNK_SAFE_MARGIN);
    
    //TODO: Better hash function!!!
    //TODO Fix when zero tilechunkx, y, z values to be set
    uint32 HashValue = 19*WorldChunkX + 7*WorldChunkY + 3*WorldChunkZ;
    uint32 HashSlot = HashValue & (ArrayCount(World->ChunkHash) - 1);
    
    Assert(HashSlot >= 0 && HashSlot < ArrayCount(World->ChunkHash));
    
    world_chunk * WorldChunk = World->ChunkHash + HashSlot;
    
    do
    {
        if(Arena && !WorldChunk->Set)
        {
            //Set new WorldChunk
            
            WorldChunk->X = WorldChunkX;
            WorldChunk->Y = WorldChunkY;
            WorldChunk->Z = WorldChunkZ;
            
            WorldChunk->NextInHash = 0; 
            WorldChunk->Set = true;
            break;
        }	
        
        if((WorldChunkX == WorldChunk->X) && 
           (WorldChunkY == WorldChunk->Y) && 
           (WorldChunkZ == WorldChunk->Z))
        {
            break;
        }
        
        if(Arena && WorldChunk->Set && (!WorldChunk->NextInHash))
        {
            WorldChunk->NextInHash = PushStruct(Arena, world_chunk);
            
            //WorldChunk = WorldChunk->NextInHash;
            
            //WorldChunk->X = 0;
            //WorldChunk->Set = false;
            //break;
        }
        
        WorldChunk = WorldChunk->NextInHash;
        
    }
    while(WorldChunk);
    
    return WorldChunk;
}



internal void InitializeWorld(world * World, vector_3 ChunkDimInMeters)
{
    TIMED_FUNCTION();
    
    World->ChunkDimInMeters = ChunkDimInMeters;
    
    World->FirstFree = 0;
    for(uint32 WorldChunkIndex = 0; WorldChunkIndex < ArrayCount(World->ChunkHash); ++WorldChunkIndex)
    {
        //World->ChunkHash[WorldChunkIndex].x = 0;
        World->ChunkHash[WorldChunkIndex].Set = false;
        World->ChunkHash[WorldChunkIndex].FirstBlock.EntityCount = 0;
    }
    
}


inline void 
RecanonicalizeCoord(real32 ChunkDimInMeters, real32 * TileRelative, int32 * Tile)
{
    TIMED_FUNCTION();
    //TODO: Need to do something that doesnt use the divide/multiply method for recanonicalizing because this can end up rounding back on to tile you just came from.
    
    //NOTE: Wrapping is not allowed, so all coordinates are assumed to be within the safe margin
    //Assert that we are no where near the edges of the world
    
    int32 Offset = RoundReal32ToInt32(*TileRelative / ChunkDimInMeters);
    *Tile += Offset;
    *TileRelative -= Offset * ChunkDimInMeters;
    
    Assert(IsCannonical(*TileRelative, ChunkDimInMeters));
}

inline world_position 
MapIntoChunkSpace(world * World, world_position BasePos, vector_3 Offset)
{
    TIMED_FUNCTION();
    world_position Result = BasePos;
    
    Result.Offset_ += Offset; 
    
    RecanonicalizeCoord(World->ChunkDimInMeters.x, &Result.Offset_.x, &Result.ChunkX);
    RecanonicalizeCoord(World->ChunkDimInMeters.y, &Result.Offset_.y, &Result.ChunkY);
    RecanonicalizeCoord(World->ChunkDimInMeters.z, &Result.Offset_.z, &Result.ChunkZ);
    
    return Result;
} 


inline vector_3
Subtract(world * World, world_position * A, world_position * B)
{
    TIMED_FUNCTION();
    
    vector_3 dTile = {((real32)A->ChunkX - (real32)B->ChunkX), 
        ((real32)A->ChunkY - (real32)B->ChunkY), 
        ((real32)A->ChunkZ - (real32)B->ChunkZ)};
    
    vector_3 Result = Hadamard(dTile , World->ChunkDimInMeters) + (A->Offset_ - B->Offset_);
    return Result;
}

internal world_position 
CenteredChunkPosition(uint32 ChunkX, uint32 ChunkY, uint32 ChunkZ)
{
    world_position Result = {};
    
    Result.ChunkX = ChunkX;
    Result.ChunkY = ChunkY;
    Result.ChunkZ = ChunkZ;
    
    return Result;
}


internal world_position 
CenteredChunkPosition(world_chunk * Chunk)
{
    world_position Result = CenteredChunkPosition(Chunk->X, Chunk->Y, Chunk->Z);
    return Result;
}

internal void
ChangeEntityLocationRaw(memory_arena * Arena, world * World , uint32 LowEntityIndex,
                        world_position * OldPosition, world_position * NewPosition)
{
    TIMED_FUNCTION();
    //TODO: if this function moves and entity into the camera bounds, 
    //should it go into the high frequency immediatly
    //TODO: if this function moves an entity outside the camera bounds, 
    //shall we remove it from the high enitities immediatly?
    
    Assert(!OldPosition || IsValidPosition(OldPosition));
    Assert(!NewPosition || IsValidPosition(NewPosition));
    
    
    if(OldPosition && NewPosition && AreInSameChunk(World, OldPosition, NewPosition))
    {
        //NOTE: leave entity where it is 
    }
    else 
    {
        if(OldPosition)
        {
            //NOTE: Change entity to new chunk
            
            world_chunk * WorldChunk = GetWorldChunk(World, OldPosition->ChunkX, OldPosition->ChunkY,OldPosition->ChunkZ);
            Assert(WorldChunk);
            
            if(WorldChunk)
            {
                bool32 NotFound = true;
                world_entity_block * FirstBlock = &WorldChunk->FirstBlock;
                for(world_entity_block * Block = FirstBlock;
                    (Block && NotFound); 
                    Block = Block->NextBlock)
                {
                    for(uint32 Index = 0; 
                        ((Index < Block->EntityCount) && NotFound); 
                        ++Index)
                    {
                        if(Block->LowEntityIndex[Index] == LowEntityIndex)
                        {
                            Assert(FirstBlock->EntityCount > 0);
                            Block->LowEntityIndex[Index] = FirstBlock->LowEntityIndex[--FirstBlock->EntityCount];
                            if(FirstBlock->EntityCount == 0)
                            {
                                if(FirstBlock->NextBlock)
                                {
                                    world_entity_block * NextBlock = FirstBlock->NextBlock;
                                    *FirstBlock = *NextBlock;
                                    
                                    NextBlock->NextBlock = World->FirstFree;
                                    World->FirstFree = NextBlock;
                                }								
                            }
                            
                            NotFound = false;
                        }
                    }
                }
            }
        }
        //NOTE: Insert new entity into its memory block
        
        if(NewPosition)
        {
            world_chunk * WorldChunk = GetWorldChunk(World, NewPosition->ChunkX, NewPosition->ChunkY, NewPosition->ChunkZ, Arena);
            Assert(WorldChunk);
            world_entity_block * FirstBlock = &WorldChunk->FirstBlock;
            
            if(FirstBlock->EntityCount >= ArrayCount(FirstBlock->LowEntityIndex))
            {
                //NOTE: We got out of room for entities, add new entity_block
                world_entity_block * OldBlock = World->FirstFree;
                if(OldBlock)
                {
                    //NOTE:Why don't we just always move firstFree onwards?
                    World->FirstFree = OldBlock->NextBlock;
                }
                else
                {
                    OldBlock = PushStruct(Arena, world_entity_block);	 
                }
                
                *OldBlock = *FirstBlock;
                
                if(FirstBlock->NextBlock)
                {
                    OldBlock->NextBlock = FirstBlock->NextBlock;
                }
                
                FirstBlock->NextBlock = OldBlock;	
                
                FirstBlock->EntityCount = 0;
            }
            
            Assert(FirstBlock->EntityCount < ArrayCount(FirstBlock->LowEntityIndex));
            FirstBlock->LowEntityIndex[FirstBlock->EntityCount++] = LowEntityIndex;
        }
    }
}


internal void
ChangeEntityLocation(memory_arena * Arena, world * World , uint32 LowEntityIndex, 
                     low_entity * LowEntity, world_position NewPosition)
{
    TIMED_FUNCTION();
    
    world_position * OldP = 0;
    world_position * NewP = 0;
    
    if(!IsSet(&LowEntity->SimEntity, EntityFlag_Nonspatial) && IsValidPosition(&LowEntity->Position))
    {
        OldP = &LowEntity->Position;
    }
    
    if(IsValidPosition(&NewPosition))
    {
        NewP = &NewPosition;
    }
    
    
    ChangeEntityLocationRaw(Arena, World , LowEntityIndex,  OldP,  NewP);
    
    if(IsValidPosition(&NewPosition))
    {
        LowEntity->Position = NewPosition;
        ClearFlags(&LowEntity->SimEntity, EntityFlag_Nonspatial);
    }
    else
    {
        LowEntity->Position = NullPosition();
        AddFlags(&LowEntity->SimEntity, EntityFlag_Nonspatial);
    }
}














