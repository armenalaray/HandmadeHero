
#ifndef HANDMADE_TILE_H

//TODO: Replace this for a vector_3

struct world_position
{
    //NOTE: Seems that we need to search through world_positions to
    //gather up entity_references inside the sim_region, we cannot
    //implicitly now the chunk position for an entity_reference
    
    int32 ChunkX;
    int32 ChunkY;
    int32 ChunkZ;
    
    //NOTE:These are relative from the world_chunk center
    vector_3 Offset_;  
};

//TODO: Could make this just tile_chunk and then allow multiple tilechunks per X/Y/Z
struct world_entity_block
{
    uint32 EntityCount;
    uint32 LowEntityIndex[16];
    world_entity_block * NextBlock;
};

struct world_chunk
{
    bool32 Set;
    int32 X;
    int32 Y;
    int32 Z;
    //TODO: Profile this and determine if a pointer would be better here
    world_entity_block FirstBlock;
    
    world_chunk * NextInHash;
};

struct world
{
    vector_3 ChunkDimInMeters;
    
    world_entity_block * FirstFree;
    
    //TODO: TileChunkHash should a set of pointers IF 
    //tile_entity_blocks continue to be stored altogether directly in the tile_chunk.
    //NOTE: At the moment this has to be a power of two	
    world_chunk ChunkHash[4096];
};



#define HANDMADE_TILE_H
#endif