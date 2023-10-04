#ifndef HANDMADE_SIMULATION_REGION_H

#define HIT_POINT_SUB_COUNT 4

struct move_spec
{
    bool32 IsAccelVectorUnary;
    real32 Speed;
    real32 Drag;
};

Introspection(category:"hit_point") 
struct hit_point
{
    //TODO: Bake this down into one variable
    uint8 Flags;	
    uint8 FilledAmount;
};

enum entity_type
{
    EntityType_Null,
    EntityType_Room,
    EntityType_Player,
    EntityType_Wall,
    EntityType_Familiar,
    EntityType_Monster,
    EntityType_Sword,
    EntityType_Stairwell
};


struct sim_entity;
union entity_reference 
{
    sim_entity * SimEntity;
    uint32 StorageIndex;
};

enum sim_entity_flags
{
    //TODO: Shall these be a flag for NonColliding entites?
    //NOTE: Collides and ZGrounded are probably not usable soon 
    EntityFlag_Collides = (1 << 0),
    EntityFlag_Nonspatial = (1 << 1),
    EntityFlag_Movable = (1 << 2),
    EntityFlag_ZGrounded = (1 << 3),
    EntityFlag_Traversable = (1 << 4),
    EntityFlag_Simming = (1 << 30)
};

struct collision_volume
{
    vector_3 OffsetP;
    vector_3 Dimension;
};

Introspection(category:collision_volume_group) 
struct collision_volume_group
{
    collision_volume TotalVolume;
    //TODO: Volume count is always expected to be greater than 0 if the entity has any volume... \
Later we can compress this to denote when Count = 0, TotalVolume represents the single volume for \
an entity.
    uint32 VolumeCount;
    collision_volume * Volumes;
};


Introspection(category:"Bread Butter") 
struct sim_entity
{
    //NOTE: These are only for the simregion
    entity_type Type;
    
    b32 Updatable;
    u32 StorageIndex;
    world_chunk * Chunk;
    
    u32 Flags;
    vector_3 Position;
    vector_3 Velocity;
    
    collision_volume_group * Collision;
    
    r32 FacingDirection;
    r32 tBob;
    //TODO: Should hitpoints be entities?
    //NOTE: Max number of points
    u32 HitPointMax;
    hit_point HitPoints[16];
    
    entity_reference SwordRef;
    r32 DistanceLimit;
    
    //NOTE: This is only for stairwells
    vector_2 WalkableDimension;
    r32 WalkableHeight;
    
    //TODO: Generation index so we know how up to date this entity is
};

struct sim_entity_hash
{
    sim_entity * SimEntity;
    uint32 StorageIndex;
};

struct sim_region
{
    real32 MaxEntityRadius;
    real32 MaxEntitySpeed;
    
    world * World;
    world_position Origin;
    
    rectangle_3 Bounds;
    rectangle_3 UpdatableBounds;
    
    uint32 MaxEntityCount;
    uint32 EntityCount;
    sim_entity * Entities;
    //NOTE: This has to be a power of two
    sim_entity_hash Hash[4096];
};


struct test_wall
{
    real32 X; 
    real32 RelX;
    real32 RelY; 
    real32 DeltaX;
    real32 DeltaY; 
    real32 MinY; 
    real32 MaxY;
    vector_3 Normal;
};

struct hot_entity_info
{
    sim_entity Entity;
    b32 IsValid;
};


#define HANDMADE_SIMULATION_REGION_H
#endif
