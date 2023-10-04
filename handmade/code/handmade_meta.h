#ifndef HANDMADE_META_H

#if 0
enum member_definition_type
{
    MetaType_uint8,
    MetaType_b32,
    MetaType_bool32 = MetaType_b32,
    MetaType_r32,
    MetaType_real32 = MetaType_r32,
    MetaType_s32,
    MetaType_int32 = MetaType_s32,
    
    MetaType_u32,
    MetaType_uint32 = MetaType_u32,
    MetaType_vector_2,
    MetaType_vector_3,
    
    MetaType_entity_type,
    MetaType_world_chunk,
    MetaType_collision_volume_group,
    MetaType_hit_point,
    MetaType_entity_reference,
    MetaType_game_assets,
    MetaType_render_transform,
    MetaType_world,
    MetaType_world_position,
    MetaType_sim_entity,
    MetaType_sim_entity_hash,
    MetaType_rectangle_3,
    MetaType_sim_region,
    MetaType_render_group,
    MetaType_world_entity_block,
    MetaType_collision_volume,
    
    MetaType_collision_volumePtr,
    MetaType_world_chunkPtr,
    MetaType_collision_volume_groupPtr,
    MetaType_hit_pointPtr,
    MetaType_sim_entityPtr,
    MetaType_rectangle_3Ptr,
};
#endif

#include "handmade_meta_type_generated.h"
META_MEMBER_TYPE_ENUM();

struct member_definition
{
    enum member_definition_type Type;
    char * VarName;
    
    memory_index Offset;
    u32 Size;
    u32 Count;
};


#define HANDMADE_META_H
#endif

