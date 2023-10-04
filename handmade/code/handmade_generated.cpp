member_definition DefinitionOf_hit_point [] =
{
{MetaType_uint8,    "Flags", (memory_index)&(((hit_point *)(0))->Flags), sizeof(((hit_point *)0)->Flags), 1},
{MetaType_uint8,    "FilledAmount", (memory_index)&(((hit_point *)(0))->FilledAmount), sizeof(((hit_point *)0)->FilledAmount), 1},
};
member_definition DefinitionOf_collision_volume_group [] =
{
{MetaType_collision_volume,    "TotalVolume", (memory_index)&(((collision_volume_group *)(0))->TotalVolume), sizeof(((collision_volume_group *)0)->TotalVolume), 1},
{MetaType_uint32,    "VolumeCount", (memory_index)&(((collision_volume_group *)(0))->VolumeCount), sizeof(((collision_volume_group *)0)->VolumeCount), 1},
{MetaType_collision_volumePtr, "Volumes", (memory_index)&(((collision_volume_group *)(0))->Volumes), sizeof(((collision_volume_group *)0)->Volumes), 1},
};
member_definition DefinitionOf_sim_entity [] =
{
{MetaType_entity_type,    "Type", (memory_index)&(((sim_entity *)(0))->Type), sizeof(((sim_entity *)0)->Type), 1},
{MetaType_b32,    "Updatable", (memory_index)&(((sim_entity *)(0))->Updatable), sizeof(((sim_entity *)0)->Updatable), 1},
{MetaType_u32,    "StorageIndex", (memory_index)&(((sim_entity *)(0))->StorageIndex), sizeof(((sim_entity *)0)->StorageIndex), 1},
{MetaType_world_chunkPtr, "Chunk", (memory_index)&(((sim_entity *)(0))->Chunk), sizeof(((sim_entity *)0)->Chunk), 1},
{MetaType_u32,    "Flags", (memory_index)&(((sim_entity *)(0))->Flags), sizeof(((sim_entity *)0)->Flags), 1},
{MetaType_vector_3,    "Position", (memory_index)&(((sim_entity *)(0))->Position), sizeof(((sim_entity *)0)->Position), 1},
{MetaType_vector_3,    "Velocity", (memory_index)&(((sim_entity *)(0))->Velocity), sizeof(((sim_entity *)0)->Velocity), 1},
{MetaType_collision_volume_groupPtr, "Collision", (memory_index)&(((sim_entity *)(0))->Collision), sizeof(((sim_entity *)0)->Collision), 1},
{MetaType_r32,    "FacingDirection", (memory_index)&(((sim_entity *)(0))->FacingDirection), sizeof(((sim_entity *)0)->FacingDirection), 1},
{MetaType_r32,    "tBob", (memory_index)&(((sim_entity *)(0))->tBob), sizeof(((sim_entity *)0)->tBob), 1},
{MetaType_u32,    "HitPointMax", (memory_index)&(((sim_entity *)(0))->HitPointMax), sizeof(((sim_entity *)0)->HitPointMax), 1},
{MetaType_hit_point,    "HitPoints", (memory_index)&(((sim_entity *)(0))->HitPoints), sizeof(((sim_entity *)0)->HitPoints), 16},
{MetaType_entity_reference,    "SwordRef", (memory_index)&(((sim_entity *)(0))->SwordRef), sizeof(((sim_entity *)0)->SwordRef), 1},
{MetaType_r32,    "DistanceLimit", (memory_index)&(((sim_entity *)(0))->DistanceLimit), sizeof(((sim_entity *)0)->DistanceLimit), 1},
{MetaType_vector_2,    "WalkableDimension", (memory_index)&(((sim_entity *)(0))->WalkableDimension), sizeof(((sim_entity *)0)->WalkableDimension), 1},
{MetaType_r32,    "WalkableHeight", (memory_index)&(((sim_entity *)(0))->WalkableHeight), sizeof(((sim_entity *)0)->WalkableHeight), 1},
};
member_definition DefinitionOf_rectangle_3 [] =
{
{MetaType_vector_3,    "Min", (memory_index)&(((rectangle_3 *)(0))->Min), sizeof(((rectangle_3 *)0)->Min), 1},
{MetaType_vector_3,    "Max", (memory_index)&(((rectangle_3 *)(0))->Max), sizeof(((rectangle_3 *)0)->Max), 1},
};
#define DEBUG_SWITCH_MEMBER_DEFS(DepthLevel)\
case MetaType_collision_volume_group:{u32 BytesWritten = _snprintf_s(At, BufferSize, BufferSize, "%s:", Def->VarName); DEBUGTextLine(TextBuffer); At += BytesWritten; DumpStructDataOut(ArrayCount(DefinitionOf_collision_volume_group), DefinitionOf_collision_volume_group, MemberOffset, DepthLevel);}break;case MetaType_collision_volume_groupPtr:{u32 BytesWritten = _snprintf_s(At, BufferSize, BufferSize, "%s:", Def->VarName); DEBUGTextLine(TextBuffer); At += BytesWritten; DumpStructDataOut(ArrayCount(DefinitionOf_collision_volume_group), DefinitionOf_collision_volume_group, (void*)(*(u64*)MemberOffset), DepthLevel);}break;case MetaType_hit_point:{u32 BytesWritten = _snprintf_s(At, BufferSize, BufferSize, "%s:", Def->VarName); DEBUGTextLine(TextBuffer); At += BytesWritten; DumpStructDataOut(ArrayCount(DefinitionOf_hit_point), DefinitionOf_hit_point, MemberOffset, DepthLevel);}break;case MetaType_hit_pointPtr:{u32 BytesWritten = _snprintf_s(At, BufferSize, BufferSize, "%s:", Def->VarName); DEBUGTextLine(TextBuffer); At += BytesWritten; DumpStructDataOut(ArrayCount(DefinitionOf_hit_point), DefinitionOf_hit_point, (void*)(*(u64*)MemberOffset), DepthLevel);}break;case MetaType_rectangle_3:{u32 BytesWritten = _snprintf_s(At, BufferSize, BufferSize, "%s:", Def->VarName); DEBUGTextLine(TextBuffer); At += BytesWritten; DumpStructDataOut(ArrayCount(DefinitionOf_rectangle_3), DefinitionOf_rectangle_3, MemberOffset, DepthLevel);}break;case MetaType_rectangle_3Ptr:{u32 BytesWritten = _snprintf_s(At, BufferSize, BufferSize, "%s:", Def->VarName); DEBUGTextLine(TextBuffer); At += BytesWritten; DumpStructDataOut(ArrayCount(DefinitionOf_rectangle_3), DefinitionOf_rectangle_3, (void*)(*(u64*)MemberOffset), DepthLevel);}break;case MetaType_sim_entity:{u32 BytesWritten = _snprintf_s(At, BufferSize, BufferSize, "%s:", Def->VarName); DEBUGTextLine(TextBuffer); At += BytesWritten; DumpStructDataOut(ArrayCount(DefinitionOf_sim_entity), DefinitionOf_sim_entity, MemberOffset, DepthLevel);}break;case MetaType_sim_entityPtr:{u32 BytesWritten = _snprintf_s(At, BufferSize, BufferSize, "%s:", Def->VarName); DEBUGTextLine(TextBuffer); At += BytesWritten; DumpStructDataOut(ArrayCount(DefinitionOf_sim_entity), DefinitionOf_sim_entity, (void*)(*(u64*)MemberOffset), DepthLevel);}break;