#include "handmade_entity.h"


inline move_spec 
DefaultMoveSpec()
{
    move_spec Result = {};
    
    Result.IsAccelVectorUnary = false;
    Result.Speed = 1.0f;
    Result.Drag = 0.0f;
    
    return Result;
}

