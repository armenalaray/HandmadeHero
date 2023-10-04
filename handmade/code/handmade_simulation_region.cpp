#include "handmade_simulation_region.h"


//NOTE: we need to know somehow which sim_entity is mapped to a storage index
internal sim_entity_hash *  
GetHashFromStorageIndex(sim_region * SimRegion, uint32 StorageIndex)
{
    TIMED_FUNCTION();
    Assert(StorageIndex);
    
    sim_entity_hash * Result = 0;
    
    uint32 HashValue = StorageIndex;
    
    for(uint32 Offset = 0; Offset < ArrayCount(SimRegion->Hash); ++Offset)			
    {
        uint32 HashMask = ((ArrayCount(SimRegion->Hash) - 1));
        uint32 HashIndex = ((HashValue + Offset) & HashMask);
        sim_entity_hash * Entry = SimRegion->Hash + HashIndex;
        
        if(Entry->StorageIndex == 0 || Entry->StorageIndex == StorageIndex)
        {
            Result = Entry;
            break;
        }
    }
    return Result;
}


inline sim_entity * 
GetEntityByStorageIndex(sim_region * SimRegion, uint32 StorageIndex)
{
    sim_entity_hash * SimEntityHash = GetHashFromStorageIndex(SimRegion, StorageIndex);
    sim_entity * Result = SimEntityHash->SimEntity;	
    return Result;
}


inline vector_3 
GetSimSpacePosition(sim_region * SimRegion, low_entity * StoredEntity)
{
    //NOTE: Map the entity into camera space
    //TODO: we could use NAN to assert that nobody uses the position of a non-spatial entity
    vector_3 Result = InvalidP;
    if(!IsSet(&StoredEntity->SimEntity, EntityFlag_Nonspatial))
    {
        Result = Subtract(SimRegion->World, &StoredEntity->Position, &SimRegion->Origin);
    }
    return Result;
}


internal sim_entity *  
AddEntity(game_state * GameState, sim_region * SimRegion, uint32 StorageIndex, low_entity * Source, 
          vector_3 * SimPosition);

inline void 
LoadEntityReference(game_state * GameState, sim_region * SimRegion, entity_reference * Ref)
{
    if(Ref->StorageIndex)
    {
        sim_entity_hash * SimEntityHash = GetHashFromStorageIndex(SimRegion, Ref->StorageIndex);
        if(SimEntityHash->SimEntity == 0)
        {
            SimEntityHash->StorageIndex = Ref->StorageIndex;
            low_entity * StoredEntity = GetLowEntity(GameState, Ref->StorageIndex);
            
            vector_3 Pos = GetSimSpacePosition(SimRegion, StoredEntity);
            
            SimEntityHash->SimEntity = AddEntity(GameState, SimRegion, Ref->StorageIndex, StoredEntity, &Pos);
        }
        Ref->SimEntity = SimEntityHash->SimEntity;	
    }
}



inline void 
StoreEntityReference(entity_reference * Ref)
{
    if(Ref->SimEntity)
    {
        Ref->StorageIndex = Ref->SimEntity->StorageIndex;
    }
}

internal sim_entity *  
AddEntityRaw(game_state * GameState, sim_region * SimRegion, uint32 StorageIndex, low_entity * Source)
{
    Assert(StorageIndex);
    
    sim_entity_hash * SimEntityHash = GetHashFromStorageIndex(SimRegion, StorageIndex);
    if(SimEntityHash->SimEntity == 0)
    {
        if(SimRegion->EntityCount < SimRegion->MaxEntityCount)
        {
            sim_entity * SimEntity = SimRegion->Entities + SimRegion->EntityCount++;
            
            SimEntityHash->SimEntity = SimEntity;
            SimEntityHash->StorageIndex = StorageIndex;
            
            if(Source)
            {
                //NOTE_ This really has to be a decompression step, not a copy
                *SimEntity = Source->SimEntity;
                LoadEntityReference(GameState, SimRegion, &SimEntity->SwordRef);
                
                Assert(!IsSet(&Source->SimEntity, EntityFlag_Simming));
                AddFlags(&Source->SimEntity, EntityFlag_Simming);
                
            }
            SimEntity->StorageIndex = StorageIndex;	
            SimEntity->Updatable = false;
        }
        else
        {
            InvalidCodePath;
        }
    }
    return SimEntityHash->SimEntity;
}



internal bool32
EntityOverlapsRectangle(rectangle_3 Rectangle,collision_volume Volume , vector_3 Position)
{
    rectangle_3 GrownRectangle = AddRadiusTo(Rectangle,0.5f *  Volume.Dimension); 
    
    bool32 Result = IsInRectangle(GrownRectangle, Position + Volume.OffsetP);
    
    return Result;
}


internal sim_entity *
AddEntity(game_state * GameState, sim_region * SimRegion, 
          uint32 StorageIndex, low_entity * Source , 
          vector_3 * SimPosition)
{
    sim_entity * DestEntity = AddEntityRaw(GameState, SimRegion, StorageIndex, Source);
    if(DestEntity)
    {
        if(SimPosition)
        {
            DestEntity->Position = *SimPosition;
            DestEntity->Updatable = EntityOverlapsRectangle(SimRegion->UpdatableBounds, DestEntity->Collision->TotalVolume, DestEntity->Position);
        }
        else
        {
            DestEntity->Position = GetSimSpacePosition(SimRegion, Source);
        }
    }
    return DestEntity;
}


internal sim_region *  
BeginSim(game_state * GameState, memory_arena * SimArena, 
         world * World,  world_position Origin, 
         rectangle_3 Bounds, real32 dtForFrame)
{
    TIMED_FUNCTION();
    
    //TODO: If entities where stored in the world the game_state wouldnt be needed here		
    sim_region * SimRegion = PushStruct(SimArena, sim_region);
    ZeroStruct(SimRegion->Hash);
    
    //TODO: Calculate this eventually from the maximum value of all entities 
    //radius plus their speed
    //TODO: Perhaps try using a dual system here, where we support entities 
    //larger than max entity radius, including them multiple times into the spatial partition  
    SimRegion->MaxEntityRadius = 5.0f;
    SimRegion->MaxEntitySpeed = 30.0f;
    
    real32 UpdateSafetyMargin = SimRegion->MaxEntityRadius + dtForFrame * SimRegion->MaxEntitySpeed;
    
    SimRegion->World = World;
    SimRegion->Origin = Origin;
    
    SimRegion->UpdatableBounds = AddRadiusTo(Bounds, Vector3(SimRegion->MaxEntityRadius, SimRegion->MaxEntityRadius, 0));
    SimRegion->Bounds = AddRadiusTo(Bounds, Vector3(UpdateSafetyMargin, UpdateSafetyMargin, UpdateSafetyMargin));
    
    
    //TODO: Need to be more specific about entity counts
    SimRegion->MaxEntityCount = 4096;
    SimRegion->EntityCount = 0;
    SimRegion->Entities = PushArray(SimArena, SimRegion->MaxEntityCount, sim_entity);
    
    world_position MinChunkPosition = MapIntoChunkSpace(World, SimRegion->Origin, GetMin(SimRegion->Bounds));
    world_position MaxChunkPosition = MapIntoChunkSpace(World, SimRegion->Origin, GetMax(SimRegion->Bounds));
    
    for(int32 ChunkZ = MinChunkPosition.ChunkZ; 
        ChunkZ <= MaxChunkPosition.ChunkZ; ++ChunkZ)
    {
        for(int32 ChunkY = MinChunkPosition.ChunkY; 
            ChunkY <= MaxChunkPosition.ChunkY; ++ChunkY)
        {
            for(int32 ChunkX = MinChunkPosition.ChunkX; 
                ChunkX <= MaxChunkPosition.ChunkX; ++ChunkX)
            {
                world_chunk * Chunk = 
                    GetWorldChunk(World, ChunkX,  ChunkY, ChunkZ);
                if(Chunk)
                {
                    for(world_entity_block * Block = &Chunk->FirstBlock; 
                        Block; Block = Block->NextBlock)
                    {		
                        for(uint32 EntityIndex = 0; 
                            EntityIndex < Block->EntityCount; ++EntityIndex)	
                        {		
                            low_entity * StoredEntity = GameState->LowEntities + Block->LowEntityIndex[EntityIndex];
                            
                            vector_3 CameraRelativePosition = GetSimSpacePosition(SimRegion, StoredEntity);
                            
                            if(!IsSet(&StoredEntity->SimEntity, EntityFlag_Nonspatial))
                            {
                                if(EntityOverlapsRectangle(SimRegion->Bounds, StoredEntity->SimEntity.Collision->TotalVolume, CameraRelativePosition))
                                {
                                    //TODO: Check against a second rectangle to see if an entity is movable
                                    AddEntity(GameState, SimRegion, Block->LowEntityIndex[EntityIndex], StoredEntity, &CameraRelativePosition);
                                }	
                            }
                        }
                    }
                }
            }
        }
    }
    
    return SimRegion;
}


internal void 
EndSim(sim_region * SimRegion, game_state * GameState)
{
    TIMED_FUNCTION();
    
    world * World = SimRegion->World;
    sim_entity * SimEntity = SimRegion->Entities;
    
    for(uint32 EntityIndex = 0; 
        EntityIndex < SimRegion->EntityCount; 
        ++EntityIndex, ++SimEntity)
    {
        low_entity * StoredEntity = GameState->LowEntities + SimEntity->StorageIndex;
        
        Assert(IsSet(&StoredEntity->SimEntity, EntityFlag_Simming));
        StoredEntity->SimEntity = *SimEntity;
        Assert(!IsSet(&StoredEntity->SimEntity, EntityFlag_Simming));
        
        StoreEntityReference(&StoredEntity->SimEntity.SwordRef);
        //TODO: Save state back to stored entity, once high entities do state decompression
        
        world_position NewPosition = (IsSet(SimEntity, EntityFlag_Nonspatial)) ? 
            NullPosition() : MapIntoChunkSpace(SimRegion->World, SimRegion->Origin, SimEntity->Position);
        
        ChangeEntityLocation(&GameState->WorldArena, SimRegion->World , SimEntity->StorageIndex, StoredEntity, NewPosition);
        
        if(SimEntity->StorageIndex == GameState->CameraFollowingEntityIndex)
        {
            //TODO: Fix this to work on camera space
            world_position NewCameraPosition = GameState->CameraPosition;
            
            if(DEBUG_SWITCH(UseRoomBasedCamera))
            {
                NewCameraPosition.ChunkZ = StoredEntity->Position.ChunkZ;	
                if(SimEntity->Position.x > (13.0f))
                {
                    NewCameraPosition = MapIntoChunkSpace(SimRegion->World, NewCameraPosition, Vector3(22,0,0));
                }
                if(SimEntity->Position.x < -(13.0f))
                {
                    NewCameraPosition = MapIntoChunkSpace(SimRegion->World, NewCameraPosition, Vector3(-22,0,0));
                }
                if(SimEntity->Position.y > (10.0f))
                {
                    NewCameraPosition = MapIntoChunkSpace(SimRegion->World, NewCameraPosition, Vector3(0,12,0));
                }
                if(SimEntity->Position.y < -(10.0f))
                {
                    NewCameraPosition = MapIntoChunkSpace(SimRegion->World, NewCameraPosition, Vector3(0,-12,0));
                }
            }
            else
            {
                //real32 CameraOffsetZ = NewCameraPosition.Offset_.z;
                NewCameraPosition = StoredEntity->Position;
                //NewCameraPosition.Offset_.z = CameraOffsetZ;
            }
            
            GameState->CameraPosition = NewCameraPosition;
        }	
        
    }
}


internal bool32
CanCollide(game_state * GameState, sim_entity * A, sim_entity * B)
{
    TIMED_FUNCTION();
    
    //TODO: Look into a rules table to see if we should collide
    bool32 Result = false;
    if(A != B)
    {
        if(A->StorageIndex > B->StorageIndex)
        {
            sim_entity * Temp = A;
            A = B;
            B = Temp;
        }
        
        if(IsSet(A, EntityFlag_Collides) 
           && IsSet(B, EntityFlag_Collides))
        {
            if((!IsSet(A, EntityFlag_Nonspatial)) 
               && !IsSet(B, EntityFlag_Nonspatial))
            {
                //TODO: Property based logic goes here
                Result = true;
            }
            
            //TODO: Better hash function
            uint32 HashBucket = A->StorageIndex & (ArrayCount(GameState->CollisionRuleHash) - 1);
            
            for(pairwise_collision_rule * CollisionRule = GameState->CollisionRuleHash[HashBucket]; 
                CollisionRule; 
                CollisionRule = CollisionRule->NextInHash)
            {
                if((CollisionRule->StorageIndexA == A->StorageIndex)
                   && (CollisionRule->StorageIndexB == B->StorageIndex))
                {
                    Result = CollisionRule->CanCollide;
                    break;
                }
            }
        }
    }
    
    return Result;
}

internal bool32 
CollisionResponse(game_state * GameState, sim_entity * A, sim_entity * B)
{
    bool32 StopsOnCollision = false;
    //TODO: should this be compared against storageIndex instead of type?
    if(A->Type == EntityType_Sword)
    {
        AddCollisionRule(GameState, A->StorageIndex, B->StorageIndex, false);
        StopsOnCollision = false;
    }
    else 
    {
        StopsOnCollision = true;
    }
    
    if(A->Type > B->Type)
    {
        sim_entity * Temp = A;
        A = B;
        B = Temp;
    }
    
    if((A->Type == EntityType_Monster) && (B->Type == EntityType_Sword))
    {
        //TODO: Make real StopsOnCollision 
        if(A->HitPointMax > 0)
        {
            --A->HitPointMax;	
        }
        
    }
    
    //NOTE: Update camera/ player z movement
    //TODO: Stairs
    //SimEntity->AbsTileZ = HitLowEntity->AbsTileZ;
    
    return StopsOnCollision;
    
}

internal bool32
CanOverlap(game_state * GameState, sim_entity * Mover,sim_entity *  Region)
{
    bool32 Result = false;
    
    if(Mover != Region)
    {
        if(Region->Type == EntityType_Stairwell)
        {
            Result = true;
        }
    }
    return Result;
    
}

internal void 
HandleOverlap(game_state * GameState, sim_entity * Mover,sim_entity *  Region, real32 dt, real32 *Ground)
{
    if(Region->Type == EntityType_Stairwell)
    {
        *Ground = GetStairGround(Region, GetEntityGroundPoint(Mover));
    }
}


internal bool32 
SpeculativeCollision(sim_entity * Mover,sim_entity *  Region, vector_3 TestPosition)
{
    bool32 Result = true;
    if(Region->Type == EntityType_Stairwell)
    {
        real32 StepHeight = 0.1f; 
        //TODO: Needs work :)
#if 0
        Result = ((AbsValue(GetEntityGroundPoint(Mover).Z - Ground) > StepHeight) || 
                  (BarycentricPos.Y > 0.1f && BarycentricPos.Y < 0.9f));
#endif
        vector_3 MoverGroundPoint = GetEntityGroundPoint(Mover, TestPosition);
        real32 Ground = GetStairGround(Region, MoverGroundPoint);
        Result = (AbsValue(MoverGroundPoint.z - Ground) > StepHeight);
        
    }
    return Result;
}

#if 0
internal b32
PointIsInEntity(sim_entity * Entity, vector_2 P)
{
    b32 Result = false;
    
    return Result;
}
#endif

internal bool32
EntitiesOverlap(sim_entity * SimEntity, sim_entity * TestEntity, vector_3 Epsilon = Vector3(0,0,0))
{
    TIMED_FUNCTION();
    bool32 Result = false;
    
    for(uint32 EntityVolumeIndex = 0; !Result && (EntityVolumeIndex < SimEntity->Collision->VolumeCount); ++EntityVolumeIndex)
    {
        collision_volume * EntityVolume = SimEntity->Collision->Volumes + EntityVolumeIndex;
        
        for(uint32 TestVolumeIndex = 0;  !Result && (TestVolumeIndex < TestEntity->Collision->VolumeCount); ++TestVolumeIndex)
        {
            collision_volume * TestVolume = TestEntity->Collision->Volumes + TestVolumeIndex;
            
            rectangle_3 EntityRectangle = RectCenterDim(SimEntity->Position + EntityVolume->OffsetP, EntityVolume->Dimension + Epsilon);
            rectangle_3 TestRectangle = RectCenterDim(TestEntity->Position + TestVolume->OffsetP, TestVolume->Dimension);
            
            Result = RectanglesOverlap(EntityRectangle, TestRectangle);
        }
    }
    
    
    return Result;
    
}

internal void 
MoveEntity(game_state * GameState, sim_region * SimRegion, sim_entity * SimEntity, move_spec * MoveSpec, vector_3 PlayerAcceleration, real32 dtForFrame)
{	
    TIMED_FUNCTION();
    
    Assert(!IsSet(SimEntity, EntityFlag_Nonspatial));
    world * World = SimRegion->World;
    
    
    if(MoveSpec->IsAccelVectorUnary)
    {		
        real32 AccelerationMagnitudeSquared = LengthSq(PlayerAcceleration);
        if(AccelerationMagnitudeSquared > 1.0f)
        {
            //Normalize
            PlayerAcceleration *= (1.0f / SquareRoot(AccelerationMagnitudeSquared));
        }	
    }
    
    PlayerAcceleration *= MoveSpec->Speed;
    vector_3 Drag = -MoveSpec->Drag * SimEntity->Velocity ;
    Drag.z = 0.0f;
    
    PlayerAcceleration += Drag;
    
    if(!IsSet(SimEntity, EntityFlag_ZGrounded))
    {
        PlayerAcceleration +=  Vector3(0.0f,0.0f,-9.8f);
    }
    
    vector_3 PlayerDelta = (0.5f * PlayerAcceleration * Square(dtForFrame)) + (dtForFrame * SimEntity->Velocity);
    SimEntity->Velocity = (PlayerAcceleration * dtForFrame )+ SimEntity->Velocity;
    
    Assert(LengthSq(SimEntity->Velocity) <= Square(SimRegion->MaxEntitySpeed));
    real32 DistanceRemaining = SimEntity->DistanceLimit;
    
    if(DistanceRemaining == 0.0f)
    {
        //TODO: Do we want to formalize this number
        DistanceRemaining = 10000.0f;
    }
    
    for(uint32 Iteration = 0; (Iteration < 4); ++Iteration)	
    {			
        real32 tMin = 1.0f;
        real32 tMax = 0.0f;
        
        real32 PlayerDeltaLength = Length(PlayerDelta); 
        
        //TODO: What do we want to do with epsilon here?
        //Think this through for the final collission code
        if(PlayerDeltaLength > 0.0f)
        {
            if(PlayerDeltaLength > DistanceRemaining)
            {
                tMin = (DistanceRemaining / PlayerDeltaLength);
            }
            
            vector_3 DesiredPosition = SimEntity->Position + PlayerDelta;
            
            vector_3 WallNormalMax = {};
            vector_3 WallNormalMin = {};
            
            sim_entity * HitEntityMax = 0;
            sim_entity * HitEntityMin = 0;
            
            
            if(!IsSet(SimEntity, EntityFlag_Nonspatial))
            {
                //TODO: Spatial partition here
                for(uint32 SimEntityIndex = 0; SimEntityIndex < SimRegion->EntityCount; ++SimEntityIndex)
                {
                    sim_entity * TestEntity = SimRegion->Entities + SimEntityIndex;
                    
                    real32 OverlapEpsilon = 0.001f;
                    if((IsSet(TestEntity, EntityFlag_Traversable) && 
                        (EntitiesOverlap(SimEntity, TestEntity, OverlapEpsilon * Vector3(1,1,1))))
                       || (CanCollide(GameState, SimEntity, TestEntity)))
                    {
                        for(uint32 EntityVolumeIndex = 0; 
                            EntityVolumeIndex < SimEntity->Collision->VolumeCount;
                            ++EntityVolumeIndex)
                        {
                            collision_volume * EntityVolume = SimEntity->Collision->Volumes + EntityVolumeIndex;
                            
                            for(uint32 TestVolumeIndex = 0; 
                                TestVolumeIndex < TestEntity->Collision->VolumeCount;
                                ++TestVolumeIndex)
                            {
                                collision_volume * TestVolume = TestEntity->Collision->Volumes + TestVolumeIndex;
                                
                                //NOTE: Simple Minkowski Sum
                                vector_3 MinkowskiDiameter = {TestVolume->Dimension.x + EntityVolume->Dimension.x, 
                                    TestVolume->Dimension.y + EntityVolume->Dimension.y, 
                                    TestVolume->Dimension.z + EntityVolume->Dimension.z};
                                
                                vector_3 MinCorner = -0.5f * MinkowskiDiameter;
                                vector_3 MaxCorner = 0.5f * MinkowskiDiameter;
                                
                                
                                //NOTE: RelOldPlayerPosition Relative to center of tile
                                vector_3 RelOldEntityPosition = 
                                    (SimEntity->Position + EntityVolume->OffsetP) - 
                                    (TestEntity->Position + TestVolume->OffsetP);
                                
                                //TODO: Do we want an open inclusion at the max corner?
                                if(RelOldEntityPosition.z >= MinCorner.z && 
                                   RelOldEntityPosition.z < MaxCorner.z )
                                {
                                    
                                    bool32 HitThis = false;
                                    
                                    test_wall Wall[] = 
                                    {
                                        {MinCorner.x, RelOldEntityPosition.x, 
                                            RelOldEntityPosition.y, PlayerDelta.x, 
                                            PlayerDelta.y, MinCorner.y ,
                                            MaxCorner.y, Vector3(-1.0f, 0.0f ,0.0f)},
                                        {MaxCorner.x, RelOldEntityPosition.x, 
                                            RelOldEntityPosition.y, PlayerDelta.x, 
                                            PlayerDelta.y, MinCorner.y , 
                                            MaxCorner.y,Vector3(1.0f,0.0f, 0.0f)},
                                        {MinCorner.y, RelOldEntityPosition.y, 
                                            RelOldEntityPosition.x, PlayerDelta.y, 
                                            PlayerDelta.x, MinCorner.x , 
                                            MaxCorner.x,Vector3(0.0f,-1.0f, 0.0f)},
                                        {MaxCorner.y, RelOldEntityPosition.y,
                                            RelOldEntityPosition.x, PlayerDelta.y,
                                            PlayerDelta.x, MinCorner.x , 
                                            MaxCorner.x,Vector3(0.0f,1.0f, 0.0f)},
                                    };
                                    
                                    if(IsSet(TestEntity, EntityFlag_Traversable))
                                    {
                                        real32 TesttMax = tMax;
                                        vector_3 TestWallNormal = {};
                                        for(uint32 WallIndex = 0; 
                                            WallIndex < ArrayCount(Wall); 
                                            ++WallIndex)
                                        {
                                            test_wall * TestWall = Wall + WallIndex;
                                            
                                            real32 tEpsilon = 0.001f;
                                            if(TestWall->DeltaX != 0.0f)
                                            {
                                                real32 tResult =(TestWall->X - TestWall->RelX) / TestWall->DeltaX;
                                                real32 Y = TestWall->RelY + tResult * TestWall->DeltaY;
                                                
                                                if((tResult >= 0.0f) && (TesttMax < tResult))
                                                {
                                                    //Check if t is smaller than testT
                                                    if((Y >= TestWall->MinY) && (Y <= TestWall->MaxY))
                                                    {
                                                        TesttMax = Maximum(0.0f, tResult - tEpsilon) ;
                                                        TestWallNormal = TestWall->Normal;
                                                        HitThis = true;
                                                    }
                                                }					
                                            }
                                            
                                        }
                                        
                                        if(HitThis)
                                        {
                                            WallNormalMax = TestWallNormal;  
                                            tMax = TesttMax;  
                                            HitEntityMax = TestEntity;
                                        }
                                        
                                    }
                                    else
                                    {
                                        real32 TesttMin = tMin;
                                        vector_3 TestWallNormal = {};
                                        for(uint32 WallIndex = 0; WallIndex < ArrayCount(Wall); ++WallIndex)
                                        {
                                            test_wall * TestWall = Wall + WallIndex;
                                            
                                            real32 tEpsilon = 0.001f;
                                            if(TestWall->DeltaX != 0.0f)
                                            {
                                                real32 tResult = (TestWall->X - TestWall->RelX) / TestWall->DeltaX;
                                                real32 Y = TestWall->RelY + tResult * TestWall->DeltaY;
                                                
                                                if((tResult >= 0.0f) && (TesttMin > tResult))
                                                {
                                                    //Check if t is smaller than testT
                                                    if((Y >= TestWall->MinY) && (Y <= TestWall->MaxY))
                                                    {
                                                        TesttMin = Maximum(0.0f , tResult - tEpsilon);
                                                        TestWallNormal = TestWall->Normal;
                                                        HitThis = true;
                                                    }
                                                }	
                                            }
                                        }
                                        
                                        //NOTE: We need a concept of stepping on and stepping off stairs 
                                        //so we can prevent from leaving stairs instead
                                        //just preventing you from gettin onto them 
                                        if(HitThis)
                                        {
                                            vector_3 TestPosition = SimEntity->Position + TesttMin * PlayerDelta;
                                            
                                            if(SpeculativeCollision(SimEntity, TestEntity, TestPosition))
                                            {
                                                WallNormalMin = TestWallNormal;  
                                                tMin = TesttMin;  
                                                HitEntityMin = TestEntity;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            //NOTE: we take the minimum of both t's to let entity advanze   
            vector_3 WallNormal = {};
            sim_entity * HitEntity = 0;
            real32 tStop = 0.0f;
            
            if(tMin < tMax)
            {
                WallNormal = WallNormalMin;
                HitEntity = HitEntityMin;
                tStop = tMin;
            }
            else
            {
                WallNormal = WallNormalMax;
                HitEntity = HitEntityMax;
                tStop = tMax;
            }
            
            SimEntity->Position += tStop * PlayerDelta;
            DistanceRemaining -= tStop * PlayerDeltaLength;
            
            if(HitEntity)
            {
                PlayerDelta = DesiredPosition - SimEntity->Position;
                
                bool32 StopsOnCollision = CollisionResponse(GameState, SimEntity, HitEntity);
                
                if(StopsOnCollision)
                {
                    SimEntity->Velocity = SimEntity->Velocity - (1 * (InnerProduct(SimEntity->Velocity, WallNormal)  *WallNormal));
                    PlayerDelta = PlayerDelta - (1 * (InnerProduct(PlayerDelta, WallNormal) * WallNormal));
                }
                
            }
            else
            {
                break;
            }
            
        }
        else
        {
            break;
        }
        
    }
    
    real32 Ground = 0.0f;
    
    //TODO: Handle multi-volumes here
    //NOTE: Handle events base on area overlapping
    //TODO: Handle overlapping precisely moving it into the collision loop?
    {
        //TODO: Spatial partition here
        for(uint32 SimEntityIndex = 0; SimEntityIndex < SimRegion->EntityCount; ++SimEntityIndex)
        {
            sim_entity * TestEntity = SimRegion->Entities + SimEntityIndex;
            
            if(CanOverlap(GameState, SimEntity, TestEntity) && EntitiesOverlap(SimEntity, TestEntity))
            {
                //NOTE: Here we handle overlapping per entity, do we need to handle overlapping per volume?
                //NOTE: we asuume here that the player is simentity and TestEntity is 
                HandleOverlap(GameState, SimEntity, TestEntity, dtForFrame, &Ground);
            }
        }
    }
    
    Ground += SimEntity->Position.z - GetEntityGroundPoint(SimEntity).z;
    
    if((SimEntity->Position.z <= Ground) || 
       (IsSet(SimEntity, EntityFlag_ZGrounded) && 
        (SimEntity->Velocity.z == 0)))
    {
        SimEntity->Position.z = Ground;
        SimEntity->Velocity.z = 0;
        AddFlags(SimEntity,EntityFlag_ZGrounded);
    }
    else 
    {
        ClearFlags(SimEntity,EntityFlag_ZGrounded);
    }
    
    if(SimEntity->DistanceLimit != 0.0f)
    {
        //TODO: Do we want to formalize this number
        SimEntity->DistanceLimit = DistanceRemaining;
    }
    
    //TODO: change to use the acceleration vector
    if((SimEntity->Velocity.x == 0) && (SimEntity->Velocity.y == 0))
    {
        //NOTE: Dont set Facing Direction
    }
    else 
    {
        SimEntity->FacingDirection = ATan2(SimEntity->Velocity.y, SimEntity->Velocity.x);
        //NOTE: Normalize Angle to 0 - Tau32 
        
#if 0
        if(SimEntity->FacingDirection < 0.0f)
        {
            SimEntity->FacingDirection += Tau32;
        }
#endif
        
    }
}