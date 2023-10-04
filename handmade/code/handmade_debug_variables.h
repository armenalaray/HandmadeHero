#ifndef HANDMADE_DEBUG_VARIABLES_H


//TODO(Alex): Parameterize the fountain?
//TODO(Alex): Add distance to the debug camera as a float example 

//NOTE(Alex): We want to transform this piece of data to a more flexible construct that lets us 
//add debug variables into a hierarchical manner.   

//NOTE(Alex): The adding system has to work on any context with any system, 
//it adds the variable just once, in multiple scnenarios 
#if 0

struct var_stack
{
    u32 DepthLevel;
    debug_variable * Layer[MAX_STACK_SIZE];
};

struct debug_context_info 
{
    debug_state * DebugState;
    memory_arena * Arena;
    var_stack Stack;
};


internal debug_variable * 
DEBUGAddUnlinkedVariable(memory_arena * Arena, char * Name)
{
    debug_variable * Var = PushStruct(Arena, debug_variable); 
    Var->Name = PushString(Arena, Name);
    return Var;
}


internal debug_variable_link *  
DEBUGAddVariableToGroup(memory_arena * Arena, debug_variable * Parent, debug_variable * Var)
{
    debug_variable_link * Link = PushStruct(Arena, debug_variable_link);
    Link->Var = Var;
    if(Parent)
    {
        DLIST_ADD_TAIL(&Parent->Sentinel, Link);
    }
    return Link;
}

internal debug_variable *  
DEBUGAddVariable(memory_arena * Arena, debug_variable * Parent,  char * Name)
{
    debug_variable * Var = DEBUGAddUnlinkedVariable(Arena, Name);
    debug_variable_link * Link = DEBUGAddVariableToGroup(Arena, Parent,  Var);
    return Var;
}

internal debug_variable *  
DEBUGAddVariable(debug_context_info * Context, char * Name)
{
    Assert(Context->Stack.DepthLevel >= 0);
    debug_variable * Parent = Context->Stack.Layer[Context->Stack.DepthLevel];
    
    debug_variable * Var = DEBUGAddVariable(Context->Arena, Parent, Name);
    return Var;
}


internal debug_variable *
DEBUGBeginGroup(memory_arena * Arena, debug_variable * Parent, char * Name)
{
    debug_variable * Group = DEBUGAddVariable(Arena, Parent, Name);
    Group->Type = DebugVariableType_Group;
    DLIST_INIT(&Group->Sentinel);
    return Group;
}

internal debug_variable *
DEBUGBeginGroup(debug_context_info * Context, char * Name)
{
    debug_variable * Group = DEBUGAddVariable(Context, Name);
    Group->Type = DebugVariableType_Group;
    DLIST_INIT(&Group->Sentinel);
    //NOTE(Alex): Here we are asserting that we are capable of adding to the last level
    Assert(Context->Stack.DepthLevel < (ArrayCount(Context->Stack.Layer) - 1));
    Context->Stack.Layer[++Context->Stack.DepthLevel] = Group;
    
    return Group;
}


internal void
DEBUGEndGroup(debug_context_info * Context)
{
    Assert(Context->Stack.DepthLevel);
    --Context->Stack.DepthLevel;
}

internal debug_variable * 
DEBUGAddVariable(debug_context_info * Context, 
                 char * Name, 
                 debug_variable_type Type, 
                 b32 B32)
{
    debug_variable * Var = DEBUGAddVariable(Context, Name);
    Var->Type = Type;
    switch(Var->Type)
    {
        case DebugVariableType_Event:
        {
            Var->Event.B32 = B32;
        }break;
    }
    return Var;
}

internal debug_variable * 
DEBUGAddVariable(debug_context_info * Context,
                 char * Name,
                 debug_variable_type VarType, 
                 asset_type_id AssetTypeID)
{
    debug_variable * Var = DEBUGAddVariable(Context, Name);
    Var->Type = VarType;
    
    bitmap_id ID = GetFirstBitmapFrom(Context->DebugState->Assets, AssetTypeID);
    hha_bitmap * HHA = GetBitmapInfo(Context->DebugState->Assets, ID);
    
    Var->BitmapDisplay.ID = ID; 
    Var->BitmapDisplay.WidthOverHeight = (r32)HHA->Dim[0] / (r32)HHA->Dim[1];
    Var->BitmapDisplay.Alpha = 1.0f;
    
    return Var;
}


#define ADD_DEBUG_VARIABLE(Name, Type)\
DEBUGAddVariable(Context, #Name, DebugVariableType_##Type, DEBUGUI_##Name)

internal void 
DEBUGCreateVariables(debug_context_info * Context)
{
    
}

#endif

#define HANDMADE_DEBUG_VARIABLES_H
#endif


#if 0        
debug_context_info Context_ = {};
debug_context_info * Context = &Context_;

Context->DebugState = DebugState;
Context->Arena = &DebugState->DebugArena;
Context->Stack.DepthLevel = 0;
Context->Stack.Layer[0] = 0;

debug_variable * RootVar = DEBUGBeginGroup(Context, "Root");
{
    DEBUGBeginGroup(Context, "Debugger");
    {
        DEBUGBeginGroup(Context, "Render");
        {
            ADD_DEBUG_VARIABLE(SampleEnvironmentMap, b32);
            
            DEBUGBeginGroup(Context, "Camera");
            {
                ADD_DEBUG_VARIABLE(EnableDebugCamera, b32);
                ADD_DEBUG_VARIABLE(UseRoomBasedCamera, b32);
                ADD_DEBUG_VARIABLE(DebugCameraDistance, r32);
            }
            DEBUGEndGroup(Context);
        }
        DEBUGEndGroup(Context);
        
        
        DEBUGBeginGroup(Context, "GroundChunks");
        {
            ADD_DEBUG_VARIABLE(GroundChunkOutlines, b32);
            ADD_DEBUG_VARIABLE(GroundChunkCheckerBoard, b32);
            ADD_DEBUG_VARIABLE(GroundChunkReloading, b32);
        }
        DEBUGEndGroup(Context);
        
        DEBUGBeginGroup(Context, "Particles");
        {
            ADD_DEBUG_VARIABLE(ParticlesTest, b32);
            ADD_DEBUG_VARIABLE(ParticlesGrid, b32);
        }
        DEBUGEndGroup(Context);
        
        DEBUGBeginGroup(Context, "EntitySystem");
        {
            ADD_DEBUG_VARIABLE(UsedSpaceOutlines, b32);
            ADD_DEBUG_VARIABLE(EntityCollisionVolumes, b32);
            ADD_DEBUG_VARIABLE(FamiliarFollowsHero, b32);
        }
        DEBUGEndGroup(Context);
        
        DEBUGBeginGroup(Context, "Profiler");
        {
            DEBUGBeginGroup(Context, "By Thread");
            {
                DEBUGAddVariable(Context, "", DebugVariableType_CounterThreadList);
            }
            DEBUGEndGroup(Context);
            DEBUGBeginGroup(Context, "By Function");
            {
                DEBUGAddVariable(Context, "", DebugVariableType_CounterThreadList);
            }
            DEBUGEndGroup(Context);
        }
        DEBUGEndGroup(Context);
        
        DEBUGBeginGroup(Context, "BitmapDisplay");
        {
            DEBUGAddVariable(Context, "", DebugVariableType_BitmapDisplay, Asset_Head);
        }
        DEBUGEndGroup(Context);
    }
    DEBUGEndGroup(Context);
}
DEBUGEndGroup(Context);

Assert(Context->Stack.DepthLevel == 0);

#endif
