#ifndef HANDMADE_ENTITY_H


#define InvalidP Vector3(10000.0f,10000.0f,10000.0f)

inline bool32 
IsSet(sim_entity * SimEntity, uint32 Flag)
{
    bool32 Result = SimEntity->Flags & Flag;
    return Result;
}

inline void
AddFlags(sim_entity * SimEntity, uint32 Flags)
{
    SimEntity->Flags |= Flags;
}

inline void 
ClearFlags(sim_entity * SimEntity, uint32 Flags)
{
    SimEntity->Flags &= ~Flags;
}


inline void 
MakeEntityNonSpatial(sim_entity * Entity)
{
    Entity->Position = InvalidP;
    AddFlags(Entity, EntityFlag_Nonspatial);	
}


inline void 
MakeEntitySpatial(sim_entity * Entity, vector_3 Position, vector_3 Velocity)
{
    ClearFlags(Entity, EntityFlag_Nonspatial);
    Entity->Position = Position;
    Entity->Velocity = Velocity;
}


inline vector_3
GetEntityGroundPoint(sim_entity * Entity, vector_3 TestPosition)
{
    vector_3 Result = TestPosition;
    return Result;
}


inline vector_3
GetEntityGroundPoint(sim_entity * Entity)
{
    vector_3 Result = GetEntityGroundPoint(Entity, Entity->Position);
    return Result;
}

inline real32 
GetStairGround(sim_entity * Region, vector_3 AtGroundPoint)
{
    Assert(Region->Type == EntityType_Stairwell);
    rectangle_2 RegionRect = RectCenterDim(Region->Position.xy, Region->WalkableDimension);
    
    vector_2 BarycentricPos = Clamp01(GetBarycentricPosition(RegionRect, AtGroundPoint.xy));
    real32 Ground = Region->Position.z + BarycentricPos.y * Region->WalkableHeight;
    
    return Ground;
}




#define HANDMADE_ENTITY_H
#endif