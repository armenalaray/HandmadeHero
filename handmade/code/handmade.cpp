#include "handmade.h"

#include "handmade_asset.cpp" 
#include "handmade_render_group.cpp"
#include "handmade_world.cpp"
#include "handmade_entity.cpp"
#include "handmade_simulation_region.cpp"
#include "handmade_audio.cpp"
#include "handmade_meta.cpp"

//#pragma optimize("", off)
inline world_position 
ChunkPositionFromTilePosition(world * World, int32 AbstileX, 
                              int32 AbstileY, int32 AbstileZ, 
                              vector_3 AdditionalOffset = Vector3(0.0f,0.0f,0.0f))
{
    world_position BasePos = {};
    
    real32 TileSideInMeters = 1.4f;
    real32 TileDepthInMeters = 3.0f;
    
    
    vector_3 TileDim = Vector3(TileSideInMeters, TileSideInMeters, TileDepthInMeters); 
    vector_3 Offset = Hadamard(TileDim , Vector3((real32)AbstileX, (real32)AbstileY, (real32)AbstileZ));
    
    world_position Result = MapIntoChunkSpace(World, BasePos,AdditionalOffset +  Offset);
    
    Assert(IsCannonical(World, Result.Offset_));
    return Result;
}


struct add_low_entity_result
{
    low_entity * Low;
    uint32 LowEntityIndex;
};


internal add_low_entity_result  
AddLowEntity(game_state * GameState, entity_type Type, world_position Position)
{
    add_low_entity_result Result = {};
    
    Assert(GameState->LowEntityCount < ArrayCount(GameState->LowEntities));
    Result.LowEntityIndex = GameState->LowEntityCount++;
    
    Result.Low = GameState->LowEntities + Result.LowEntityIndex;
    *Result.Low = {};
    Result.Low->Position = NullPosition();
    Result.Low->SimEntity.Type = Type;
    Result.Low->SimEntity.Collision = GameState->NullCollision;
    
    //TODO: Do we need a Begin/end paradigm for adding entities so that they
    //can be brought into the high set when they are added and are in camera space. 
    ChangeEntityLocation(&GameState->WorldArena, GameState->World , 
                         Result.LowEntityIndex,Result.Low , Position);	
    
    return Result;
}


internal add_low_entity_result  
AddGroundedEntity(game_state * GameState, entity_type Type, 
                  world_position Position, collision_volume_group * Collision)
{
    add_low_entity_result Entity = AddLowEntity(GameState, Type, Position);
    Entity.Low->SimEntity.Collision = Collision;
    return Entity;
}


internal add_low_entity_result
AddRoom(game_state * GameState,uint32 AbsTileX, uint32  AbsTileY, uint32 AbsTileZ)
{
    world_position Position = ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);
    add_low_entity_result Entity = AddGroundedEntity(GameState, EntityType_Room, 
                                                     Position, GameState->StandardRoomCollision);
    AddFlags(&Entity.Low->SimEntity, EntityFlag_Traversable);	
    
    return Entity;
}

internal add_low_entity_result   
AddWall(game_state * GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
    world_position Position = ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);
    add_low_entity_result Entity = AddGroundedEntity(GameState, EntityType_Wall, 
                                                     Position, GameState->WallCollision);
    AddFlags(&Entity.Low->SimEntity, EntityFlag_Collides);	
    return Entity;
}


internal add_low_entity_result   
AddStair(game_state * GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
    
    world_position Position = ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);
    add_low_entity_result Entity = AddGroundedEntity(GameState, 
                                                     EntityType_Stairwell, 
                                                     Position,  GameState->StairCollision);
    
    Entity.Low->SimEntity.WalkableHeight = GameState->TypicalFloorHeight;
    Entity.Low->SimEntity.WalkableDimension = Entity.Low->SimEntity.Collision->TotalVolume.Dimension.xy;
    
    AddFlags(&Entity.Low->SimEntity, EntityFlag_Collides);	
    
    return Entity;
}




internal void 
AddHitPoints(low_entity * Storage, uint32 HitPointSize)
{
    Assert(HitPointSize < ArrayCount(Storage->SimEntity.HitPoints));
    Storage->SimEntity.HitPointMax = HitPointSize;
    
    for(uint32 HitPointIndex = 0; HitPointIndex < Storage->SimEntity.HitPointMax; ++ HitPointIndex)
    {
        hit_point * HitPoint = Storage->SimEntity.HitPoints + HitPointIndex;
        HitPoint->Flags = 0;
        HitPoint->FilledAmount = HIT_POINT_SUB_COUNT;	
    }
}

internal add_low_entity_result   
AddMonster(game_state * GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
    world_position Position = ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);
    add_low_entity_result Entity = AddGroundedEntity(GameState, EntityType_Monster, 
                                                     Position, GameState->MonsterCollision);
    AddFlags(&Entity.Low->SimEntity, EntityFlag_Collides | EntityFlag_Movable);	
    AddHitPoints(Entity.Low, 3);
    
    return Entity;
}


internal add_low_entity_result   
AddSword(game_state * GameState)
{
    add_low_entity_result Entity = AddLowEntity(GameState, EntityType_Sword, NullPosition());
    
    Entity.Low->SimEntity.Collision = GameState->SwordCollision;
    //NOTE: Collides Flag added for making hitpoint test 
    AddFlags(&Entity.Low->SimEntity, EntityFlag_Movable | EntityFlag_Collides);	
    
    return Entity;
}

internal add_low_entity_result   
AddFamiliar(game_state * GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
    world_position Position = ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);
    add_low_entity_result Entity = AddGroundedEntity(GameState, EntityType_Familiar, 
                                                     Position, GameState->FamiliarCollision);
    AddFlags(&Entity.Low->SimEntity, EntityFlag_Collides | EntityFlag_Movable);	
    
    return Entity;
}

internal add_low_entity_result   
AddPlayer(game_state * GameState)
{
    //TODO(Alex): remove this hack!
    world_position Position = MapIntoChunkSpace(GameState->World, 
                                                GameState->CameraPosition, 
                                                Vector3(2.0f,0, 0));
    add_low_entity_result Entity = AddGroundedEntity(GameState, EntityType_Player,
                                                     Position, GameState->PlayerCollision);
    
    AddHitPoints(Entity.Low, 3);
    
    AddFlags(&Entity.Low->SimEntity, EntityFlag_Collides | EntityFlag_Movable);	
    
    add_low_entity_result Sword = AddSword(GameState);
    Entity.Low->SimEntity.SwordRef.StorageIndex = Sword.LowEntityIndex;
    
    if(GameState->CameraFollowingEntityIndex == 0)
    {
        GameState->CameraFollowingEntityIndex = Entity.LowEntityIndex;
    }
    
    return Entity;
}



internal void 
DrawHitPoints(render_group * RenderGroup, sim_entity * SimEntity)
{
    if(SimEntity->HitPointMax >= 1)
    {
        vector_2 HealthDimension = {0.2f ,0.2f};
        real32 SpacingX = 2.0f * HealthDimension.x;
        
        vector_2 HitPos = {-0.5f * SpacingX * (SimEntity->HitPointMax - 1), -0.5f};
        vector_2 DeltaHitPos = {SpacingX, 0.0f};
        
        for(uint32 HitPointIndex = 0; HitPointIndex < SimEntity->HitPointMax; ++HitPointIndex)
        {		
            hit_point * HitPoint = SimEntity->HitPoints + HitPointIndex;
            vector_4 Color = {1.0f, 0.0f, 0.0f, 1.0f};
            
            if(HitPoint->FilledAmount == 0)
            {
                Color = Vector4(0.5f, 0.5f, 0.5f, 1.0f);
            }
            
            PushRect(RenderGroup, Vector3(HitPos, 0), HealthDimension, Color);
            HitPos += DeltaHitPos;
        }	
    }
}



internal void 
ClearCollisionRulesFor(game_state * GameState, uint32 StorageIndex)
{
    //NOTE:Here we can optimize this by searching through the bucket where StorageIndex
    //is the smallest compared
    //NOTE: Another way to make it easier to make removal is to store the entity pair
    //in both sides as an entry for A and B, now its easier to remove, just iterate 
    //through storage index bucket list ,add them to the free list , finally iterate
    //through the free list and remove the pair in the opposite order.
    for(uint32 HashBucket = 0; HashBucket <  ArrayCount(GameState->CollisionRuleHash); ++HashBucket)
    {
        for(pairwise_collision_rule ** Rule = &GameState->CollisionRuleHash[HashBucket]; 
            *Rule;)
        {
            if(((*Rule)->StorageIndexA == StorageIndex) || ((*Rule)->StorageIndexB == StorageIndex))
            {
                pairwise_collision_rule * RemovedRule = *Rule;
                
                *Rule = (*Rule)->NextInHash; 
                
                RemovedRule->NextInHash = GameState->FirstFreeCollisionRule;
                GameState->FirstFreeCollisionRule = RemovedRule;
                
            }
            else
            {
                Rule = &(*Rule)->NextInHash;
            }
        }
    }
}

internal void	 
AddCollisionRule(game_state * GameState, uint32 StorageIndexA, uint32 StorageIndexB, bool32 CanCollide)
{
    
    if(StorageIndexA > StorageIndexB)
    {
        uint32 Temp = StorageIndexA;
        StorageIndexA = StorageIndexB;
        StorageIndexB = Temp;
    }
    
    //TODO: Better hash function
    pairwise_collision_rule * Found = 0;
    uint32 HashBucket = StorageIndexA & (ArrayCount(GameState->CollisionRuleHash) - 1);
    
    for(pairwise_collision_rule * CollisionRule = GameState->CollisionRuleHash[HashBucket];
        CollisionRule; 
        CollisionRule = CollisionRule->NextInHash)
    {
        if((CollisionRule->StorageIndexA == StorageIndexA) &&
           (CollisionRule->StorageIndexB == StorageIndexB))
        {
            Found = CollisionRule;
            break;
        }
    }
    
    if(!Found)
    {
        Found = GameState->FirstFreeCollisionRule;
        if(Found)
        {
            GameState->FirstFreeCollisionRule = Found->NextInHash;
        }
        else
        {
            Found = PushStruct(&GameState->WorldArena, pairwise_collision_rule);
        }
        
        Found->NextInHash = GameState->CollisionRuleHash[HashBucket];
        GameState->CollisionRuleHash[HashBucket] = Found;
    }
    
    Assert(Found);
    {
        Found->StorageIndexA = StorageIndexA;
        Found->StorageIndexB = StorageIndexB;
        Found->CanCollide = CanCollide;
    }
}

internal collision_volume_group * 
MakeSimpleGroundedCollision(game_state * GameState, real32 DimX, real32 DimY, real32 DimZ) 
{
    //TODO: Not WorldArena! Change to using the fundamental types arena, etc.
    collision_volume_group * Collision = PushStruct(&GameState->WorldArena, collision_volume_group);
    Collision->VolumeCount =  1;
    Collision->Volumes = PushArray(&GameState->WorldArena, Collision->VolumeCount, collision_volume);
    Collision->TotalVolume.OffsetP = Vector3(0,0,0.5f * DimZ);
    Collision->TotalVolume.Dimension = Vector3(DimX, DimY,DimZ);
    //NOTE: Initialize first and only volume as the total volume
    Collision->Volumes[0] = Collision->TotalVolume;
    
    return Collision;
}

internal collision_volume_group * 
MakeNullCollision(game_state * GameState)       
{
    //TODO: Not WorldArena! Change to using the fundamental types arena, etc.
    collision_volume_group * Collision = PushStruct(&GameState->WorldArena, collision_volume_group);
    Collision->VolumeCount =  0;
    Collision->Volumes = 0;
    //TODO: Should these be negative
    Collision->TotalVolume.OffsetP = Vector3(0,0,0);
    Collision->TotalVolume.Dimension = Vector3(0,0,0);
    
    return Collision;
}


internal task_with_memory * 
BeginTaskWithMemory(transient_state * TranState)
{
    task_with_memory * TaskFound = 0;
    for(uint32 TaskIndex = 0 ; 
        TaskIndex < ArrayCount(TranState->Tasks);
        ++TaskIndex)
    {
        task_with_memory * Task = TranState->Tasks + TaskIndex;
        
        if(!Task->BeingUsed)
        {
            TaskFound = Task;
            Task->BeingUsed = true;
            Task->MemoryFlush = BeginTempMemory(&Task->Arena);
            break;
        }
    }
    return TaskFound;
}

internal void
EndTaskWithMemory(task_with_memory * Task)
{
    EndTempMemory(Task->MemoryFlush);
    
    CompletePreviousWritesBeforeFututeWrites;
    Task->BeingUsed = false;
}

struct fill_ground_chunk_work
{
    task_with_memory * Task;
    transient_state * TranState;
    game_state * GameState;
    ground_buffer * GroundBuffer;
    world_position ChunkP;
};

internal PLATFORM_WORK_QUEUE_CALLBACK(FillGroundChunkWork)
{
    TIMED_FUNCTION();
    
    fill_ground_chunk_work * Work  = (fill_ground_chunk_work *)Data;
    
    loaded_bitmap * Buffer = &Work->GroundBuffer->Bitmap;
    //TODO: Decide what our pushbuffer size is 
    //TODO: Safe cast from uint64 to uint32
    
    render_group * RenderGroup = AllocateRenderGroup(Work->TranState->Assets, &Work->Task->Arena, 
                                                     (uint32)GetRemainingArenaSize(&Work->Task->Arena), true);	
    BeginRenderGroup(RenderGroup);
    
    real32 Width = Work->GameState->World->ChunkDimInMeters.x;
    real32 Height = Work->GameState->World->ChunkDimInMeters.y;
    
    Assert(Width ==  Height);
    vector_2 HalfDim = 0.5f * Vector2(Width, Height);
    
    Ortographic(RenderGroup, Buffer->Width, Buffer->Height, (Buffer->Width - 2) / Width);
    
    Clear(RenderGroup, Vector4(1.0f,0.5f,0.0f,1.0f));
    
    //TODO: Make random number generation more systemic 
    //TODO: Look into some spatial seed generation.
    
    for(int32 ChunkOffsetY = -1; 
        ChunkOffsetY <= 1; 
        ++ChunkOffsetY)
    {
        for(int32 ChunkOffsetX =  -1;
            ChunkOffsetX <= 1; 
            ++ChunkOffsetX)
        {
            
            int32 ChunkX = Work->ChunkP.ChunkX + ChunkOffsetX;
            int32 ChunkY = Work->ChunkP.ChunkY + ChunkOffsetY;
            int32 ChunkZ = Work->ChunkP.ChunkZ;
            
            vector_4 Color  = {1.0f,1.0f,1.0f,1.0f};
            if(DEBUG_SWITCH(GroundChunkCheckerBoard))
            {
                
                Color  = {1.0f,0.0f,0.0f,1.0f};
                if(ChunkY < 0)
                {
                    int X = 5;
                }
                
                if(AbsValue((r32)(ChunkX % 2)) == AbsValue((r32)(ChunkY % 2)))
                {
                    Color = Vector4(0.0f,0.0f,1.0f,1.0f);
                }
            }
            
            random_series Series = Seed(139 * ChunkX + 593 * ChunkY + 329 * ChunkZ);
            vector_2 ChunkCenter = Vector2(ChunkOffsetX * Width, ChunkOffsetY * Height);  
            
            for(uint32 GrassIndex = 0; 
                GrassIndex < 100; 
                ++GrassIndex)
            {
                bitmap_id Stamp = GetRandomBitmapFrom(Work->TranState->Assets,&Series, (RandomChoice(&Series, 2)) ? Asset_Grass : Asset_Stone);
                
                vector_2 Offset  = Hadamard(HalfDim,  Vector2(RandomBilateral(&Series), RandomBilateral(&Series)));
                vector_2 Position = ChunkCenter + Offset;
                
                PushBitmap(RenderGroup, Stamp, 2.0f,Vector3(Position, 0), Color);
            }
        }
    }
    
    for(int32 ChunkOffsetY =  -1; 
        ChunkOffsetY <= 1; 
        ++ChunkOffsetY)
    {
        for(int32 ChunkOffsetX =  -1;
            ChunkOffsetX <= 1; 
            ++ChunkOffsetX)
        {
            
            int32 ChunkX = Work->ChunkP.ChunkX + ChunkOffsetX;
            int32 ChunkY = Work->ChunkP.ChunkY + ChunkOffsetY;
            int32 ChunkZ = Work->ChunkP.ChunkZ;
            
            random_series Series = Seed(139 * ChunkX + 593 * ChunkY + 329 * ChunkZ);
            
            vector_2 ChunkCenter = Vector2(ChunkOffsetX * Width, ChunkOffsetY * Height);  
            
            for(uint32 GrassIndex = 0;
                GrassIndex < 100; 
                ++GrassIndex)
            {
                bitmap_id Stamp = GetRandomBitmapFrom(Work->TranState->Assets,&Series, Asset_Tuft);
                
                vector_2 Offset  = Hadamard(HalfDim,  Vector2(RandomBilateral(&Series), RandomBilateral(&Series)));
                vector_2 Position = ChunkCenter + Offset;
                
                PushBitmap(RenderGroup, Stamp, 0.2f, Vector3(Position, 0));
            }
        }
    }
    
    Assert(AllResourcesPresent(RenderGroup));
    
    RenderGroupToOutput(RenderGroup, Buffer);
    EndRenderGroup(RenderGroup);
    EndTaskWithMemory(Work->Task);
}


//#pragma optimize("", on)
internal void 
FillGroundChunk(transient_state * TranState, game_state * GameState, ground_buffer * GroundBuffer, 
                world_position * ChunkP)
{
    task_with_memory * Task = BeginTaskWithMemory(TranState);
    if(Task)
    {
        fill_ground_chunk_work * Work = PushStruct(&Task->Arena,fill_ground_chunk_work);
        
        GroundBuffer->Position = *ChunkP;
        
        Work->Task = Task;
        Work->TranState = TranState;
        Work->GameState = GameState;
        Work->ChunkP = *ChunkP;
        Work->GroundBuffer = GroundBuffer;
        
        Platform.AddEntry(TranState->LowPriorityQueue, FillGroundChunkWork, Work);
    }
}
//#pragma optimize("", off)

internal void 
ClearBitmap(loaded_bitmap * Bitmap)
{
    if(Bitmap->Memory)
    {
        memory_index LoadedBitmapSize = Bitmap->Width * Bitmap->Height * BITMAP_BYTES_PER_PIXEL;
        ZeroSize(LoadedBitmapSize, Bitmap->Memory);
    }
}

internal loaded_bitmap
MakeEmptyBitmap(memory_arena * Arena , uint32 Width, uint32 Height, bool32 ClearToZero = false)
{
    loaded_bitmap Result = {};
    Result.Width = Width;
    Result.Height = Height;
    Result.Pitch = Result.Width * BITMAP_BYTES_PER_PIXEL;
    Result.WidthOverHeight = SafeRatio0((real32)Result.Width, (real32)Result.Height);
    Result.AlignPorcentage = Vector2(0.5f,0.5f);
    
    memory_index LoadedBitmapSize = Width * Height * BITMAP_BYTES_PER_PIXEL;
    Result.Memory = PushSize(Arena, LoadedBitmapSize, 16);
    if(ClearToZero)
    {
        ClearBitmap(&Result);
    }
    return Result;
}

internal void 
MakeSphereNormalMap(loaded_bitmap * Bitmap, real32 Roughness, real32 Cx = 1.0f, real32 Cy = 1.0f)
{
    //NOTE: Normal w is map's roughness, goes from 0 - 1 
    real32 InvWidthMax = 1.0f / (real32)(Bitmap->Width - 1);
    real32 InvHeightMax = 1.0f / (real32)(Bitmap->Height - 1);
    
    uint8 * Row = (uint8 *)Bitmap->Memory;
    
    for(int32 Y = 0; 
        Y < Bitmap->Height; 
        ++Y)
    {
        uint32 * Pixel = (uint32 *)Row;
        for(int32 X = 0; 
            X < Bitmap->Width; 
            ++X)
        {
            vector_2 BitmapUV = {(real32)X * InvWidthMax, (real32)Y * InvHeightMax};
            
            vector_3 Normal = {0.0f,0.70710678118654f,0.70710678118654f};
            
            real32 XNormal = Cx * (2.0f * BitmapUV.x - 1.0f);
            real32 YNormal = Cy * (2.0f * BitmapUV.y - 1.0f);
            real32 ZNormal = 1.0f;
            
            real32 RootDenominator =  1.0f - XNormal * XNormal - YNormal * YNormal;
            
            if(RootDenominator >= 0)
            {
                ZNormal = SquareRoot(RootDenominator);
                Normal = {XNormal, YNormal, ZNormal};
            }
            
            Normal = Normalize(Normal);
            
            vector_4 Color = {
                255.0f * 0.5f * (Normal.x + 1.0f),
                255.0f * 0.5f * (Normal.y + 1.0f), 
                255.0f * 0.5f * (Normal.z + 1.0f),
                255.0f *  Roughness
            };
            
            *Pixel = (((uint32)(Color.a + 0.5f) << 24)
                      | ((uint32)(Color.x + 0.5f) << 16)
                      | ((uint32)(Color.y + 0.5f) << 8)
                      | ((uint32)(Color.z + 0.5f) << 0));
            
            ++Pixel;
        }
        Row += Bitmap->Pitch;
    }
}

internal void 
MakeSphereDiffuseMap(loaded_bitmap * Bitmap, real32 Cx = 1.0f, real32 Cy = 1.0f)
{
    real32 InvWidthMax = 1.0f / (real32)(Bitmap->Width - 1);
    real32 InvHeightMax = 1.0f / (real32)(Bitmap->Height - 1);
    
    uint8 * Row = (uint8 *)Bitmap->Memory;
    
    for(int32 Y = 0; 
        Y < Bitmap->Height; 
        ++Y)
    {
        uint32 * Pixel = (uint32 *)Row;
        for(int32 X = 0; 
            X < Bitmap->Width; 
            ++X)
        {
            vector_2 BitmapUV = {(real32)X * InvWidthMax, (real32)Y * InvHeightMax};
            
            real32 XNormal = Cx * (2.0f * BitmapUV.x - 1.0f);
            real32 YNormal = Cy * (2.0f * BitmapUV.y - 1.0f);
            
            real32 RootDenominator =  1.0f - XNormal * XNormal - YNormal * YNormal;
            
            real32 Alpha = 0.0f;
            if(RootDenominator >= 0)
            {
                Alpha = 1.0f;
            }
            
            vector_3 BaseColor = {0.0f,0.0f,0.0f};
            
            Alpha *= 255.0f;
            vector_4 Color = {
                Alpha * (BaseColor.x),
                Alpha * (BaseColor.y), 
                Alpha * (BaseColor.z),
                Alpha
            };
            
            *Pixel = (((uint32)(Color.a + 0.5f) << 24)
                      | ((uint32)(Color.x + 0.5f) << 16)
                      | ((uint32)(Color.y + 0.5f) << 8)
                      | ((uint32)(Color.z + 0.5f) << 0));
            
            ++Pixel;
        }
        Row += Bitmap->Pitch;
    }
}


internal void 
MakePyramidNormalMap(loaded_bitmap * Bitmap, real32 Roughness)
{
    real32 InvWidthMax = 1.0f / (real32)(Bitmap->Width - 1);
    real32 InvHeightMax = 1.0f / (real32)(Bitmap->Height - 1);
    
    uint8 * Row = (uint8 *)Bitmap->Memory;
    
    for(int32 Y = 0; 
        Y < Bitmap->Height; 
        ++Y)
    {
        uint32 * Pixel = (uint32 *)Row;
        for(int32 X = 0; 
            X < Bitmap->Width; 
            ++X)
        {
            vector_2 BitmapUV = {(real32)X * InvWidthMax, (real32)Y * InvHeightMax};
            
            vector_3 Normal = {};
            
            if((BitmapUV.x < BitmapUV.y) && (BitmapUV.y < (1 - BitmapUV.x)))
            {
                //NOTE: Left hand side 
                Normal = {-0.70710678118654f, 0.0f, 0.70710678118654f};
            }
            else if((BitmapUV.x < BitmapUV.y) && (BitmapUV.y >= (1 - BitmapUV.x)))
            {
                Normal = {0.0f, 0.70710678118654f, 0.70710678118654f};
            }
            else if((BitmapUV.x >= BitmapUV.y) && (BitmapUV.y < (1 - BitmapUV.x)))
            {
                Normal = {0.0f,-0.70710678118654f, 0.70710678118654f};
            }
            else if((BitmapUV.x >= BitmapUV.y) && (BitmapUV.y >= (1 - BitmapUV.x)))
            {
                Normal = {0.70710678118654f, 0.0f, 0.70710678118654f};
            }
            
            vector_4 Color = {
                255.0f * 0.5f * (Normal.x + 1.0f),
                255.0f * 0.5f * (Normal.y + 1.0f), 
                255.0f * 0.5f * (Normal.z + 1.0f),
                255.0f *  Roughness
            };
            
            *Pixel = (((uint32)(Color.a + 0.5f) << 24)
                      | ((uint32)(Color.x + 0.5f) << 16)
                      | ((uint32)(Color.y + 0.5f) << 8)
                      | ((uint32)(Color.z + 0.5f) << 0));
            
            ++Pixel;
        }
        Row += Bitmap->Pitch;
    }
}

#if HANDMADE_INTERNAL
global_variable game_memory * GlobalDebugMemory;
#endif

inline game_assets * 
DEBUGGetGameAssets(game_memory * Memory)
{
    game_assets * Assets = 0;
    transient_state * TranState = (transient_state*)Memory->TransientStorage;
    if(TranState->IsInitialized)
    {
        Assets = TranState->Assets;
    }
    
    return Assets;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    TIMED_FUNCTION();
    
#if HANDMADE_INTERNAL
    GlobalDebugMemory = Memory;
#endif
    
    StringsAreEqual("Hola","Hol");
    
    Platform = Memory->PlatformAPI;
    
    Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].MoveUp) == 
           (ArrayCount(Input->Controllers[0].Buttons)-1));
    
    uint32 GroundBufferWidth = 256;
    uint32 GroundBufferHeight = 256;
    
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
    game_state * GameState = (game_state *)Memory->PermanentStorage;
    if(!GameState->IsInitialized)
    {
        uint32 TilesPerWidth = 17;
        uint32 TilesPerHeight = 9;
        
        GameState->TypicalFloorHeight = 3.0f;
        
        //TODO: Remove this!
        real32 PixelsToMeters = 1.0f / 42.0f;
        vector_3 GrounChunkDim = {(real32)GroundBufferWidth * PixelsToMeters, 
            (real32)GroundBufferHeight * PixelsToMeters, GameState->TypicalFloorHeight};
        
        
        InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
                        (uint8*)Memory->PermanentStorage + sizeof(game_state));
        
        InitializeAudioState(&GameState->AudioState, &GameState->WorldArena);
        
        GameState->World = PushStruct(&GameState->WorldArena, world);
        world * World = GameState->World;
        
        InitializeWorld(World, GrounChunkDim);
        
        //NOTE:Reserve entity slot 0 for null entity
        AddLowEntity(GameState, EntityType_Null, NullPosition());
        
        GameState->NullCollision = MakeNullCollision(GameState);
        GameState->SwordCollision = MakeSimpleGroundedCollision(GameState, 1.0f, 0.5f, 0.1f);
        GameState->NullCollision  = MakeSimpleGroundedCollision(GameState, 1.0f, 0.5f, 0.1f);;
        GameState->PlayerCollision = MakeSimpleGroundedCollision(GameState, 1.0f,0.5f,1.2f);
        GameState->MonsterCollision = MakeSimpleGroundedCollision(GameState, 1.0f,0.5f,0.5f);
        GameState->FamiliarCollision = MakeSimpleGroundedCollision(GameState, 1.0f, 0.5f, 0.5f);
        
        
        real32 TileSideInMeters = 1.4f;
        real32 TileDepthInMeters = GameState->TypicalFloorHeight;
        
        GameState->WallCollision = MakeSimpleGroundedCollision(GameState,
                                                               TileSideInMeters, 
                                                               TileSideInMeters, 
                                                               TileDepthInMeters);
        
        GameState->StairCollision = MakeSimpleGroundedCollision(GameState,
                                                                TileSideInMeters, 
                                                                2.0f * TileSideInMeters, 
                                                                1.1f * TileDepthInMeters);
        
        GameState->StandardRoomCollision = MakeSimpleGroundedCollision(GameState,
                                                                       TilesPerWidth * TileSideInMeters, 
                                                                       TilesPerHeight * TileSideInMeters, 
                                                                       0.9f * TileDepthInMeters);
        
        
        GameState->GeneralEntropy = Seed(1234);
        GameState->EffectsEntropy = Seed(1234);
        
        uint32 ScreenBaseX = 0;
        uint32 ScreenBaseY = 0;
        uint32 ScreenBaseZ = 0;
        
        uint32 ScreenX = ScreenBaseX;
        uint32 ScreenY = ScreenBaseY;
        
        uint32 AbsTileZ = ScreenBaseZ;
        
        
        //GameState->CameraPosition = ChunkPositionFromTilePosition(World, 17 / 2, 9 / 2, AbsTileZ);
        
        bool32 DoorLeft = false;
        bool32 DoorRight = false;
        bool32 DoorTop = false;
        bool32 DoorBottom = false;
        bool32 DoorUp = false;
        bool32 DoorDown = false;
        
        for(uint32 ScreenIndex = 0; 
            ScreenIndex < 1000; 
            ++ScreenIndex)
        {	
#if 1
            uint32 DoorDirection = RandomChoice(&GameState->GeneralEntropy, (DoorUp || DoorDown) ? 2 : 4); 
#else
            uint32 DoorDirection = RandomChoice(&GameState->GeneralEntropy, 2); 
#endif
            bool32 CreatedZDoor = false;
            
            //DoorDirection = 3;
            
            if(DoorDirection == 3)
            {
                CreatedZDoor = true;
                DoorDown = true;
            }
            else if(DoorDirection == 2)
            {
                CreatedZDoor = true;
                DoorUp = true;	
            }
            else if(DoorDirection == 1)
            {
                DoorRight = true;
            }
            else
            {
                DoorTop = true;
            }
            
            AddRoom(GameState, ScreenX * TilesPerWidth + (TilesPerWidth / 2), 
                    ScreenY * TilesPerHeight +  (TilesPerHeight / 2), AbsTileZ);
            
            for(uint32 TileY = 0 ; TileY < TilesPerHeight ; ++TileY)
            {
                for(uint32 TileX = 0 ; TileX < TilesPerWidth; ++TileX)
                {
                    uint32 AbsTileX = ScreenX * TilesPerWidth + TileX;
                    uint32 AbsTileY = ScreenY * TilesPerHeight + TileY;
                    
                    uint32 ShouldBeWall = false;
                    if((TileX == 0) && (!DoorLeft || (TileY != TilesPerHeight / 2)))
                    {
                        ShouldBeWall = true;
                    }
                    if(TileX == (TilesPerWidth - 1) && (!DoorRight || (TileY != TilesPerHeight / 2)))
                    {
                        ShouldBeWall = true;
                    }
                    if((TileY == 0) && (!DoorBottom || (TileX != TilesPerWidth / 2))) 
                    {
                        ShouldBeWall = true;
                    }	
                    if(TileY == (TilesPerHeight - 1) && (!DoorTop || (TileX != TilesPerWidth / 2))) 
                    {
                        ShouldBeWall = true;
                    }
                    if(ShouldBeWall)
                    {
                        AddWall(GameState, AbsTileX, AbsTileY, AbsTileZ);	
                        
                    }
                    else if(CreatedZDoor)
                    {
                        if(((AbsTileZ % 2) && (TileX == 10) && (TileY == 5)) || 
                           (!(AbsTileZ % 2) && (TileX == 5) && (TileY == 5)))
                        {
                            AddStair(GameState, AbsTileX, AbsTileY, DoorDown ? (AbsTileZ - 1) : AbsTileZ);
                        }
                    }
                }
            }	
            
            DoorLeft = DoorRight;
            DoorBottom = DoorTop;
            DoorRight = false;
            DoorTop = false;
            
            if(CreatedZDoor)
            {
                DoorDown = !DoorDown;
                DoorUp = !DoorUp;
            }
            else 
            {
                DoorDown = false;
                DoorUp = false;
            }
            
            //Next room direction 
            if(DoorDirection == 3)
            {
                AbsTileZ--; 
            }
            else if(DoorDirection == 2)
            {
                AbsTileZ++;					
            }
            else if(DoorDirection == 1)
            {
                ScreenX += 1;
            }
            else
            {
                ScreenY += 1;
            }
        }           
        
        uint32 TileCameraX = ScreenBaseX * TilesPerWidth + 17 / 2;
        uint32 TileCameraY = ScreenBaseY * TilesPerHeight + 9 / 2;
        uint32 TileCameraZ = ScreenBaseZ;
        
        world_position NewCameraPosition = {};
        NewCameraPosition = ChunkPositionFromTilePosition(World, TileCameraX, TileCameraY, TileCameraZ);
        
        GameState->CameraPosition = NewCameraPosition;
        
        AddMonster(GameState, TileCameraX - 2,TileCameraY + 2,TileCameraZ);
        
        for(uint32 FamiliarIndex = 0; FamiliarIndex < 1; ++FamiliarIndex)
        {
            int32 FamiliarOffsetX =  RandomBetween(&GameState->GeneralEntropy, -5 , 5);
            int32 FamiliarOffsetY = RandomBetween(&GameState->GeneralEntropy, -3 , -2);
            
            AddFamiliar(GameState, TileCameraX + FamiliarOffsetX, TileCameraY + FamiliarOffsetY, TileCameraZ);	
        }
        
        GameState->IsInitialized = true;
    }
    
    //NOTE: Transient state initialization
    Assert(sizeof(transient_state) <= Memory->TransientStorageSize);
    
    transient_state * TranState = (transient_state *) Memory->TransientStorage;
    
    TranState->HighPriorityQueue = Memory->HighPriorityQueue;
    TranState->LowPriorityQueue = Memory->LowPriorityQueue;
    
    if(!TranState->IsInitialized)
    {
        InitializeArena(&TranState->TranArena, Memory->TransientStorageSize - sizeof(transient_state),
                        (uint8*)Memory->TransientStorage + sizeof(transient_state));
        
        for(uint32 TaskIndex = 0; 
            TaskIndex < ArrayCount(TranState->Tasks);
            ++TaskIndex)
        {
            task_with_memory * Task = TranState->Tasks + TaskIndex;
            Task->BeingUsed = false;
            SubArena(&Task->Arena, &TranState->TranArena, Megabytes(1));
        }
        
        TranState->Assets = AllocateGameAssets(&TranState->TranArena, TranState, Megabytes(16));
        GameState->MusicSound = PlaySound(&GameState->AudioState, GetFirstSoundFrom(TranState->Assets, Asset_Music));
        
        //TODO: Choose a real number for this
        TranState->GroundBufferCount = 256; //128;
        TranState->GroundBuffer = PushArray(&TranState->TranArena, 
                                            TranState->GroundBufferCount, ground_buffer);
        for(uint32 GroundIndex = 0; 
            GroundIndex < TranState->GroundBufferCount;
            ++GroundIndex)
        {
            ground_buffer * GroundBuffer = TranState->GroundBuffer + GroundIndex;
            
            GroundBuffer->Position = NullPosition();
            GroundBuffer->Bitmap = MakeEmptyBitmap(&TranState->TranArena, 
                                                   GroundBufferWidth, 
                                                   GroundBufferHeight, false);
        }
        
        
        
        GameState->TestDiffuse = MakeEmptyBitmap(&TranState->TranArena, 256,256, false);
        //DrawRectangle(&GameState->TestDiffuse, Vector2(0,0), Vector2i(GameState->TestDiffuse.Width, GameState->TestDiffuse.Height), Vector4(0.5f,0.5f,0.5f,1.0f));
        GameState->TestNormal = MakeEmptyBitmap(&TranState->TranArena, GameState->TestDiffuse.Width, 
                                                GameState->TestDiffuse.Height, false);
        MakeSphereNormalMap(&GameState->TestNormal, 0.0f, 1.0f,1.0f);
        MakeSphereDiffuseMap(&GameState->TestDiffuse);
        //MakePyramidNormalMap(&GameState->TestNormal, 0.0f);
        
        
        TranState->EnvMapWidth = 512;
        TranState->EnvMapHeight = 256;
        
        for(uint32 EnvMapIndex = 0; 
            EnvMapIndex < ArrayCount(TranState->EnvMaps); 
            ++EnvMapIndex)
        {
            environment_map * EnvMap = TranState->EnvMaps + EnvMapIndex;
            
            uint32 LODWidth = TranState->EnvMapWidth;
            uint32 LODHeight = TranState->EnvMapHeight;
            
            for(uint32 LODIndex = 0;
                LODIndex < ArrayCount(EnvMap->LOD);
                ++LODIndex)
            {
                EnvMap->LOD[LODIndex] = MakeEmptyBitmap(&TranState->TranArena, LODWidth, LODHeight, false);
                
                LODWidth >>= 1;
                LODHeight >>= 1;
            }
        }
        
        
        TranState->IsInitialized = true;
    }
    
    if(DEBUG_SWITCH(GroundChunkReloading))
    {
        if(Memory->ExecutableReloaded)
        {
            for(uint32 GroundIndex = 0; 
                GroundIndex < TranState->GroundBufferCount;
                ++GroundIndex)
            {
                ground_buffer * GroundBuffer = TranState->GroundBuffer + GroundIndex;
                GroundBuffer->Position = NullPosition();
            }
        }
    }
    
    {
        vector_2 Volume = {};
        Volume.E[1] = SafeRatio0((r32)Input->MouseX , (r32)Buffer->Width);
        Volume.E[0] = 1.0f - Volume.E[1];
        
        vector_2 Transition = {2.0f,2.0f};
        
        ChangeVolume(GameState->MusicSound, Volume, Transition);
    }
    
    world * World = GameState->World;
    
    //NOTE: Update player motion
    for(int ControllerIndex = 0; 
        ControllerIndex < ArrayCount(Input->Controllers); 
        ++ControllerIndex)
    {
        game_controller_input * Controller = GetController(Input, ControllerIndex);
        controlled_hero * ControlledHero = GameState->ControlledHeroes + ControllerIndex;
        
        if(ControlledHero->EntityIndex == 0)
        {
            if(Controller->Start.ButtonEndedDown)
            {
                *ControlledHero = {};
                ControlledHero->EntityIndex = AddPlayer(GameState).LowEntityIndex;		
            }				
        }
        else
        {
            ControlledHero->PlayerAcceleration = {};
            ControlledHero->ZVelocity = 0.0f;
            ControlledHero->SwordVelocity = {};
            
            if(Controller->IsAnalog)
            {
                //NOTE: Use analog movement tuning
                ControlledHero->PlayerAcceleration = Vector2(Controller->StickAverageX,
                                                             Controller->StickAverageY);
            }
            else
            {
                //NOTE: Use digital movement tuning
                //TODO: Make opposite movements cancel through vector addition
                if(Controller->MoveUp.ButtonEndedDown)
                {	
                    ControlledHero->PlayerAcceleration.y = 1.0f;
                }
                if(Controller->MoveDown.ButtonEndedDown)
                {	
                    ControlledHero->PlayerAcceleration.y = -1.0f;
                }
                if(Controller->MoveLeft.ButtonEndedDown)
                {	
                    ControlledHero->PlayerAcceleration.x = -1.0f;
                }
                if(Controller->MoveRight.ButtonEndedDown)
                {
                    ControlledHero->PlayerAcceleration.x = 1.0f;
                }
            }
            
            if(Controller->Start.ButtonEndedDown)
            {
                ControlledHero->ZVelocity = 3.0f;
            }		
            
            ControlledHero->SwordVelocity = {};
            
            
            if(Controller->ActionUp.ButtonEndedDown)
            {
                ChangeVolume(GameState->MusicSound, Vector2(1.0f,1.0f), Vector2(5.0f, 5.0f));
                ControlledHero->SwordVelocity = Vector2(0.0f,1.0f);
            }
            if(Controller->ActionDown.ButtonEndedDown)
            {
                ChangeVolume(GameState->MusicSound, Vector2(0.0f,0.0f), Vector2(5.0f, 5.0f));
                ControlledHero->SwordVelocity = Vector2(0.0f,-1.0f);
            }
            if(Controller->ActionLeft.ButtonEndedDown)
            {
                ChangeVolume(GameState->MusicSound, Vector2(1.0f,0.0f), Vector2(5.0f, 5.0f));
                ControlledHero->SwordVelocity = Vector2(-1.0f,0.0f);
            }
            if(Controller->ActionRight.ButtonEndedDown)
            {
                ChangeVolume(GameState->MusicSound, Vector2(0.0f,1.0f), Vector2(5.0f, 5.0f));
                ControlledHero->SwordVelocity = Vector2(1.0f,0.0f);
            }
            
        }
    }
    
    
    //NOTE: RENDERER -------------------------------------------------------------------
    temp_memory RenderMemory = BeginTempMemory(&TranState->TranArena);
    
    loaded_bitmap DrawBuffer_ = {};
    loaded_bitmap * DrawBuffer = &DrawBuffer_;
    DrawBuffer->Width = Buffer->Width;
    DrawBuffer->Height = Buffer->Height;
    DrawBuffer->Pitch = Buffer->Pitch;
    DrawBuffer->Memory = Buffer->Memory;
    
    //NOTE: We could just allocate rendergroup once if we wanted
    render_group * RenderGroup = AllocateRenderGroup(TranState->Assets, &TranState->TranArena, Megabytes(4), false);	
    BeginRenderGroup(RenderGroup);
    
    real32 WidthOfMonitor = 0.635f; //NOTE: Horizontal measurement of monitor in meters.
    real32 MetersToPixels = DrawBuffer->Width * WidthOfMonitor; 
    real32 InitCameraToMonitorZ = 0.6f;
    real32 InitCameraToTargetZ = 9.0f;
    Perspective(RenderGroup, DrawBuffer->Width, DrawBuffer->Height, MetersToPixels, InitCameraToMonitorZ, InitCameraToTargetZ);
    
    Clear(RenderGroup, Vector4(0.3f, 0.4f, 0.3f, 1.0f));
    vector_2 ScreenCenter = {(real32)DrawBuffer->Width * 0.5f, (real32)DrawBuffer->Height * 0.5f};
    
    rectangle_2 ScreenBounds = GetCameraRectangleAtTarget(RenderGroup);
    rectangle_3 CameraBoundsInMeters = RectMinMax(Vector3(ScreenBounds.Min, 0.0f),Vector3(ScreenBounds.Max, 0));
    
    //TODO:Actually handle this properly so updatable bounds are represented correctly
    CameraBoundsInMeters.Min.z = -2.0f * GameState->TypicalFloorHeight;
    CameraBoundsInMeters.Max.z = 0.8f * GameState->TypicalFloorHeight;
    
    //NOTE: Push GroundBuffers to renderer
    for(uint32 GroundIndex = 0; 
        GroundIndex < TranState->GroundBufferCount; 
        ++GroundIndex)
    {
        ground_buffer * GroundBuffer = TranState->GroundBuffer + GroundIndex;
        if(IsValidPosition(&GroundBuffer->Position))
        {
            loaded_bitmap * Bitmap = &GroundBuffer->Bitmap;
            vector_3 Delta = Subtract(GameState->World, &GroundBuffer->Position, 
                                      &GameState->CameraPosition);
            if(Delta.z > -1.0f && Delta.z < 1.0f)
            {
                real32 GroundSideInMeters = World->ChunkDimInMeters.x;
                PushBitmap(RenderGroup, Bitmap, GroundSideInMeters, Delta);
                
                if (DEBUG_SWITCH(GroundChunkOutlines))
                {
                    PushRectOutline(RenderGroup, Delta, World->ChunkDimInMeters.xy , Vector4(1.0f, 1.0f, 0.0f, 1.0f));
                }
                
            }
        }
    }
    
    //NOTE: FillGroundChunk Selection based on furthest one
    {
        world_position MinChunkPosition = MapIntoChunkSpace(World, GameState->CameraPosition, GetMin(CameraBoundsInMeters));
        world_position MaxChunkPosition = MapIntoChunkSpace(World, GameState->CameraPosition, GetMax(CameraBoundsInMeters));
        
        for(int32 ChunkZ = MinChunkPosition.ChunkZ; 
            ChunkZ <= MaxChunkPosition.ChunkZ; ++ChunkZ)
        {
            for(int32 ChunkY = MinChunkPosition.ChunkY; 
                ChunkY <= MaxChunkPosition.ChunkY; ++ChunkY)
            {
                for(int32 ChunkX = MinChunkPosition.ChunkX; 
                    ChunkX <= MaxChunkPosition.ChunkX; ++ChunkX)
                {
                    //world_chunk * Chunk = GetWorldChunk(World, ChunkX,  ChunkY, ChunkZ);
                    //if(Chunk)
                    {
                        world_position ChunkCenterP = CenteredChunkPosition(ChunkX, ChunkY, ChunkZ);
                        vector_3 RelP = Subtract(World, &ChunkCenterP, &GameState->CameraPosition);
                        
                        real32 FurthestBufferLengthSq = 0;
                        ground_buffer * FurthestBuffer = 0;
                        
                        for(uint32 GroundBufferIndex = 0; 
                            GroundBufferIndex < TranState->GroundBufferCount; 
                            ++GroundBufferIndex)
                        {
                            ground_buffer * GroundBuffer = TranState->GroundBuffer + GroundBufferIndex;
                            
                            if(AreInSameChunk(World, &GroundBuffer->Position, &ChunkCenterP))
                            {
                                FurthestBuffer = 0;
                                break;
                            }
                            else if(IsValidPosition(&GroundBuffer->Position))
                            {
                                //NOTE:If buffer is already filled
                                vector_3 RelGroundP = Subtract(World, &GroundBuffer->Position, &GameState->CameraPosition);
                                real32 BufferLengthSq = LengthSq(RelGroundP);
                                if(FurthestBufferLengthSq < BufferLengthSq)
                                {
                                    FurthestBufferLengthSq = BufferLengthSq; 
                                    FurthestBuffer = GroundBuffer;
                                }
                            }
                            else
                            {
                                //NOTE: Priority is fill uninitialized buffers
                                FurthestBufferLengthSq = Real32Max;
                                FurthestBuffer = GroundBuffer;
                            }
                        }
                        
                        if(FurthestBuffer)
                        {
                            FillGroundChunk(TranState, GameState, FurthestBuffer, &ChunkCenterP);
                        }
                    }
                }
            }
        }
    }
    
    vector_3 SimBoundsExpansion = {15.0f, 15.0f, 0};
    rectangle_3 SimBounds = AddRadiusTo(CameraBoundsInMeters,SimBoundsExpansion);
    
    temp_memory SimMemory = BeginTempMemory(&TranState->TranArena);
    world_position SimCenterP = GameState->CameraPosition;
    sim_region * SimRegion = BeginSim(GameState, &TranState->TranArena, GameState->World, 
                                      SimCenterP, SimBounds,
                                      Input->dtForFrame);
    
    //TODO(Alex): Add some width to the outlines so its visible from far away!
    PushRectOutline(RenderGroup, Vector3(0,0,0), GetDim(CameraBoundsInMeters).xy,
                    Vector4(0.5f,0.0f,1.0f,1.0f));
    PushRectOutline(RenderGroup, Vector3(0,0,0), GetDim(SimBounds).xy,
                    Vector4(1.0f,0.5f,0.0f,1.0f));
    
    PushRectOutline(RenderGroup, Vector3(0,0,0), GetDim(SimRegion->UpdatableBounds).xy,
                    Vector4(1.0f,1.0f,0.0f,1.0f));
    
    PushRectOutline(RenderGroup, Vector3(0,0,0), GetDim(SimRegion->Bounds).xy,
                    Vector4(1.0f,0.0f,0.0f,1.0f));
    
    //NOTE: this handles the possibility of having multiple sim regions in diffferent areas of world space
    vector_3 CameraP = Subtract(World, &GameState->CameraPosition, &SimCenterP);
    
    //TODO: Move this into handmade_entity.cpp!
    //NOTE: SimEntities are zero indexed!
    
    SET_ACTIVE_ZLEVEL();
    
    for(uint32 EntityIndex = 0; 
        EntityIndex < SimRegion->EntityCount; 
        ++EntityIndex)
    {
        sim_entity * Entity = SimRegion->Entities + EntityIndex;
        
        if(Entity->Updatable)
        {
            //TODO: Culling of entities based on Z / camera view
            //low_entity * Storage = GameState->LowEntities + Entity->StorageIndex;
            
            real32 dtForFrame = Input->dtForFrame;
            //NOTE:This is incorrect, should be computed after update 
            real32 ShadowAlpha = (1.0f - 0.5f * (Entity->Position.z));
            if(ShadowAlpha < 0)
            {
                ShadowAlpha = 0;
            }
            else if(ShadowAlpha > 1)
            {
                ShadowAlpha = 1;
            }
            
            //NOTE: Propably we want to separate update call from renderer for entities sometime soon?.
            //NOTE < 0 : Below camera, 0 = CameraP , > 0 Above Camera
            vector_3 CameraRelativeGroundP = GetEntityGroundPoint(Entity) - CameraP;
            
            real32 FadeEndTop = 0.75f * GameState->TypicalFloorHeight;
            real32 FadeStartTop = 0.5f * GameState->TypicalFloorHeight;
            real32 FadeStartBottom = -2.0f * GameState->TypicalFloorHeight;
            real32 FadeEndBottom = -2.25f * GameState->TypicalFloorHeight;
            
            RenderGroup->GlobalAlpha  = 1.0f;
            if(CameraRelativeGroundP.z > FadeStartTop)
            {
                RenderGroup->GlobalAlpha = Clamp01FromRange(FadeEndTop, CameraRelativeGroundP.z, FadeStartTop);
            }
            else if (CameraRelativeGroundP.z < FadeStartBottom)
            {
                RenderGroup->GlobalAlpha = Clamp01FromRange(FadeEndBottom , CameraRelativeGroundP.z, FadeStartBottom);
            }
            
            move_spec MoveSpec = DefaultMoveSpec();
            vector_3 EntityAcceleration = {};
            //
            //NOTE: Pre-Physics simulation
            //
            switch(Entity->Type)
            {
                case EntityType_Player:
                {
                    for(uint32 ControlledHeroIndex = 0;
                        ControlledHeroIndex < ArrayCount(GameState->ControlledHeroes);
                        ++ControlledHeroIndex)					
                    {
                        controlled_hero * ControlledHero = GameState->ControlledHeroes + ControlledHeroIndex;
                        
                        if(ControlledHero->EntityIndex == Entity->StorageIndex)
                        {
                            if(ControlledHero->ZVelocity != 0.0f)
                            {
                                Entity->Velocity.z = ControlledHero->ZVelocity;	
                            }
                            
                            MoveSpec.IsAccelVectorUnary = true;
                            MoveSpec.Speed = 50.0f;
                            MoveSpec.Drag = 8.0f;
                            
                            //NOTE: Controller input acceleration
                            EntityAcceleration =  Vector3(ControlledHero->PlayerAcceleration,0.0f);
                            
                            //TODO: Now that we have some real usage examples, 
                            //lets solidify the positioning system
                            if(ControlledHero->SwordVelocity.x != 0.0f ||
                               ControlledHero->SwordVelocity.y != 0.0f)
                            {
                                sim_entity * Sword = Entity->SwordRef.SimEntity;
                                
                                if(Sword && IsSet(Sword, EntityFlag_Nonspatial))	
                                {
                                    Sword->DistanceLimit = 10;					
                                    MakeEntitySpatial(Sword, Entity->Position, 
                                                      Vector3(10.0f * ControlledHero->SwordVelocity, 0.0f));
                                    AddCollisionRule(GameState, Entity->StorageIndex,
                                                     Sword->StorageIndex, false);
                                    
                                    PlaySound(&GameState->AudioState, 
                                              GetRandomSoundFrom(TranState->Assets, 
                                                                 &GameState->EffectsEntropy, 
                                                                 Asset_Bloop));
                                }				
                            }					
                        }
                    }
                }break;		
                case EntityType_Wall:
                {
                    
                }break;
                case EntityType_Stairwell:
                {
                    
                }break;
                case EntityType_Sword:
                {			
                    MoveSpec.IsAccelVectorUnary = false;
                    MoveSpec.Speed = 0.0f;
                    MoveSpec.Drag = 0;
                    
                    //TODO: Add the ability in the collission routines to know a movement limit for an entiy.
                    //And then use that info to kill the sword.
                    //TODO: Need to handle the fact that distancetraveled might not
                    //have enough distance for the total entity move for the frame;
                    
                    if(Entity->DistanceLimit  <= 0.0f)
                    {
                        ClearCollisionRulesFor(GameState,Entity->StorageIndex);
                        MakeEntityNonSpatial(Entity);
                    }
                    
                }break;
                
                case EntityType_Familiar:
                {		
                    MoveSpec.IsAccelVectorUnary = true;
                    MoveSpec.Speed = 50.0f;
                    MoveSpec.Drag = 8.0f;
                    
                    sim_entity * PlayerEntity = 0;
                    real32 ClosestPlayerDistanceSq = Square(10.0f);
                    
                    if(DEBUG_SWITCH(FamiliarFollowsHero))
                    {
                        //TODO: Make spatial queries easy for things
                        for(uint32 EntityIndex = 0; EntityIndex < SimRegion->EntityCount; 
                            ++EntityIndex)
                        {
                            sim_entity * TestEntity = SimRegion->Entities + EntityIndex;
                            if(TestEntity->Type == EntityType_Player)
                            {
                                real32 TestDSq = LengthSq(TestEntity->Position - Entity->Position);
                                
                                if(ClosestPlayerDistanceSq > TestDSq)
                                {
                                    //NOTE: Means we are closer to the hero
                                    PlayerEntity = TestEntity;
                                    ClosestPlayerDistanceSq = TestDSq;
                                }
                            }
                        }
                    }
                    
                    if(PlayerEntity && (ClosestPlayerDistanceSq > Square(3.0f)))
                    {
                        real32 BaseAcceleration = 0.5f;
                        real32 OneOverLength = BaseAcceleration / SquareRoot(ClosestPlayerDistanceSq);
                        EntityAcceleration = OneOverLength * (PlayerEntity->Position - Entity->Position);	
                    }
                    
                }break;		
                case EntityType_Monster:
                {
                }break;
                case EntityType_Room:
                {
                }break;
                default:
                {
                    InvalidCodePath;
                }break;
            }
            
            //
            //NOTE: Physics Simulation
            //
            if(!IsSet(Entity, EntityFlag_Nonspatial) && 
               IsSet(Entity, EntityFlag_Movable))
            {
                MoveEntity(GameState, SimRegion, Entity, &MoveSpec, EntityAcceleration, dtForFrame);
            }
            
            RenderGroup->Transform.OffsetP = GetEntityGroundPoint(Entity);//Entity->Position;
            
            asset_vector MatchVector = {};
            MatchVector.E[Tag_FacingDirection] = Entity->FacingDirection;
            
            asset_vector WeightVector = {};
            WeightVector.E[Tag_FacingDirection] = 1.0f;
            
            hero_bitmap_ids HeroBitmapIDs = {};
            HeroBitmapIDs.Head = GetBestMatchBitmapFrom(TranState->Assets, Asset_Head, &MatchVector, &WeightVector);
            HeroBitmapIDs.Cape = GetBestMatchBitmapFrom(TranState->Assets, Asset_Cape, &MatchVector, &WeightVector);
            HeroBitmapIDs.Torso = GetBestMatchBitmapFrom(TranState->Assets, Asset_Torso, &MatchVector, &WeightVector);
            
            
            if(Entity->Type == EntityType_Player)
            {
                
#if 0                
                DEBUG_OPEN_DATA_BLOCK(Hero Bitmaps, 0, 0);
                
                DebugAddVariable(HeroBitmapIDs.Head, DebugFlag_VarData);
                DebugAddVariable(HeroBitmapIDs.Cape, DebugFlag_VarData);
                DebugAddVariable(HeroBitmapIDs.Torso, DebugFlag_VarData);
                
                DEBUG_CLOSE_DATA_BLOCK();
#endif
                
            }
            
            
            //
            //NOTE: Post-Physics simulation
            //
            
            switch(Entity->Type)
            {
                case EntityType_Player:
                {
                    //TODO: Z alpha
                    real32 HeroSizeC = 2.5f;
                    PushBitmap(RenderGroup, GetFirstBitmapFrom(TranState->Assets, Asset_Shadow), HeroSizeC * 1.2f, Vector3(0, 0, 0),
                               Vector4(1.0f,1.0f,1.0f,ShadowAlpha));
                    PushBitmap(RenderGroup, HeroBitmapIDs.Torso, HeroSizeC * 1.2f, Vector3(0,0,0));
                    PushBitmap(RenderGroup, HeroBitmapIDs.Cape, HeroSizeC * 1.2f, Vector3(0,0,0));
                    PushBitmap(RenderGroup, HeroBitmapIDs.Head,HeroSizeC * 1.2f, Vector3(0,0,0));
                    
                    DrawHitPoints(RenderGroup, Entity);
                    
                    if (DEBUG_SWITCH(ParticlesTest))
                    {
                        u32 NextIndex = 0;
                        
                        u32 ParticlesSpawnPerFrame = 3;
                        for(u32 ParticleIndex = 0;
                            ParticleIndex < ParticlesSpawnPerFrame;
                            ++ParticleIndex)
                        {
                            particle * Particle = GameState->Particles + GameState->NextParticle++;
                            
                            if(GameState->NextParticle >= ArrayCount(GameState->Particles))
                            {
                                GameState->NextParticle = 0;
                            }
                            
                            Particle->Position = Vector3(RandomBetween(&GameState->GeneralEntropy, -0.1f, 0.1f),0,0);
                            Particle->Velocity = Vector3(RandomBetween(&GameState->GeneralEntropy, -0.1f,0.1f), 
                                                         7.0f * RandomBetween(&GameState->GeneralEntropy, 1.0f, 1.2f), 0.0f);
                            Particle->Color = Vector4(RandomBetween(&GameState->GeneralEntropy, 0.7f, 1.0f),
                                                      RandomBetween(&GameState->GeneralEntropy, 0.7f, 1.0f),
                                                      RandomBetween(&GameState->GeneralEntropy, 0.7f, 1.0f), 
                                                      0.94f);
                            Particle->Acceleration = Vector3(0,-9.8f,0);
                            Particle->dColor = Vector4(0.0f,0.0f,0.0f, -0.1f);
                            
                            
                            asset_vector MatchVector = {};
                            MatchVector.E[Tag_FontType] = FontType_Default;
                            asset_vector WeightVector = {};
                            WeightVector.E[Tag_FontType] = 1.0f;
                            
                            font_id FontID = GetBestMatchFontFrom(TranState->Assets, Asset_Font, &MatchVector, &WeightVector);
                            
                            TranState->DefaultFont = PushFont(RenderGroup, FontID);
                            TranState->HHADefualtFont = GetFontInfo(TranState->Assets, FontID);
                            
                            if(TranState->DefaultFont)
                            {
                                char Nothings[] = "NOTHINGS";
                                
                                Particle->BitmapID = GetBitmapForGlyph(TranState->HHADefualtFont, 
                                                                       TranState->DefaultFont, 
                                                                       Nothings[NextIndex++ % ArrayCount(Nothings)]);
                                
                            }
                        }
                        
                        ZeroStruct(GameState->ParticleCells);
                        
                        //NOTE: Construct density data
                        r32 CellScale = 0.25f;
                        r32 InvCellScale = 1.0f / CellScale;
                        
                        vector_3 Origin = {-0.5f * CellScale * PARTICLE_CELL_COUNT,0,0};
                        for(u32 ParticleIndex = 0;
                            ParticleIndex < ArrayCount(GameState->Particles);
                            ++ParticleIndex)
                        {
                            particle * Particle = GameState->Particles + ParticleIndex;
                            
                            vector_3 P = InvCellScale * (Particle->Position - Origin);
                            
                            s32 X = TruncateReal32ToInt32(P.x);
                            s32 Y = TruncateReal32ToInt32(P.y);
                            
                            if(X < 0){X = 0;}
                            if(X > PARTICLE_CELL_COUNT - 1){X = PARTICLE_CELL_COUNT-1;}
                            if(Y < 0){Y = 0;}
                            if(Y > PARTICLE_CELL_COUNT - 1){Y = PARTICLE_CELL_COUNT-1;}
                            
                            particle_cell * ParticleCell = GameState->ParticleCells[Y] + X;
                            
                            r32 Density  = 1.0f;
                            ParticleCell->Density += Density;
                            ParticleCell->VelocityTimesDensity += Density * Particle->Velocity;
                        }
                        
                        if (DEBUG_SWITCH(ParticlesGrid))
                        {
                            vector_2 CellDim = CellScale * Vector2(1.0f,1.0f);
                            for(u32 Y = 0;
                                Y < ArrayCount(GameState->ParticleCells);
                                ++Y)
                            {
                                for(u32 X = 0;
                                    X < ArrayCount(GameState->ParticleCells[Y]);
                                    ++X)
                                {
                                    particle_cell * Cell = GameState->ParticleCells[Y] + X;
                                    
                                    vector_3 Offset = 0.5f * Vector3(CellDim.x,CellDim.y,0) + 
                                        CellScale * Vector3((r32)X, (r32)Y, 0.0f) + Origin;
                                    
                                    r32 Alpha = Clamp01(Cell->Density *0.1f);
                                    
                                    PushRect(RenderGroup, Offset, CellDim, 
                                             Vector4(Alpha, Alpha, Alpha, 1.0f));
                                }
                            }
                        }
                        
                        for(u32 ParticleIndex = 0;
                            ParticleIndex < ArrayCount(GameState->Particles);
                            ++ParticleIndex)
                        {
                            particle * Particle = GameState->Particles + ParticleIndex;
                            //NOTE: Simulation
                            vector_3 P = InvCellScale * (Particle->Position - Origin);
                            
                            s32 X = TruncateReal32ToInt32(P.x);
                            s32 Y = TruncateReal32ToInt32(P.y);
                            
                            if(X < 0){X = 0;}
                            if(X > PARTICLE_CELL_COUNT - 1){X = PARTICLE_CELL_COUNT - 1;}
                            if(Y < 0){Y = 0;}
                            if(Y > PARTICLE_CELL_COUNT - 1){Y = PARTICLE_CELL_COUNT - 1;}
                            //NOTE: Pick cell on which particle is.
                            particle_cell * CellCenter = &GameState->ParticleCells[Y][X];
                            particle_cell * CellLeft = &GameState->ParticleCells[Y][X - 1];
                            particle_cell * CellRight = &GameState->ParticleCells[Y][X + 1];
                            particle_cell * CellUp = &GameState->ParticleCells[Y + 1][X];
                            particle_cell * CellDown = &GameState->ParticleCells[Y - 1][X];
                            
                            
                            vector_3 Dispersion = {};
                            r32 CD = 1.0f;
                            Dispersion += CD * (CellCenter->Density - CellLeft->Density) * Vector3(-1.0f,0,0);
                            Dispersion += CD * (CellCenter->Density - CellRight->Density) * Vector3(1.0f,0,0);
                            Dispersion += CD * (CellCenter->Density - CellUp->Density) * Vector3(0.0f,1.0f,0);
                            Dispersion += CD * (CellCenter->Density - CellDown->Density) * Vector3(0,-1.0f,0);
                            
                            vector_3 Acceleration = Particle->Acceleration + Dispersion;
                            
                            Particle->Position += (0.5f * Square(dtForFrame) * Acceleration + 
                                                   dtForFrame * Particle->Velocity);
                            Particle->Velocity += Acceleration * dtForFrame;
                            
                            r32 CoefficientOfRestitution = 0.3f;
                            r32 CoefficientOfFriction = 0.7f;
                            if(Particle->Position.y < 0)
                            {
                                Particle->Position.y = -Particle->Position.y;
                                Particle->Velocity.y = -CoefficientOfRestitution * Particle->Velocity.y;
                                Particle->Velocity.x =  CoefficientOfFriction * Particle->Velocity.x;
                            }
                            
                            Particle->Color += (dtForFrame * Particle->dColor);
                            
                            vector_4 Color = {};
                            Color.r = Clamp01(Particle->Color.r);
                            Color.g = Clamp01(Particle->Color.g);
                            Color.b = Clamp01(Particle->Color.b);
                            Color.a = Clamp01(Particle->Color.a);
                            
                            if(Color.a > 0.9f)
                            {
                                Color.a = 0.9f * Clamp01FromRange(1.0f, Color.a, 0.9f);
                            }
                            
                            //NOTE: Rendering
                            PushBitmap(RenderGroup, Particle->BitmapID, 1.0f, Particle->Position, Color);
                        }
                    }
                    
                }break;		
                case EntityType_Wall:
                {
                    PushBitmap(RenderGroup, GetFirstBitmapFrom(TranState->Assets, Asset_Tree), 2.5f, Vector3(0,0,0));		
                }break;
                
                case EntityType_Stairwell:
                {
#if 0                    
                    PushRect(RenderGroup, Vector3(0, 0, 0),Entity->WalkableDimension, 
                             Vector4(1.0f,0.5f,0.0f,1.0f));
                    PushRect(RenderGroup, Vector3(0, 0, Entity->WalkableHeight), 
                             Entity->WalkableDimension, Vector4(1.0f,1.0f,0.0f,1.0f));
#else
                    PushBitmap(RenderGroup, GetFirstBitmapFrom(TranState->Assets, Asset_Stair), Entity->WalkableDimension.y, Vector3(0,0,0));
                    //TODO(Alex): Remove this, this is just a test
                    RenderGroup->Transform.OffsetP += Vector3(0,0, Entity->WalkableHeight);
                    PushBitmap(RenderGroup, GetFirstBitmapFrom(TranState->Assets, Asset_Stair), Entity->WalkableDimension.y, Vector3(0,0,0));
#endif
                    
                }break;	
                
                case EntityType_Sword:
                {			
                    PushBitmap(RenderGroup, GetFirstBitmapFrom(TranState->Assets, Asset_Shadow), 0.25f, Vector3(0, 0, 0), 
                               Vector4(1,1,1,ShadowAlpha));	
                    
                    PushBitmap(RenderGroup, GetFirstBitmapFrom(TranState->Assets, Asset_Sword), 0.5f, Vector3(0, 0, 0));
                }break;
                
                case EntityType_Familiar:
                {		
                    Entity->tBob += dtForFrame;
                    if(Entity->tBob >= Tau32)
                    {
                        Entity->tBob -= (Tau32);
                    }
                    
                    real32 BobSin = Sin(5.0f * Entity->tBob);
                    
                    real32 FamiliarSizeC = 2.5f;
                    PushBitmap(RenderGroup, GetFirstBitmapFrom(TranState->Assets, Asset_Shadow), FamiliarSizeC * 1.5f, Vector3(0, 0, 0), 
                               Vector4(1.0f,1.0f,1.0f,(0.5f * ShadowAlpha) + 0.2f * BobSin));
                    
                    PushBitmap(RenderGroup, HeroBitmapIDs.Head, FamiliarSizeC * 1.2f, Vector3(0, 0, 1.0f * BobSin), 
                               Vector4(1.0f,1.0f,1.0f,ShadowAlpha));
                }break;		
                case EntityType_Monster:
                {
                    real32 MonsterSizeC = 2.5f;
                    PushBitmap(RenderGroup, GetFirstBitmapFrom(TranState->Assets, Asset_Shadow), MonsterSizeC * 1.5f, Vector3(0, 0, 0),
                               Vector4(1.0f,1.0f,1.0f,ShadowAlpha));
                    PushBitmap(RenderGroup, HeroBitmapIDs.Cape, MonsterSizeC * 1.2f, Vector3(0, 0, 0),
                               Vector4(1.0f,1.0f,1.0f,ShadowAlpha));
                    
                    DrawHitPoints(RenderGroup, Entity);
                }break;
                case EntityType_Room:
                {
                    
                    if(DEBUG_SWITCH(UsedSpaceOutlines))
                    {
                        for(uint32 VolumeIndex = 0; 
                            VolumeIndex < Entity->Collision->VolumeCount; 
                            ++VolumeIndex)
                        {
                            collision_volume * Volume = Entity->Collision->Volumes + VolumeIndex;
                            PushRectOutline(RenderGroup, Volume->OffsetP - Vector3(0,0,0.5f * Volume->Dimension.z),
                                            Volume->Dimension.xy, Vector4(1.0f,0.0f,0.5f,1.0f));
                        }
                    }
                    
                }break;
                default:
                {
                    InvalidCodePath;
                }break;
            }
            
            //if(DEBUG_SWITCH(EntityCollisionVolumes))
            {
                debug_id DebugEntityID = GET_DEBUG_ID((GameState->LowEntities + Entity->StorageIndex), 0);
                
                for(u32 VolumeIndex = 0; 
                    VolumeIndex < Entity->Collision->VolumeCount; 
                    ++VolumeIndex)
                {
                    collision_volume * Volume = Entity->Collision->Volumes + VolumeIndex;
                    vector_4 EntityVolumeColor = {};
                    if(DEBUG_HIGHLIGHTED(DebugEntityID, &EntityVolumeColor))
                    {
                        PushRectOutline(RenderGroup, Volume->OffsetP, 
                                        Volume->Dimension.xy, 
                                        EntityVolumeColor, 0.05f);
                    }
                }
                
                if(DEBUG_PRINT_DATA(DebugEntityID))
                {
                    DEBUG_OPEN_DATA_BLOCK("sim_entity", DebugEntityID);
                    DebugAddVariable(Entity->Type);
                    
                    DebugAddVariable(Entity->Updatable);
                    DebugAddVariable(Entity->StorageIndex);
                    
                    //DEBUG_OPEN_DATA_BLOCK(Entity->Chunk);
                    
                    //DebugAddVariable(Entity->Chunk);
                    
                    DebugAddVariable(Entity->Flags);
                    DebugAddVariable(Entity->Position);
                    DebugAddVariable(Entity->Velocity);
                    
                    //DebugAddVariable(Entity->Collision);
                    
                    DebugAddVariable(Entity->FacingDirection);
                    DebugAddVariable(Entity->tBob);
                    DebugAddVariable(Entity->HitPointMax);
                    
                    //DEBUG_BEGIN_ARRAY(Entity->HitPoints[16]);
                    //DebugAddVariable(Entity->SwordRef);
                    DebugAddVariable(Entity->DistanceLimit);
                    DebugAddVariable(Entity->WalkableDimension);
                    DebugAddVariable(Entity->WalkableHeight);
                    
                    DEBUG_CLOSE_DATA_BLOCK();
                }
            }//End DEBUG_SWITCH()
        }//End if(Entity->Updatable)
    }//End for
    
    //NOTE: Clear GlobalAlpha value for next frame 
    RenderGroup->GlobalAlpha = 1.0f;
    
    
    
    //NOTE: Normal Reflection testing 
#if 0
    GameState->Time += Input->dtForFrame;
    real32 Angle = GameState->Time;
    
    vector_3 MapColor[] = 
    {
        {1,0,0},
        {0,1,0},
        {0,0,1}
    };
    
    int32 CheckerWidth = 16;
    int32 CheckerHeight = 16;
    bool32 RowCheckerOn = false;
    for(uint32 MapIndex = 0; 
        MapIndex < ArrayCount(TranState->EnvMaps); 
        ++MapIndex)
    {
        environment_map * Map = TranState->EnvMaps + MapIndex;
        loaded_bitmap * LOD = Map->LOD;
        
        for(uint32 Y = 0; 
            Y < TranState->EnvMapHeight;
            Y+=CheckerHeight)
        {
            bool32 CheckerOn = RowCheckerOn;
            for(uint32 X = 0; 
                X < TranState->EnvMapWidth;
                X+=CheckerWidth)
            {
                vector_4 Color = CheckerOn ? Vector4(MapColor[MapIndex], 1.0f) : Vector4(0,0,0,1.0f); 
                
                vector_2 MinP = Vector2i(X,Y);
                vector_2 MaxP = MinP + Vector2i(CheckerWidth, CheckerHeight);
                
                DrawRectangle(LOD, MinP , MaxP, Color, 
                              RectMinMax((s32)MinP.x , (s32)MinP.y, (s32)MaxP.x, (s32)MaxP.y), false); 
                
                CheckerOn = !CheckerOn;
            }
            
            RowCheckerOn = !RowCheckerOn;
        }
    }
    
    //DrawBitmap(&TranState->EnvMaps[0].LOD[0], &TranState->GroundBuffer[52].Bitmap, 100.0f, 50.0f, 1.0f);
    TranState->EnvMaps[0].Pz = -1.5f;
    TranState->EnvMaps[1].Pz = 0.0f;
    TranState->EnvMaps[2].Pz = 1.5f;
    
#if 0
    vector_2 Disp = {100.0f * Cos(0.5f * Angle),
        100.0f * Sin(0.1f * Angle)};
#else
    vector_2 Disp = {};
#endif
    vector_2 Origin = ScreenCenter;
#if 1
    vector_2 XAxis = 200.0f * Vector2(Cos(Angle), Sin(Angle));
    vector_2 YAxis =  Perp(XAxis);
#else
    vector_2 XAxis = {100,0};
    vector_2 YAxis = {0,100};
#endif
    real32 CAngle = 10.0f * Angle;
#if 0
    vector_4 Color =  Vector4(0.5f + 0.5f * Sin(CAngle), 
                              0.5f + 0.5f * Cos(2.2f * CAngle),
                              0.5f + 0.5f * Sin(9.9f * CAngle),
                              0.5f + 0.5f * Cos(5.5f * CAngle));
#else
    vector_4 Color = Vector4(1.0f,1.0f,1.0f,1.0f);
#endif
    
    PushCoordinateSystem(RenderGroup, Disp + Origin - 0.5f * XAxis - 0.5f * YAxis,
                         XAxis, YAxis, Color,
                         &GameState->TestDiffuse,
                         &GameState->TestNormal,
                         TranState->EnvMaps + 2,
                         TranState->EnvMaps + 1,
                         TranState->EnvMaps + 0);
    
    vector_2 MapP = {0.0f, 0.0f};
    
    for(uint32 MapIndex = 0; 
        MapIndex < ArrayCount(TranState->EnvMaps); 
        ++MapIndex)
    {
        environment_map * Map = TranState->EnvMaps + MapIndex;
        loaded_bitmap * LOD = Map->LOD;
        
        vector_2 XAxis = {0.5f * (real32)LOD->Width,0};
        vector_2 YAxis = {0, 0.5f * (real32)LOD->Height};
        PushCoordinateSystem(RenderGroup, MapP, XAxis, YAxis, Vector4(1.0f,1.0f,1.0f,1.0f), LOD,0,0,0,0);
        
        MapP += YAxis + Vector2(0.0f, 6.0f);
    }
    
#endif
    
    TiledRenderGroupToOutput(TranState->HighPriorityQueue, RenderGroup, DrawBuffer);
    EndRenderGroup(RenderGroup);
    
    //TODO: Add logic to the sim region to handle unplaced entities
    //TODO: Move the update of the camera to a place where the we can know of its update before it gets rendered,
    //so there isnt a frame of lag for the camera updating compared to hero
    EndSim(SimRegion, GameState);
    EndTempMemory(SimMemory);
    EndTempMemory(RenderMemory);
    
    //NOTE: Checks that each stack frame of tempMemory begins and end Memory usage == 0;
    CheckArena(&GameState->WorldArena);
    CheckArena(&TranState->TranArena);
    
    //EvictAssetsAsNeeded(TranState->Assets);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state * GameState = (game_state*)Memory->PermanentStorage;
    transient_state * TranState = (transient_state*)Memory->TransientStorage;
    
    OutputPlayingSounds(&GameState->AudioState, SoundBuffer, TranState->Assets, &TranState->TranArena);
}


#if HANDMADE_INTERNAL
#include "handmade_debug.cpp"
#else
extern "C" GAME_DEBUG_FRAME_END(GameDEBUGFrameEnd)
{
    return (0);
}
#endif























