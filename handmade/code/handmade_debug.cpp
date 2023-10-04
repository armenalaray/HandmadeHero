//TODO: Remove stdio.h
#include "stdio.h"
#include "handmade_debug.h"
#include "handmade_debug_variables.h"

inline debug_frame * 
GetRelCollatedFrame(debug_state * DebugState, s32 RelValue)
{
    s32 RelIndex = (s32)DebugState->FrameIndex + RelValue;
    Assert(AbsValue((r32)RelIndex) <= MAX_FRAME_COUNT);
    
    u32 Index = (MAX_FRAME_COUNT + RelIndex) % MAX_FRAME_COUNT;
    debug_frame * Result = DebugState->Frames + Index;
    return Result;
}

inline debug_frame *
GetLastCollatedFrame(debug_state * DebugState)
{
    debug_frame * Result = GetRelCollatedFrame(DebugState, -1);
    return Result;
}

inline debug_id
GetDebugID(void * ptr1, void * ptr2)
{
    debug_id ID = {};
    ID.Value[0] = ptr1;
    ID.Value[1] = ptr2;
    return ID;
}

inline b32 
InteractionIsValid(debug_interaction * Interaction)
{
    b32 Result = (Interaction->Type !=  DebugInteraction_None);
    return Result;
}


inline b32 
DebugIDsAreEqual(debug_id A, debug_id B)
{
    b32 Result = ((A.Value[0] == B.Value[0]) && 
                  (A.Value[1] == B.Value[1]));
    return Result;
}


internal debug_view *  
DEBUGGetViewFor(debug_state * DebugState, debug_id ID, b32 ShouldAdd = true)
{
    debug_view * Result = 0;
    u32 HashIndex = (u32)((13 * (memory_index)ID.Value[0] + 17 *(memory_index)ID.Value[1]) 
                          & (ArrayCount(DebugState->ViewHash) - 1));
    
    debug_view ** HashSlot = DebugState->ViewHash + HashIndex; 
    
    for(debug_view * View = *HashSlot;
        View;
        View = View->NextInHash)
    {
        if(DebugIDsAreEqual(View->ID, ID))
        {
            Result = View;
            break;
        }
        
    }
    
    if(!Result && ShouldAdd)
    {
        Result = PushStruct(&DebugState->DebugArena, debug_view);
        Result->ID = ID;
        Result->NextInHash = *HashSlot;
        *HashSlot = Result;
    }
    
    return Result;
}

inline debug_state * 
GetDebugState(game_memory * Memory)
{
    debug_state * Result = 0;
    if(Memory)
    {
        Result = (debug_state*)Memory->DebugStorage;
        if(!Result->IsInitialized)
        {
            Result = 0;
        }
    }
    return Result;
}

inline debug_state * 
GetDebugState(void)
{
    debug_state * Result = GetDebugState(GlobalDebugMemory);
    return Result;
}

inline debug_interaction 
DebugDIDSelectInteraction(debug_id ID)
{
    debug_interaction Result = {};
    Result.Type = DebugInteraction_Select;
    Result.ID = ID;
    Result.Generic = 0;
    
    return Result;
}


/*
    NOTE(Alex): There should be a synchronization between the highlighted part and the actual 
selectable elements, so we could only highlight the ones on the same layer as us, 
*/



inline b32
SelectedID(debug_id ID)
{
    b32 Result = false;
    debug_state * DebugState = GetDebugState();
    if(DebugState)
    {
        for(u32 Index = 0;
            Index < DebugState->SelectedIDCount;
            ++Index)
        {
            if(DebugIDsAreEqual(ID, DebugState->SelectedIDs[Index]))
            {
                Result = true;
            }
        }
    }
    
    return Result;
}

internal void
ClearSelection()
{
    debug_state * DebugState = GetDebugState();
    if(DebugState)
    {
        DebugState->SelectedIDCount = 0;
    }
}

internal void
AddToSelection(debug_id ID)
{
    debug_state * DebugState = GetDebugState();
    if(DebugState)
    {
        if(!SelectedID(ID) && 
           DebugState->SelectedIDCount < ArrayCount(DebugState->SelectedIDs))
        {
            DebugState->SelectedIDs[DebugState->SelectedIDCount++] = ID;
        }
    }
}

internal void 
DEBUG_TEST_HIT(debug_id ID, r32 ZLevel)
{
    debug_state * DebugState = GetDebugState();
    if(DebugState)
    {
        if(ZLevel >= GlobalDebugTable->ZLevel)
        {
            GlobalDebugTable->ZLevel = ZLevel;
            DebugState->NextHotInteraction = DebugDIDSelectInteraction(ID);
        }
    }
}

internal b32
DEBUG_HIGHLIGHTED(debug_id ID, vector_4 * Color)
{
    b32 Result = false;
    debug_state * DebugState = GetDebugState();
    if(DebugState)
    {
        if(SelectedID((ID)))
        {
            *Color = Vector4(2.0f / 255.0f, 219.0f / 255.0f, 52.0f / 255.0f, 1.0f);
            Result = true;
        }
        if(DebugIDsAreEqual((ID), DebugState->NextHotInteraction.ID))
        {
            *Color = Vector4(1,1,1,1);
            Result = true;
        }
    }
    
    return Result;
}

internal b32 
DEBUG_PRINT_DATA(debug_id ID)
{
    debug_state * DebugState = GetDebugState();
    b32 Result = false;
    if(DebugState)
    {
        Result = (SelectedID(ID) || DebugIDsAreEqual(ID, DebugState->NextHotInteraction.ID));
    }
    return Result;
}


#define INVALID_DEBUG_VARIABLE_INDEX ArrayCount(DebugVariableList)

inline b32 
IsHex(char Char)
{
    b32 Result = ((Char >= '0' && Char <= '9') || (Char >= 'A' && Char <= 'F'));
    return Result;
}


inline u32
GetHex(u32 Value)
{
    // NOTE(Alex): 65 base A - F  48 base 0 - 9 
    u32 HexValue = 0;
    u32 Result = 0;
    
    for(u32 Index = 0; 
        Index < 4;
        ++Index)
    {
        HexValue = Value >> (Index * 8);
        HexValue &= 0xFF;
        
        Assert(IsHex((char)HexValue));
        
        if(HexValue >= '0' && HexValue <= '9')
        {
            HexValue = HexValue - 48;
        }
        else
        {
            HexValue = HexValue - 65 + 10;
        }
        
        Result |= (HexValue << (Index * 4));
    }
    
    return Result;
}


internal rectangle_2 
DoTextOp(debug_state * DebugState, debug_text_operation Op, vector_2 P, char * String, 
         vector_4 Color = Vector4(1.0f,1.0f,1.0f,1.0f))
{
    rectangle_2 Result = InvertedInfinityRectangle2();
    
    if(DebugState && DebugState->Font)
    {
        render_group * RenderGroup = DebugState->RenderGroup;
        loaded_font * Font = DebugState->Font;
        hha_font * HHA = DebugState->HHA;
        
        r32 CharScale = DebugState->FontScale;
        
        u32 PrevCodePoint = 0;
        r32 PrevCharScale = 0;
        r32 Kerning = 0;
        
        r32 XPos = P.x;
        r32 YPos = P.y;
        
        r32 TextWidth = 0;
        
        for(char * At = String;
            *At;
            ++At)
        {
            
            if(At[0] == '#' && 
               At[1] == '\\' && 
               At[2] >= '0' && 
               At[2] <= '9' &&
               At[3] >= '0' &&
               At[3] <= '9' &&
               At[4] >= '0' &&
               At[4] <= '9')
            {
                r32 Range = 1.0f / 9.0f;
                Color = Vector4(Clamp01((r32)(At[2] - '0') * Range), 
                                Clamp01((r32)(At[3] - '0') * Range), 
                                Clamp01((r32)(At[4] - '0') * Range), 
                                1.0f);
                At += 5;
            }
            
            if(At[0] == '^' && 
               At[1] == '\\' && 
               At[2] != 0)
            {
                r32 Range = 1.0f / 9.0f;
                CharScale = DebugState->FontScale * Clamp01((r32)(At[2] - '0') * Range);
                At += 3;
            }
            
            if(*At)
            {
                u32 CodePoint = *At;
                
                if(At[0] == '#' && 
                   IsHex(At[1]) && 
                   IsHex(At[2]) && 
                   IsHex(At[3]) && 
                   IsHex(At[4]))
                {
                    CodePoint = GetHex((At[1] << 24) | 
                                       (At[2] << 16) | 
                                       (At[3] << 8) | 
                                       (At[4] << 0));
                    At += 4;
                }
                
                Kerning = PrevCharScale * GetKerningForTwoCodePoints(HHA, Font, PrevCodePoint, CodePoint);
                XPos += Kerning;
                
                if(CodePoint != '\n' && CodePoint != ' ')
                {
                    bitmap_id BitmapID = GetBitmapForGlyph(HHA, Font, CodePoint);
                    hha_bitmap * Info = GetBitmapInfo(RenderGroup->Assets, BitmapID);
                    
                    vector_3 Offset = Vector3(XPos, YPos, 0);
                    r32 Height = Info->Dim[1] * CharScale;
                    
                    if(Op == DebugTextOperation_DrawText)
                    {
                        PushBitmap(RenderGroup, BitmapID, Height, Offset, Color);
                    }
                    else
                    {
                        Assert(Op == DebugTextOperation_SizeText);
                        loaded_bitmap * Bitmap = GetBitmap(RenderGroup->Assets, BitmapID, RenderGroup->GenerationIndex);
                        if(Bitmap)
                        {
                            used_bitmap_size_info Info = GetUsedBitmapSizeInfo(RenderGroup, Bitmap, Height, Offset);
                            
                            rectangle_2 CharRect = RectMinDim(Info.Position.xy, Info.Size);
                            Result = Union(CharRect, Result);
                        }
                    }
                }
                
                PrevCodePoint = CodePoint;
                PrevCharScale = CharScale;
            }
        }//Endfor
    }
    
    return Result;
}


internal void 
DrawTextAt(vector_2 P, char * String, vector_4 Color = Vector4(1.0f, 1.0f, 1.0f, 1.0f))
{
    debug_state * DebugState = GetDebugState();
    if(DebugState)
    {
        Assert(DebugState->RenderGroup->InsideRenderer);
        DoTextOp(DebugState, DebugTextOperation_DrawText, P, String, Color);
    }
}

internal rectangle_2 
DEBUGGetTextSize(char * String)
{
    rectangle_2 Result = InvertedInfinityRectangle2();
    debug_state * DebugState = GetDebugState();
    if(DebugState)
    {
        Assert(DebugState->RenderGroup->InsideRenderer);
        Result = DoTextOp(DebugState, DebugTextOperation_SizeText, Vector2(0,0), String);
    }
    return Result;
}


internal void 
DEBUGTextLine(char * String, vector_4 Color = Vector4(1.0f,1.0f,1.0f,1.0f))
{
    debug_state * DebugState = GetDebugState();
    if(DebugState)
    {
        DebugState->AtY -= GetLineAdvanceFor(DebugState->HHA) * DebugState->FontScale;
        DrawTextAt(Vector2(DebugState->LeftEdge, DebugState->AtY), String, Color);
    }
}



struct debug_stats
{
    r64 Min;
    r64 Max;
    
    r64 Avg;
    u32 Count;
};


inline void
BeginStats(debug_stats * Stats)
{
    Stats->Min = Real32Max;
    Stats->Max = -Real32Max;
    Stats->Avg = 0.0f;
    Stats->Count = 0;
}

inline void
AccumulateStats(debug_stats * Stats, r32 Value)
{
    Stats->Count += 1;
    
    r64 DValue = (r64)Value;
    
    if(Stats->Min > DValue)
    {
        Stats->Min = DValue;
    }
    
    if(Stats->Max < DValue)
    {
        Stats->Max = DValue;
    }
    
    Stats->Avg += (r64)Value;
}

inline void
EndStats(debug_stats * Stats)
{
    if(Stats->Count)
    {
        Stats->Avg /=  (r64)Stats->Count;
    }
    else
    {
        Stats->Min = 0;
        Stats->Max = 0;
    }
}

#define HANDMADE_CONFIG_FILE "../code/handmade_config.h"
internal memory_index
DEBUGEventToText(debug_event * Event, char * Buffer, char * End, u32 Flags)
{
    char * At = Buffer; 
    if(Flags & DebugVariableToText_AddDebugUI)
    {
        if(Event->Type == DebugType_OpenDataBlock)
        {
            At += _snprintf_s(At, (memory_index)(End - At), (memory_index)(End - At),  
                              "// ");
        }
        else
        {
            At += _snprintf_s(At, (memory_index)(End - At), (memory_index)(End - At),  
                              "#define DEBUGUI_");
        }
    }
    
    if(Flags & DebugVariableToText_AddName)
    {
        At += _snprintf_s(At, (memory_index)(End - At), (memory_index)(End - At),  
                          "%s", Event->Name);
    }
    
    if(Flags & DebugVariableToText_Semicolon)
    {
        At += _snprintf_s(At, (memory_index)(End - At), (memory_index)(End - At),  
                          ": ");
    }
    else
    {
        At += _snprintf_s(At, (memory_index)(End - At), (memory_index)(End - At),  
                          " ");
    }
    
    switch(Event->Type)
    {
        case DebugType_b32:
        {
            if(Flags & DebugVariableToText_PrettyBool)
            {
                At += _snprintf_s(At, (memory_index)(End - At), (memory_index)(End - At),  
                                  "%s", Event->B32 ?  "True" : "False");
            }
            else
            {
                At += _snprintf_s(At, (memory_index)(End - At), (memory_index)(End - At),  
                                  "%d", Event->B32);
            }
        }break;
        case DebugType_s32:
        {
            //if(!(Flags & DebugVariableToText_Screen && (Var->Type == DebugVariableType_b32)))
            At += _snprintf_s(At, (memory_index)(End - At), (memory_index)(End - At),  
                              "%d", Event->S32);
            
        }break;
        case DebugType_u32:
        {
            At += _snprintf_s(At, (memory_index)(End - At), (memory_index)(End - At),  
                              "%u", Event->U32);
            
        }break;
        case DebugType_r32:
        {
            At += _snprintf_s(At, (memory_index)(End - At), (memory_index)(End - At),  
                              "%ff", Event->R32);
        }break;
        case DebugType_vector_2:
        {
            At += _snprintf_s(At, (memory_index)(End - At), (memory_index)(End - At),  
                              "Vector2(%ff, %ff)", 
                              Event->V2.x, 
                              Event->V2.y);
        }break;
        case DebugType_vector_3:
        {
            At += _snprintf_s(At, (memory_index)(End - At), (memory_index)(End - At),  
                              "Vector3(%ff, %ff, %ff)", 
                              Event->V3.x, 
                              Event->V3.y, 
                              Event->V3.z);
        }break;
        case DebugType_vector_4:
        {
            At += _snprintf_s(At, (memory_index)(End - At), (memory_index)(End - At),  
                              "Vector4(%ff, %ff, %ff, %ff)", 
                              Event->V4.x, 
                              Event->V4.y, 
                              Event->V4.z, 
                              Event->V4.w);
        }break;
        case DebugType_rectangle_2:
        {
            At += _snprintf_s(At, (memory_index)(End - At), (memory_index)(End - At),  
                              "RectMinMax(Vector2(%ff, %ff) , Vector2(%ff, %ff))", 
                              Event->Rect2.Min.x, 
                              Event->Rect2.Min.y, 
                              Event->Rect2.Max.x, 
                              Event->Rect2.Max.y);
        }break;
        case DebugType_rectangle_3:
        {
            At += _snprintf_s(At, (memory_index)(End - At), (memory_index)(End - At),  
                              "RectMinMax(Vector3(%ff, %ff, %ff) , Vector3(%ff, %ff, %ff))", 
                              Event->Rect3.Min.x, 
                              Event->Rect3.Min.y, 
                              Event->Rect3.Min.z, 
                              Event->Rect3.Max.x, 
                              Event->Rect3.Max.y,
                              Event->Rect3.Max.z);
        }break;
    }
    
    
    if(Flags & DebugVariableToText_NullTerminator)
    {
        At += _snprintf_s(At, (memory_index)(End - At), (memory_index)(End - At), "\n");
    }
    
    memory_index Result = (At - Buffer);
    return Result;
}

inline b32
DEBUGVarShouldBeWritten(debug_event * Event)
{
    b32 Result = ((Event->Type != DebugType_CounterThreadList) && 
                  (Event->Type != DebugType_bitmap_id) &&
                  (Event->Type != DebugType_font_id) &&
                  (Event->Type != DebugType_sound_id));
    return Result; 
}


internal debug_tree *  
AddTree(debug_state * DebugState, debug_link * Link, vector_2 AtP)
{
    debug_tree * Result = PushStruct(&DebugState->DebugArena, debug_tree);
    Result->RootLink = Link;
    Result->UIP = AtP;
    
    DLIST_ADD_TAIL(&DebugState->TreeSentinel, Result);
    return Result;
}

struct debug_iterator 
{
    debug_link * Link;
    debug_link * Sentinel;
};

inline b32
IteratorIsValid(debug_iterator * Iter)
{
    b32 Result = (Iter->Link != Iter->Sentinel);
    return Result;
}


internal void 
DrawCounterThreadList(debug_state * DebugState, rectangle_2  Bounds, vector_2 MouseP)
{
    PushRect(DebugState->RenderGroup, Bounds, 0, Vector4(0,0,0,1.0f));
    
    u32 FrameCount = MAX_FRAME_COUNT;
    if(FrameCount > 8)
    {
        FrameCount = 8;
    }
    
    r32 ChartWidth = GetDim(Bounds).x;
    u32 LaneCount = DebugState->FrameBarLaneCount;
    
    r32 BarSpacing = 0;
    r32 BarHeight = 0;
    r32 LaneHeight = 0;
    
    if((LaneCount > 0) && (FrameCount > 0))
    {
        BarSpacing = GetDim(Bounds).y / (r32)FrameCount;
        BarHeight = BarSpacing - 5.0f;
        LaneHeight = BarHeight / LaneCount;
    }
    
    r32 ChartLeft = Bounds.Min.x;
    r32 ChartTop = Bounds.Max.y;
    
    r32 Scale = DebugState->FrameBarScale;
    
    vector_3 Colors[] =
    {
        {1.0f,0.0f,0.0f},
        {0.0f,1.0f,0.0f},
        {0.0f,0.0f,1.0f},
        {1.0f,1.0f,0.0f},
        {0.0f,1.0f,1.0f},
        {1.0f,0.0f,1.0f},
        {0.5f,1.0f,0.0f},
        {0.0f,1.0f,0.5f},
        {0.5f,0.0f,1.0f},
        {0.0f,0.5f,1.0f},
        {1.0f,0.5f,0.0f},
        {1.0f,0.0f,0.5f},
        {0.5f,1.0f,0.5f},
    };
    
    for(u32 FrameIndex = 0; 
        FrameIndex < FrameCount;
        ++FrameIndex)
    {
        debug_frame * Frame = GetRelCollatedFrame(DebugState,  -(s32)(FrameIndex + 1));
        r32 StartX = ChartLeft;
        r32 StartY = ChartTop - ((r32)FrameIndex * BarSpacing);
        
        for(u32 RegionIndex = 0;
            RegionIndex < Frame->RegionCount;
            ++RegionIndex)
        {
            debug_frame_region * Region = Frame->Regions + RegionIndex;
            
            r32 ValueMinX = StartX + (Scale * Region->MinX * ChartWidth);
            r32 ValueMaxX = StartX + (Scale * Region->MaxX * ChartWidth);
            
            
            r32 ValueHeight = ValueMaxX - ValueMinX;
            Assert(ValueHeight >= 0);
            
            if(Region->LaneIndex == 0)
            {
                int x = 5;
            }
            
            rectangle_2 RegionRect = RectMinMax(Vector2(ValueMinX,  StartY - 
                                                        (LaneHeight * (Region->LaneIndex + 1))), 
                                                Vector2(ValueMinX +  ValueHeight, 
                                                        StartY  - (LaneHeight * Region->LaneIndex)));
            
            //vector_3 Color = Colors[RegionIndex % ArrayCount(Colors)];
            vector_3 Color = Colors[Region->ColorIndex % ArrayCount(Colors)];
            PushRect(DebugState->RenderGroup, RegionRect, 0.0f, Vector4(Color, 1.0f));
            
            if(IsInRectangle(RegionRect, MouseP))
            {
#if 0
                char TextBuffer[256];
                _snprintf_s(TextBuffer, sizeof(TextBuffer),
                            "%s: %-10u Cy", 
                            Event->Name, 
                            (u32)Region->CycleCount);
                
                DrawTextAt(Vector2(MouseP.x, MouseP.y + 5.0f), TextBuffer);
#endif
                
            }
            
        }
    }
    
#if 0    
    {
        
        r32 BenchmarkHeight = BarSpacing * FrameCount;
        r32 BenchmarkWidth = 1.0f;
        
        rectangle_2 TargetCyclesRect = RectMinMax(Vector2(ChartLeft + ChartWidth - BenchmarkWidth, ChartTop - BenchmarkHeight),
                                                  Vector2(ChartLeft + ChartWidth, ChartTop));
        
        PushRect(DebugState->RenderGroup, TargetCyclesRect, 0, Vector4(1.0f, 1.0f, 1.0f, 1.0f));
    }
#endif
    
    
}

inline b32
InteractionsAreEqual(debug_interaction A , debug_interaction B)
{
    //TODO(Alex):probably expand this so it works with debug_id
    b32 Result = ((A.Type == B.Type) &&
                  (A.Generic == B.Generic) && 
                  DebugIDsAreEqual(A.ID, B.ID));
    return Result;
}

inline b32 
InteractionIsHot(debug_state * DebugState, debug_interaction Interaction)
{
    b32 Result = InteractionsAreEqual(DebugState->HotInteraction, Interaction);
    return Result;
}

struct debug_layout
{
    vector_2 At;
    u32 DepthLevel;
    r32 BaseX;
    r32 LineAdvance;
    vector_4 Color;
    r32 YSpacing;
    vector_2 MouseP;
    debug_state * DebugState;
};

struct debug_layout_element
{
    //NOTE: Storage
    b32 Size;
    vector_2 Dim;
    
    debug_interaction * Interaction;
    
    //NOTE Return 
    rectangle_2 Bounds;
};


inline debug_layout_element
StartLayoutElement(vector_2 Dim)
{
    debug_layout_element Element = {};
    Element.Size = false;
    Element.Dim = Dim;
    
    Element.Interaction = 0;
    Element.Bounds = InvertedInfinityRectangle2();
    return Element;
}

inline void
DefaultInteraction(debug_layout_element * Element, debug_interaction * Interaction)
{
    Element->Interaction = Interaction;
}

inline void
SetElementSizeable(debug_layout_element * Element)
{
    Element->Size = true;
}

inline void
ReturnLayoutElement(debug_layout * Layout, 
                    debug_layout_element * Element, 
                    b32 UseYSpacing)
{
    //NOTE(Alex):This will draw the rectangle based on the parameters aforementioned
    r32 YSpacing = (UseYSpacing) ? Layout->YSpacing : 0;
    Layout->At.x = Layout->BaseX + Layout->DepthLevel * Layout->LineAdvance * 2.0f;
    
    Element->Bounds = RectMinMax(Layout->At + Vector2(0, -Element->Dim.y - YSpacing),
                                 Layout->At + Vector2(Element->Dim.x, -YSpacing));
    
    r32 MinY = Element->Bounds.Min.y;
    
    rectangle_2 CornerRect = InvertedInfinityRectangle2();
    if(Element->Size)
    {
        vector_2 Radius = Vector2(4.0f, 4.0f);
        Element->Bounds = Offset(Element->Bounds, Vector2(Radius.x , -Radius.y));
        rectangle_2 Frame = AddRadiusTo(Element->Bounds, Radius); 
        
        vector_2 OuterMinCorner = Frame.Min;
        vector_2 OuterMaxCorner = Frame.Max;
        vector_2 InnerMinCorner = Element->Bounds.Min;
        vector_2 InnerMaxCorner = Element->Bounds.Max;
        
        
        rectangle_2 DownRect = RectMinMax(Vector2(InnerMinCorner.x, OuterMinCorner.y),
                                          Vector2(InnerMaxCorner.x, InnerMinCorner.y));
        
        rectangle_2 LeftRect = RectMinMax(Vector2(OuterMinCorner.x, InnerMinCorner.y),
                                          Vector2(InnerMinCorner.x, InnerMaxCorner.y));
        
        rectangle_2 RightRect = RectMinMax(Vector2(InnerMaxCorner.x, InnerMinCorner.y),
                                           Vector2(OuterMaxCorner.x, InnerMaxCorner.y));
        
        rectangle_2 UpRect = RectMinMax(Vector2(InnerMinCorner.x, InnerMaxCorner.y),
                                        Vector2(InnerMaxCorner.x, OuterMaxCorner.y));
        
        CornerRect = RectMinMax(Vector2(InnerMaxCorner.x, OuterMinCorner.y),
                                Vector2(OuterMaxCorner.x, InnerMinCorner.y));
        
        
        PushRect(Layout->DebugState->RenderGroup, CornerRect, 0, 
                 (InteractionIsHot(Layout->DebugState, *Element->Interaction)) ? 
                 Vector4(1.0f, 1.0f, 0.0f, 1.0f) : Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        
        
        PushRect(Layout->DebugState->RenderGroup, DownRect, 0, Vector4(0.5f, 0.5f, 0.5f, 1.0f));
        
        PushRect(Layout->DebugState->RenderGroup, LeftRect, 0, Vector4(0.5f, 0.5f, 0.5f, 1.0f));
        
        PushRect(Layout->DebugState->RenderGroup, RightRect, 0, Vector4(0.5f, 0.5f, 0.5f, 1.0f));
        
        PushRect(Layout->DebugState->RenderGroup, UpRect, 0, Vector4(0.5f, 0.5f, 0.5f, 1.0f));
        
        MinY = OuterMinCorner.y;
    }
    
    
    if(IsInRectangle(Element->Bounds, Layout->MouseP))
    {
        if(Element->Interaction->Type != DebugInteraction_SetAutomatic)
        {
            Element->Interaction->Type = DebugInteraction_SetAutomatic;
        }
        
        Layout->DebugState->NextHotInteraction = *Element->Interaction;
    }
    if(IsInRectangle(CornerRect, Layout->MouseP))
    {
        Layout->DebugState->NextHotInteraction = *Element->Interaction;
    }
    
    Layout->At.y =  MinY;
}


inline debug_interaction 
EventInteraction(debug_interaction_type Type, debug_tree * Tree, debug_link * Link)
{
    debug_interaction Result = {};
    Result.Type = Type;
    Result.ID =  GetDebugID(Tree, Link);
    Result.Event = Link->Event;
    
    return Result;
}

internal void
DrawHierarchicalMenu(debug_state * DebugState, vector_2 MouseP)
{
    //TODO(Alex): Check how to unleash variable stepping through var references
    //shall we show only the hierarchy that we teared off? probably yeah.
    for(debug_tree * Tree = DebugState->TreeSentinel.Next;
        Tree != &DebugState->TreeSentinel;
        Tree = Tree->Next)
    {
        vector_2 TreeHandleDim = Vector2(20.0f,20.0f); 
        debug_layout Layout = {};
        
        Layout.At = Tree->UIP;
        Layout.DepthLevel = 0;
        Layout.BaseX = Tree->UIP.x + TreeHandleDim.x;
        Layout.LineAdvance = GetLineAdvanceFor(DebugState->HHA) * DebugState->FontScale;
        Layout.YSpacing = 4.0f;
        Layout.MouseP = MouseP;
        Layout.DebugState = DebugState;
        
        debug_iterator Stack[MAX_STACK_SIZE] = {0};
        
#if 0        
        Stack[0].Sentinel = &Tree->RootVar->Sentinel; 
        Stack[0].Link = Tree->RootVar->Sentinel.Next; 
        debug_frame * LastFrame = GetLastCollatedFrame(DebugState);
#endif
        debug_frame * LastFrame = GetLastCollatedFrame(DebugState);
        if(LastFrame->RootGroup)
        {
            Stack[0].Sentinel = LastFrame->RootGroup->Sentinel; 
            Stack[0].Link = Stack[0].Sentinel->Next; 
        }
        
        
        debug_iterator * Iter = Stack;
        
        {
            debug_interaction MoveInteraction = {};
            MoveInteraction.Type = DebugInteraction_Move;
            MoveInteraction.P = &Tree->UIP;
            
            rectangle_2 TreeRect = RectMinDim(Tree->UIP - Vector2(0, TreeHandleDim.y), TreeHandleDim);
            
            PushRect(DebugState->RenderGroup, TreeRect, 0, 
                     (InteractionIsHot(DebugState, MoveInteraction)) ? 
                     Vector4(1.0f, 1.0f, 0.0f, 1.0f) : Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            
            if(IsInRectangle(TreeRect, MouseP))
            {
                DebugState->NextHotInteraction = MoveInteraction;
            }
        }
        
        for(;;)
        {
            if(!IteratorIsValid(Iter))
            {
                if(Layout.DepthLevel > 0) 
                {
                    Iter = Stack + --Layout.DepthLevel;
                }
                else
                {
                    break;
                }
            }
            else
            {
                Assert(Iter->Link);
                debug_link * Link = Iter->Link;
                debug_event * Event = Iter->Link->Event;
                debug_interaction ItemInteraction = EventInteraction(DebugInteraction_SetAutomatic, Tree, Link);
                
                b32 IsHot = InteractionIsHot(DebugState, ItemInteraction);
                vector_4 Color = (IsHot)  ? Vector4(1.0f, 1.0f, 0, 1.0f) : Vector4(1.0f,1.0f,1.0f,1.0f);
                
                debug_view * View  = DEBUGGetViewFor(DebugState, GetDebugID(Tree, Link));
                switch(Event->Type)
                {
                    case DebugType_CounterThreadList:
                    {
                        View->Type = DebugViewType_InlineBlock;
                        debug_interaction ResizeInteraction = EventInteraction(DebugInteraction_Resize, Tree, Link); 
                        
                        debug_layout_element Element = StartLayoutElement(View->InlineBlock.Dim);
                        DefaultInteraction(&Element, &ResizeInteraction);
                        SetElementSizeable(&Element);
                        ReturnLayoutElement(&Layout, &Element, true);
                        
                        View->InlineBlock.Offset = Element.Bounds.Min + Vector2(0, GetDim(Element.Bounds).y);
                        
                        DrawCounterThreadList(DebugState, Element.Bounds, MouseP);
                    }break;
                    case DebugType_bitmap_id:
                    {
                        View->Type = DebugViewType_InlineBlock;
                        
                        hha_bitmap * HHA = GetBitmapInfo(DebugState->Assets, Event->BitmapID);
                        r32 WidthOverHeight = (r32)HHA->Dim[0] / (r32)HHA->Dim[1];
                        View->InlineBlock.Dim.x = View->InlineBlock.Dim.y * WidthOverHeight;
                        
                        debug_interaction ResizeInteraction = EventInteraction(DebugInteraction_Resize, Tree, Link); 
                        
                        debug_layout_element Element = StartLayoutElement(View->InlineBlock.Dim);
                        DefaultInteraction(&Element, &ResizeInteraction);
                        SetElementSizeable(&Element);
                        ReturnLayoutElement(&Layout, &Element, true);
                        
                        View->InlineBlock.Offset = Element.Bounds.Min + Vector2(0, GetDim(Element.Bounds).y);
                        
                        PushRect(DebugState->RenderGroup, Vector3(GetCenter(Element.Bounds),0), 
                                 GetDim(Element.Bounds),
                                 Vector4(0.0f,0.0f,0.0f,1.0f));
                        PushBitmap(DebugState->RenderGroup, Event->BitmapID, 
                                   GetDim(Element.Bounds).y,  
                                   Vector3(GetCenter(Element.Bounds),0), 
                                   Vector4(1.0f, 1.0f, 1.0f, 1.0f), true);
                    }break;
                    default:
                    {
                        switch(Event->Type)
                        {
                            case DebugType_OpenDataBlock:
                            {
                                View->Type = DebugViewType_Collapsible;
                            }break;
                            default:
                            {
                                View->Type = DebugViewType_Basic;
                            }break;
                        }
                        
                        char Text[512] = {0};
                        char * At = Text;
                        At += DEBUGEventToText(Event, 
                                               At, 
                                               Text + (ArrayCount(Text) * sizeof(char)), 
                                               DebugVariableToText_AddName|
                                               DebugVariableToText_PrettyBool|
                                               DebugVariableToText_Semicolon
                                               );
                        //TODO(Alex): think a little bit more on how to advance text in y 
                        //so the rectangle is positioned to include descenders 
                        rectangle_2 TextBounds = DEBUGGetTextSize(Text);
                        View->InlineBlock.Dim = Vector2(GetDim(TextBounds).x, Layout.LineAdvance);
                        
                        debug_layout_element Element = StartLayoutElement(View->InlineBlock.Dim);
                        DefaultInteraction(&Element, &ItemInteraction);
                        ReturnLayoutElement(&Layout, &Element, false);
                        
                        DrawTextAt(Element.Bounds.Min, Text, Color);
                    }break;
                }
                
                if(View->Type == DebugViewType_Collapsible)
                {
                    //if(View->Collapsible.ExpandedAlways)
                    {
                        debug_link * Temp = Iter->Link; 
                        Iter = Stack + ++Layout.DepthLevel;
                        Iter->Sentinel = Temp->Sentinel;
                        Iter->Link = Iter->Sentinel;
                    }
                }
            }
            
            Iter->Link = Iter->Link->Next;
        }
        
        //TODO(Alex): Find out how to handle AtY position or if we need to expand the 
        //notion to have multiple copies of them 
        DebugState->AtY = Layout.At.y;
    }
}

internal void 
DrawDebugMainMenu(debug_state * DebugState, vector_2 MouseP)
{
    
#if 0    
    u32 HotIndex = U32Max;
    r32 BestDistance = Real32Max;
    
    r32 Radius = 200.0f;
    
    u32 ItemCount = ArrayCount(DebugVariableList);
    r32 CirclePortion = Tau32 / (r32)ItemCount; 
    vector_2 CenterP = DebugState->MenuPos;
    
    for(u32 VarIndex = 0;
        VarIndex < ItemCount;
        ++VarIndex)
    {
        debug_variable * Var = DebugVariableList + VarIndex; 
        
        r32 Angle = VarIndex * CirclePortion;
        vector_2 P = CenterP + Radius * Arm2(Angle);
        
        vector_4 TextColor = (Var->Value) ? Vector4(1.0f,1.0f,1.0f,1.0f) : Vector4(0.5f,0.5f,0.5f,1.0f);
        if(VarIndex == DebugState->HotIndex)
        {
            TextColor = Vector4(1.0f, 1.0f, 0, 1.0f);
        }
        
        r32 ThisDistance = GetDistance(P, MouseP);
        if(BestDistance > ThisDistance)
        {
            BestDistance = ThisDistance;
            HotIndex = VarIndex;
        }
        
        rectangle_2 TextRect = DEBUGGetTextSize(Var->Name);
        DrawTextAt(P - (0.5f * GetDim(TextRect)), Var->Name, TextColor);
    }
    
    if(BestDistance < (Radius * 0.5f))
    {
        DebugState->HotIndex = HotIndex;
    }
    else
    {
        DebugState->HotIndex = INVALID_DEBUG_VARIABLE_INDEX;
    }
#endif
    
}

#define NotDragged(P1 , P2) (((P1).x == (P2).x) || ((P1).y == (P2).y))

internal void
BeginInteract(debug_state * DebugState, game_input * Input, vector_2 MouseP, b32 AltUI)
{
    DebugState->BeginInteractMouseP = MouseP;
    debug_interaction * HotInteraction = &DebugState->HotInteraction;
    if(InteractionIsValid(HotInteraction))
    {
        switch(HotInteraction->Type)
        {
            case DebugInteraction_SetAutomatic:
            {
                if(AltUI)
                {
                    //if(HotInteraction->Type != DebugInteraction_Move)
                    {
                        HotInteraction->Type = DebugInteraction_Tear;
                    }
                }
                else
                {
                    switch(HotInteraction->Event->Type)
                    {
                        case DebugType_OpenDataBlock:
                        {
                            HotInteraction->Type = DebugInteraction_Toggle;
                        }break;
                        case DebugType_b32:
                        {
                            HotInteraction->Type = DebugInteraction_Toggle;
                        }break;
                        case DebugType_r32: 
                        {
                            HotInteraction->Type = DebugInteraction_Drag;
                        }break;
                    }
                }
            }break;
            case DebugInteraction_Resize:
            {
                debug_view * View  = DEBUGGetViewFor(DebugState, HotInteraction->ID);
                vector_2 RectDim = Vector2(View->InlineBlock.Dim.x , -View->InlineBlock.Dim.y);
                DebugState->RelMouseP = MouseP - (View->InlineBlock.Offset + RectDim);
            }break;
        }
        
        if(HotInteraction->Type == DebugInteraction_Tear)
        {
#if 0
            debug_event * Event = HotInteraction->Event;
            
            debug_variable * Group = DEBUGBeginGroup(&DebugState->DebugArena, 0, "New Tree");
            debug_variable_link * Link = DEBUGAddVariableToGroup(&DebugState->DebugArena, Group, Event);
            debug_tree * Tree = AddTree(DebugState, Group, MouseP);
            
            DebugState->HotInteraction.Type = DebugInteraction_Move;
            DebugState->HotInteraction.P = &Tree->UIP;
#endif
        }
        
        DebugState->Interaction = DebugState->HotInteraction;
    }
    else
    {
        DebugState->Interaction.Type = DebugInteraction_NOP;
        DebugState->Interaction.Generic = 0;
    }
}

internal void 
EndInteract(debug_state * DebugState, game_input * Input, vector_2 MouseP)
{
    if((!DebugState->Compiling) && (DebugState->Interaction.Type != DebugInteraction_NOP))
    {
        switch(DebugState->Interaction.Type)
        {
            case DebugInteraction_Select:
            {
                if(!Input->CommandButtons[PlatformCommandButton_Shift].ButtonEndedDown)
                {
                    ClearSelection();
                }
                
                AddToSelection(DebugState->Interaction.ID); 
            }break;
            case DebugInteraction_Toggle:
            {
                if(NotDragged(MouseP, DebugState->BeginInteractMouseP))
                {
                    debug_event * Event = DebugState->Interaction.Event;
                    void * Generic = DebugState->Interaction.Generic;
                    switch(Event->Type)
                    {
                        case DebugType_OpenDataBlock:
                        {
                            debug_view * View  = DEBUGGetViewFor(DebugState, DebugState->Interaction.ID);
                            View->Collapsible.ExpandedAlways = !View->Collapsible.ExpandedAlways; 
                        }break;
                        case DebugType_b32:
                        {
                            Event->B32 = !Event->B32; 
                        }break;
                        case DebugType_debug_id:
                        {
                            b32 * B32 = (b32*)Event->ID.Value[0];
                            *B32 = !(*B32);
                        }break;
                    }
                    
                }
                
            }break;
        }
    }
    
    DebugState->NextHotInteraction = NullInteraction();
    DebugState->Interaction = NullInteraction();
}

internal void
DEBUGInteract(debug_state * DebugState, game_input * Input)
{
    vector_2 MouseP = Vector2(Input->MouseX, Input->MouseY); 
    vector_2 DeltaMouseP = MouseP - DebugState->LastMouseP;
    
    if(InteractionIsValid(&DebugState->Interaction))
    {
        debug_event * Event = DebugState->Interaction.Event;
        switch(DebugState->Interaction.Type)
        {
            case DebugInteraction_Drag:
            {
                switch(Event->Type)
                {
                    case DebugType_r32:
                    {
                        Event->R32 += DeltaMouseP.x; 
                    }break;
                }
            }break;
            case DebugInteraction_Resize:
            {
                debug_view * View  = DEBUGGetViewFor(DebugState, DebugState->Interaction.ID);
                
                View->InlineBlock.Dim.x = MouseP.x - (View->InlineBlock.Offset.x + DebugState->RelMouseP.x);
                View->InlineBlock.Dim.y = (View->InlineBlock.Offset.y + DebugState->RelMouseP.y) - MouseP.y;
                
                View->InlineBlock.Dim.x = Maximum(40.0f, View->InlineBlock.Dim.x);
                View->InlineBlock.Dim.y = Maximum(40.0f, View->InlineBlock.Dim.y);
            }break;
            case DebugInteraction_Move:
            {
                *(DebugState->Interaction.P) += DeltaMouseP;  
            }break;
        }
        
        b32 AltUI = (Input->MouseButtons[PlatformMouseButton_Right].ButtonEndedDown);
        
        for(s32 TransitionIndex = Input->MouseButtons[PlatformMouseButton_Left].HalfTransitionCount;
            TransitionIndex > 1;
            --TransitionIndex)
        {
            EndInteract(DebugState, Input, MouseP);
            BeginInteract(DebugState, Input, MouseP, AltUI);
        }
        
        if(!Input->MouseButtons[PlatformMouseButton_Left].ButtonEndedDown)
        {
            EndInteract(DebugState, Input, MouseP);
        }
    }
    else
    {
        DebugState->HotInteraction = DebugState->NextHotInteraction;
        
        b32 AltUI = (Input->MouseButtons[PlatformMouseButton_Right].ButtonEndedDown);
        
        for(s32 TransitionIndex = Input->MouseButtons[PlatformMouseButton_Left].HalfTransitionCount;
            TransitionIndex > 1;
            --TransitionIndex)
        {
            BeginInteract(DebugState, Input, MouseP, AltUI);
            EndInteract(DebugState, Input, MouseP);
        }
        
        if(Input->MouseButtons[PlatformMouseButton_Left].ButtonEndedDown)
        {
            BeginInteract(DebugState, Input, MouseP, AltUI);
        }
    }
    
    DebugState->LastMouseP = MouseP;
}


//-------------------------------------------------------------------------

global_variable debug_table GlobalDebugTable_ = {};
debug_table * GlobalDebugTable = &GlobalDebugTable_;

//-------------------------------------------------------------------------

inline u32 
GetLaneFromThreadIndex(debug_state * DebugState, u32 PlatformThreadIndex, thread_table * ThreadTable)
{
    u32 Result = 0;
    
    for(u32 ThreadIndex = 0; 
        ThreadIndex < ThreadTable->Count;
        ++ThreadIndex)
    {
        if(ThreadTable->IDs[ThreadIndex] == PlatformThreadIndex)
        {
            Result = ThreadIndex;
        }
    }
    
    return Result;
}

internal debug_thread * 
GetDebugThread(debug_state * DebugState, u32 ThreadID)
{
    //NOTE(Alex): Here we know that we cannot have more than n threads so its feasible to 
    //keep them into the arena
    debug_thread * Result = 0;
    for(debug_thread * Thread = DebugState->FirstThread; 
        Thread; 
        Thread = Thread->Next)
    {
        if(Thread->ID == ThreadID)
        {
            Result = Thread;
            break;
        }
    }
    
    if(!Result)
    {
        Result = PushStruct(&DebugState->DebugArena, debug_thread); 
        Result->ID = ThreadID;
        Result->FirstOpenCodeBlock = 0;
        Result->FirstOpenDataBlock = 0;
        Result->LaneIndex = (u16)DebugState->FrameBarLaneCount++;
        
        Result->Next = DebugState->FirstThread;
        DebugState->FirstThread = Result;
        
    }
    
    return Result;
}


internal debug_frame_region * 
AddRegion(debug_state * DebugState, debug_frame * CurrentFrame)
{
    Assert(CurrentFrame->RegionCount < ArrayCount(CurrentFrame->Regions));
    debug_frame_region * Result = CurrentFrame->Regions + CurrentFrame->RegionCount++;
    return Result;
}

internal debug_open_block * 
AllocateDebugOpenBlock(debug_state * DebugState, 
                       debug_event * Event, 
                       debug_open_block ** FirstOpenBlock)
{
    debug_open_block * Result  = DebugState->FirstFreeBlock;
    
    if(Result)
    {
        DebugState->FirstFreeBlock = Result->NextFree;
    }
    else
    {
        Result = PushStruct(&DebugState->CollateArena, debug_open_block);
    }
    
    Result->StartingFrameIndex = DebugState->FrameIndex; 
    Result->OpeningEvent = Event;
    Result->NextFree = 0;
    
    Result->Parent = *FirstOpenBlock;
    *FirstOpenBlock = Result;
    
    return Result;
}

internal void
DeallocateDebugOpenBlock(debug_state * DebugState, 
                         debug_open_block  ** OpenBlock)
{
    (*OpenBlock)->NextFree = DebugState->FirstFreeBlock; 
    DebugState->FirstFreeBlock = *OpenBlock;
    *OpenBlock = (*OpenBlock)->Parent;
}

inline b32 
EventsMatch(debug_event A, debug_event B)
{
    b32 Result = (A.ThreadID == B.ThreadID);
    return Result;
}

internal debug_link *  
DEBUGCollationAddLink(debug_frame * Frame, debug_link * Parent, debug_event * Event)
{
    Assert(Frame->LinkCount < ArrayCount(Frame->Links));
    debug_link * Link = Frame->Links + Frame->LinkCount++;
    Link->Event = Event;
    if(Parent)
    {
        DLIST_ADD_TAIL(Parent->Sentinel, Link);
    }
    return Link;
}

internal debug_link *
DEBUGCollationAddGroupLink(debug_frame * Frame, 
                           debug_link * Parent, 
                           debug_event * Event)
{
    debug_link * Group = DEBUGCollationAddLink(Frame, Parent,Event);
    Assert(Frame->LinkCount < ArrayCount(Frame->Links));
    Group->Sentinel = Frame->Links + Frame->LinkCount++;
    DLIST_INIT(Group->Sentinel);
    
    return Group;
}

internal void 
RestartCollation(debug_state * DebugState)
{
    EndTempMemory(DebugState->DebugTempMemory);
    DebugState->DebugTempMemory = BeginTempMemory(&DebugState->CollateArena);
    
    DebugState->FirstThread = 0;
    DebugState->FirstFreeBlock = 0;
    
    //DebugState->CollationFrame = 0;
    
    DebugState->FrameBarLaneCount = 0;
    //NOTE(Alex): This is not accurate for the moment
    DebugState->FrameBarScale = 1.0f / 60000000.0f;
}


internal void 
CollateDebugEvents(debug_state * DebugState, u32 InvalidEventArrayIndex)
{
    Assert(InvalidEventArrayIndex < MAX_DEBUG_EVENT_ARRAY_COUNT);
    
    u32 CollationArrayIndex = (MAX_DEBUG_EVENT_ARRAY_COUNT + ((s32)InvalidEventArrayIndex - 1)) % MAX_DEBUG_EVENT_ARRAY_COUNT;
    debug_event * Source = GlobalDebugTable->Events[CollationArrayIndex];
    u32 EventCount = GlobalDebugTable->EventCount[CollationArrayIndex];
    
    for(u32 EventIndex = 0;
        (EventIndex < EventCount);
        ++EventIndex)
    {
        debug_event * Event = Source + EventIndex;
        debug_thread * Thread = GetDebugThread(DebugState, Event->ThreadID);
        
        //TODO(Alex): Add FileName, LineNumber etc.
        if(Event->Type == DebugType_FrameMarker)
        {
            if(DebugState->CollationFrame)
            {
                DebugState->CollationFrame->EndClock = Event->Clock; 
                DebugState->CollationFrame->SecondsElapsed = Event->R32;
                ++DebugState->FrameIndex;
                
                u64 ClockRange = DebugState->CollationFrame->EndClock - DebugState->CollationFrame->BeginClock;
                
#if 0
                if(ClockRange > 0.0f)
                {
                    
                    r32 Scale = (1.0f / ClockRange);
                    if(DebugState->FrameBarScale > Scale)
                    {
                        DebugState->FrameBarScale = Scale;
                    }
                }
#endif
                
            }
            
            if(DebugState->FrameIndex >= MAX_FRAME_COUNT)
            {
                DebugState->FrameIndex = 0;
            }
            
            Assert(DebugState->FrameIndex < MAX_FRAME_COUNT);
            
            DebugState->CollationFrame = DebugState->Frames + DebugState->FrameIndex;
            DebugState->CollationFrame->BeginClock = Event->Clock;
            DebugState->CollationFrame->EndClock = 0;
            DebugState->CollationFrame->RegionCount = 0;
            
            DebugState->CollationFrame->LinkCount = 0;
            DebugState->CollationFrame->RootGroup = 0;
            
            DebugState->CollationFrame->RootGroup = DEBUGCollationAddGroupLink(DebugState->CollationFrame, 0, Event);
        }
        else if(DebugState->CollationFrame)
        {
            switch(Event->Type)
            {
                case DebugType_Begin:
                {
                    debug_open_block * DebugBlock = AllocateDebugOpenBlock(DebugState, 
                                                                           Event, 
                                                                           &Thread->FirstOpenCodeBlock);
                }break;
                case DebugType_End:
                {
                    if(Thread->FirstOpenCodeBlock)
                    {
                        debug_open_block * MatchingBlock = Thread->FirstOpenCodeBlock;   
                        
                        if(EventsMatch(*MatchingBlock->OpeningEvent, *Event))
                        {
                            if(MatchingBlock->StartingFrameIndex == DebugState->FrameIndex)
                            {
                                //TODO(Alex): Formalize this selector 
                                //TODO(Alex): Make Unique COUNTER Ids?
                                if(!MatchingBlock->Parent)
                                {
                                    r32 MinX = (r32)(MatchingBlock->OpeningEvent->Clock - DebugState->CollationFrame->BeginClock);
                                    r32 MaxX = (r32)(Event->Clock - DebugState->CollationFrame->BeginClock);
                                    r32 ThresholdY = 10.0f;
                                    r32 CycleCount = (MaxX - MinX); 
                                    if(CycleCount > ThresholdY)
                                    {
                                        debug_frame_region * Region = AddRegion(DebugState, DebugState->CollationFrame);
                                        Region->MinX = MinX;
                                        Region->MaxX = MaxX;
                                        Region->LaneIndex = Thread->LaneIndex;
                                        Region->CycleCount = (u32)CycleCount;
                                    }
                                }
                            }
                            else
                            {
                                //TODO(Alex): Record all frames in between and begin/end spans. 
                            }
                            
                            DeallocateDebugOpenBlock(DebugState, &Thread->FirstOpenCodeBlock);
                        }
                        else
                        {
                            //TODO(Alex): How to manage Regions that we only have its closing index?
                            //Shall we draw a line from the starting frame up to the closing event-Clock?
                        }
                    }
                }break;
                case DebugType_OpenDataBlock:
                {
                    //TODO(Alex): Simplify this so we dont need to cache events, use DebugArena 
                    //to pop memory at hot code reload
                    debug_link * Group = DEBUGCollationAddGroupLink(DebugState->CollationFrame, 
                                                                    (Thread->FirstOpenDataBlock) ? 
                                                                    Thread->FirstOpenDataBlock->Link : DebugState->CollationFrame->RootGroup,
                                                                    Event);
                    
                    debug_open_block * OpenBlock = AllocateDebugOpenBlock(DebugState, Event, &Thread->FirstOpenDataBlock);
                    OpenBlock->Link = Group;
                }break;
                case DebugType_CloseDataBlock:
                {
                    Assert(Thread->FirstOpenDataBlock);
                    if(EventsMatch(*Thread->FirstOpenDataBlock->OpeningEvent, *Event))
                    {
                        DeallocateDebugOpenBlock(DebugState, &Thread->FirstOpenDataBlock);
                    }
                }break;
                default:
                {
                    debug_link * Link = DEBUGCollationAddLink(DebugState->CollationFrame,
                                                              (Thread->FirstOpenDataBlock) ? 
                                                              Thread->FirstOpenDataBlock->Link : DebugState->CollationFrame->RootGroup, 
                                                              Event);
                }break;
            }
        }
    }
}

#if 0    
internal void
RefreshCollation(debug_state * DebugState)
{
    //NOTE(Alex): This will make collation twice for frame
    RestartCollation(DebugState, (u32)GlobalDebugTable->CurrentEventArrayIndex);
    CollateDebugEvents(DebugState, (u32)GlobalDebugTable->CurrentEventArrayIndex);
}
#endif


internal void
AddEventDebugSwitches()
{
    DEBUG_OPEN_DATA_BLOCK_(Debug Switches);
    {
        DEBUG_OPEN_DATA_BLOCK_(Render);
        {
            DebugAddVariableSwitch(SampleEnvironmentMap);
            DEBUG_OPEN_DATA_BLOCK_(Cameras);
            {
                DebugAddVariableSwitch(EnableDebugCamera);
                DebugAddVariableSwitch(UseRoomBasedCamera);
                DebugAddVariableSwitch(DebugCameraDistance);
            }
            DEBUG_CLOSE_DATA_BLOCK();
        }
        DEBUG_CLOSE_DATA_BLOCK();
        
        DEBUG_OPEN_DATA_BLOCK_(GroundChunks);
        {
            DebugAddVariableSwitch(GroundChunkOutlines);
            DebugAddVariableSwitch(GroundChunkCheckerBoard);
            DebugAddVariableSwitch(GroundChunkReloading);
        }
        DEBUG_CLOSE_DATA_BLOCK();
        
        DEBUG_OPEN_DATA_BLOCK_(Particles);
        {
            DebugAddVariableSwitch(ParticlesTest);
            DebugAddVariableSwitch(ParticlesGrid);
        }
        DEBUG_CLOSE_DATA_BLOCK();
        
        DEBUG_OPEN_DATA_BLOCK_(EntitySystem);
        {
            DebugAddVariableSwitch(UsedSpaceOutlines);
            DebugAddVariableSwitch(EntityCollisionVolumes);
            DebugAddVariableSwitch(FamiliarFollowsHero);
        }
        DEBUG_CLOSE_DATA_BLOCK();
        
#if 0            
        DEBUG_OPEN_DATA_BLOCK_("Profiler");
        {
            DEBUG_OPEN_DATA_BLOCK_("By Thread");
            {
                DEBUGAddVariable(DebugVariableType_CounterThreadList);
            }
            DEBUG_CLOSE_DATA_BLOCK();
            DEBUG_OPEN_DATA_BLOCK_("By Function");
            {
                DEBUGAddVariable(DebugVariableType_CounterThreadList);
            }
            DEBUG_CLOSE_DATA_BLOCK();
        }
        DEBUG_CLOSE_DATA_BLOCK();
        
        DEBUG_OPEN_DATA_BLOCK_("BitmapDisplay");
        {
            DEBUGAddVariable(DebugVariableType_BitmapDisplay, Asset_Head);
        }
        DEBUG_CLOSE_DATA_BLOCK();
#endif
        
    }
    DEBUG_CLOSE_DATA_BLOCK();
    
    
}

internal void 
DEBUGStart(debug_state * DebugState, game_assets * Assets, s32 Width, s32 Height)
{
    TIMED_FUNCTION();
    
    if(!DebugState->IsInitialized)
    {
        DebugState->Assets = Assets;
        
        InitializeArena(&DebugState->DebugArena, GlobalDebugMemory->DebugStorageSize - sizeof(debug_state),
                        (uint8*)GlobalDebugMemory->DebugStorage + sizeof(debug_state));
        
        
        DebugState->TreeSentinel.RootLink = 0;
        DebugState->TreeSentinel.UIP = {};
        DLIST_INIT(&DebugState->TreeSentinel);
        
        AddTree(DebugState, 0, Vector2(-0.5f * Width, 0.5f * Height));
        
        
        DebugState->RenderGroup = AllocateRenderGroup(Assets, &DebugState->DebugArena, Megabytes(32), false);
        DebugState->HighPriorityQueue = GlobalDebugMemory->HighPriorityQueue;
        
        DebugState->Interaction = NullInteraction();
        DebugState->FrameIndex = 0;
        DebugState->FrameBarLaneCount = 0;
        DebugState->FrameBarScale = 1.0f / 60000000.0f;;
        DebugState->FreezeCollation = false;
        
        DebugState->Frames = PushArray(&DebugState->DebugArena, MAX_FRAME_COUNT, debug_frame);
        SubArena(&DebugState->CollateArena, &DebugState->DebugArena, Megabytes(200));
        
        DebugState->DebugTempMemory = BeginTempMemory(&DebugState->CollateArena);
        RestartCollation(DebugState);
        
        DebugState->IsInitialized = true;
    }
    
    BeginRenderGroup(DebugState->RenderGroup);
    
    DebugState->FontScale = 1.0f;
    Ortographic(DebugState->RenderGroup, Width, Height, 1.0f);
    
    asset_vector MatchVector = {};
    MatchVector.E[Tag_FontType] = FontType_Debug;
    asset_vector WeightVector = {};
    WeightVector.E[Tag_FontType] = 1.0f;
    
    DebugState->Assets = Assets;
    DebugState->FontID = GetBestMatchFontFrom(Assets, Asset_Font, &MatchVector, &WeightVector);
    DebugState->Font = PushFont(DebugState->RenderGroup, DebugState->FontID);
    DebugState->HHA = GetFontInfo(Assets, DebugState->FontID);
    DebugState->AtY = (0.5f * Height);
    DebugState->LeftEdge = (-0.5f * Width);
    
    //AddEventDebugSwitches();
}


internal void
DumpStructDataOut(u32 MemberCount, 
                  member_definition * MemberDef, 
                  void * Instance,
                  u32 DepthLevel = 1)
{
    for(u32 MemberIndex = 0;
        MemberIndex < MemberCount;
        ++MemberIndex)
    {
        member_definition * Def = MemberDef + MemberIndex;
        
        for(u32 ArrayIndex = 0; 
            ArrayIndex < Def->Count;
            ++ArrayIndex)
        {
            char TextBuffer[4096] = {0};
            char * At  = TextBuffer;
            
            char * MemberOffset = (char*)(Instance) + Def->Offset;
            
            for(u32 DepthIndex = 0; 
                DepthIndex < DepthLevel;
                ++DepthIndex)
            {
                *At++ = ' ';
                *At++ = ' ';
                *At++ = ' ';
                *At++ = ' ';
            }
            
            memory_index BufferSize = TextBuffer + sizeof(TextBuffer) - At; 
            
            //TODO(Alex): Metagenerate this completely!
            switch(Def->Type)
            {
                case MetaType_uint8:
                {
                    _snprintf_s(At, BufferSize, BufferSize, 
                                "%s: %d Size: %d\n", 
                                Def->VarName, 
                                *((u32*)MemberOffset + ArrayIndex), 
                                Def->Size);
                }break;
                case MetaType_entity_type:
                {
                    _snprintf_s(At, BufferSize, BufferSize, 
                                "%s: %d Size: %d\n", 
                                Def->VarName, 
                                *((u32*)MemberOffset + ArrayIndex), 
                                Def->Size);
                }break;
                case MetaType_b32:
                {
                    _snprintf_s(At, BufferSize, BufferSize, 
                                "%s: %d Size: %d\n", 
                                Def->VarName, 
                                *((b32*)MemberOffset + ArrayIndex),
                                Def->Size);
                }break;
                case MetaType_r32:
                {
                    _snprintf_s(At, BufferSize, BufferSize, 
                                "%s: %.10f Size: %d\n", 
                                Def->VarName, 
                                *((r32*)MemberOffset + ArrayIndex),
                                Def->Size);
                }break;
                case MetaType_u32:
                case MetaType_uint32:
                {
                    _snprintf_s(At, BufferSize, BufferSize,
                                "%s: %d Size: %d\n", 
                                Def->VarName, 
                                *((u32*)MemberOffset + ArrayIndex),
                                Def->Size);
                }break;
                
#if 0                
                case MetaType_int32:
                {
                    _snprintf_s(At, BufferSize, BufferSize,
                                "%s: %d Size: %d\n", 
                                Def->VarName, 
                                *((s32*)MemberOffset + ArrayIndex),
                                Def->Size);
                }break;
#endif
                
                case MetaType_vector_2:
                {
                    _snprintf_s(At, BufferSize, BufferSize, 
                                "%s: (%.5f,%.5f) Size: %d\n", 
                                Def->VarName,
                                ((vector_2*)MemberOffset + ArrayIndex)->x,
                                ((vector_2*)MemberOffset + ArrayIndex)->y,
                                Def->Size);
                }break;
                case MetaType_vector_3:
                {
                    _snprintf_s(At, BufferSize, BufferSize, 
                                "%s: (%.5f,%.5f,%.5f) Size: %d\n", 
                                Def->VarName, 
                                ((vector_3*)MemberOffset + ArrayIndex)->x,
                                ((vector_3*)MemberOffset + ArrayIndex)->y,
                                ((vector_3*)MemberOffset + ArrayIndex)->z,
                                Def->Size);
                }break;
                
                DEBUG_SWITCH_MEMBER_DEFS(DepthLevel + 1);
                
                default:
                {
                }break;
            }
            
            
            if(At[0])
            {
                DEBUGTextLine(TextBuffer, Vector4(1.0f,1.0f,1.0f,1.0f));
            }
        }
    }
}

internal void 
DEBUGEnd(debug_state * DebugState, loaded_bitmap * DrawBuffer, 
         game_input * Input, game_memory * Memory)
{
    TIMED_FUNCTION();
    
    Assert(DebugState->RenderGroup->InsideRenderer);
    CheckGlobalAlphaVisible(DebugState->RenderGroup);
    
    vector_2 MouseP = Unproject(&DebugState->RenderGroup->Transform, 
                                Vector2(Input->MouseX, Input->MouseY), 
                                DebugState->RenderGroup->Transform.OffsetP.z).xy;
    
    DrawHierarchicalMenu(DebugState, MouseP);
    DEBUGInteract(DebugState, Input);
    
    //NOTE(Alex): Radial Menu
    if(Input->MouseButtons[PlatformMouseButton_Right].ButtonEndedDown)
    {
        if(Input->MouseButtons[PlatformMouseButton_Right].HalfTransitionCount)
        {
            DebugState->MenuPos =  MouseP;
            Memory->LastMenuPosX = Input->MouseX;
            Memory->LastMenuPosY = Input->MouseY;
        }
        
        if(Memory->LoopMemoryReset)
        {
            DebugState->MenuPos = Vector2(Memory->LastMenuPosX, Memory->LastMenuPosY); 
        }
        
#if 0
        DrawDebugMainMenu(DebugState, MouseP);
#endif
    }
    else if(Input->MouseButtons[PlatformMouseButton_Right].HalfTransitionCount)
    {
#if 0 
        DrawDebugMainMenu(DebugState, MouseP);
        if(DebugState->HotIndex < ArrayCount(DebugVariableList))
        {
            DebugVariableList[DebugState->HotIndex].Value = !DebugVariableList[DebugState->HotIndex].Value;
            WriteHandmadeConfig(DebugState);
        }
#endif
    }
    //------------------------
    
    
    DEBUGTextLine("DEBUG CYCLE COUNTERS");
    if(DebugState->Compiling)
    {
        debug_process_state ProcessState = Platform.DEBUGGetProcessState(DebugState->Compiler);
        if(ProcessState.IsRunning)
        {
            DEBUGTextLine("COMPILING", Vector4(1.0f, 1.0f, 0, 1.0f));
        }
        else
        {
            DebugState->Compiling = false;
        }
    }
    
    {
        char TextBuffer[256];
        /*NOTE(Alex): This is calling 
        template <size_t size>  
        int _snprintf_s(  
        char (&buffer)[size],  
        size_t count,  
        const char *format [,  
        argument] ...   
        ); // C++ only  */
        
        debug_frame * LastFrame = GetLastCollatedFrame(DebugState);
        _snprintf_s(TextBuffer, sizeof(TextBuffer), "Milliseconds for frame: %f", 
                    LastFrame->SecondsElapsed * 1000.0f);
        DEBUGTextLine(TextBuffer);
        
        char CursorCoordBuf[256];
        _snprintf_s(CursorCoordBuf, sizeof(CursorCoordBuf),
                    "X:%10f Y:%10f", 
                    MouseP.x, 
                    MouseP.y);
        DEBUGTextLine(CursorCoordBuf);
    }
    
#if 0
    if(WasPressed(&Input->MouseButtons[PlatformMouseButton_Left]))
    {
        //// NOTE(Alex): Once we refresh we have to break since 
        //the frame and region data is not valid anymore
        if(HotRecord)
        {
            DebugState->ScopeRecord = HotRecord;
        }
        else
        {
            //TODO(Alex): Actually go one level up to parent block
            DebugState->ScopeRecord = 0;
        }
        
        RefreshCollation(DebugState);
    }
#endif
    
#if 0
    //NOTE: Function CounterState DEBUG Printing
    for(u32 CounterStateIndex = 0;
        CounterStateIndex < DebugState->CounterStateCount;
        ++CounterStateIndex)
    {
        debug_counter_state * CounterState = DebugState->CounterStates + CounterStateIndex;
        
        debug_stats HitStats = {};
        debug_stats CycleStats = {};
        debug_stats COHStats = {};
        
        BeginStats(&HitStats);
        BeginStats(&CycleStats);
        BeginStats(&COHStats);
        
        for(u32 Index = 0;
            Index < MAX_SNAPSHOT_COUNT;
            ++Index)
        {
            debug_counter_snapshot * Snapshot = CounterState->Snapshots + Index;
            
            AccumulateStats(&HitStats, (r32)Snapshot->HitCount);
            AccumulateStats(&CycleStats, (r32)Snapshot->CycleCount);
            
            r32 COH = 0.0f;
            if(Snapshot->HitCount != 0)
            {
                COH = (r32)Snapshot->CycleCount / (r32)Snapshot->HitCount;
            }
            
            AccumulateStats(&COHStats, COH);
        }
        
        EndStats(&HitStats);
        EndStats(&CycleStats);
        EndStats(&COHStats);
        
        if(CounterState->BlockName)
        {
            //NOTE: Lets try to draw cyclecount chart per function
            if(CycleStats.Max != 0)
            {
                r32 BarWidth = 4.0f;
                
                r32 ChartLeft = 40.0f;
                r32 ChartMinY = DebugState->AtY ;
                r32 ChartHeight = (HHA->AscenderHeight * DebugState->FontScale);
                
                r32 Scale = 1.0f / (r32)CycleStats.Max; 
                
                for(u32 StatIndex = 0;
                    StatIndex < MAX_SNAPSHOT_COUNT;
                    ++StatIndex)
                {
                    debug_counter_snapshot * Snapshot = CounterState->Snapshots + StatIndex;
                    r32 Proportion = Snapshot->CycleCount * Scale;
                    r32 Value = ChartHeight * Proportion;
                    
                    PushRect(DEBUGRenderGroup, 
                             Vector3(ChartLeft + (BarWidth * StatIndex) + 0.5f * BarWidth, ChartMinY + 0.5f * Value, 0), Vector2(BarWidth, Value), 
                             Vector4(1.0f, Proportion, 0.0f, 1.0f));
                }
            }
            
            
            char TextBuffer[256];
            
            _snprintf_s(TextBuffer, sizeof(TextBuffer),"%32s(%5u) Cy: %-10u H: %-10u Cy/H: %-10u\n", 
                        CounterState->BlockName, 
                        CounterState->LineNumber, 
                        (u32)CycleStats.Avg, 
                        (u32)HitStats.Avg, 
                        (u32)COHStats.Avg);
            
            DEBUGTextLine(TextBuffer);
        }
    }
#endif
    
    TiledRenderGroupToOutput(DebugState->HighPriorityQueue, DebugState->RenderGroup, DrawBuffer);
    EndRenderGroup(DebugState->RenderGroup);
    
    DebugState->NextHotInteraction = NullInteraction();
}

extern "C" GAME_DEBUG_FRAME_END(GameDEBUGFrameEnd)
{
#if HANDMADE_INTERNAL
    
    debug_state * DebugState = (debug_state*)Memory->DebugStorage;
    if(DebugState)
    {
        game_assets * Assets = DEBUGGetGameAssets(Memory);
        DEBUGStart(DebugState, Assets, Buffer->Width, Buffer->Height);
    }
    
    
    GlobalDebugTable->ZLevel = -Real32Max,
    ++GlobalDebugTable->CurrentEventArrayIndex;
    if(GlobalDebugTable->CurrentEventArrayIndex >= MAX_DEBUG_EVENT_ARRAY_COUNT)
    {
        GlobalDebugTable->CurrentEventArrayIndex = 0;
    }
    
    u64 DebugEventArrayIndex_DebugEventIndex = AtomicExchangeU64(&GlobalDebugTable->EventArrayIndex_EventIndex, (GlobalDebugTable->CurrentEventArrayIndex << 32));
    u32 EventArrayIndex = (u32)(DebugEventArrayIndex_DebugEventIndex >> 32); 
    u32 EventCount = DebugEventArrayIndex_DebugEventIndex & 0xFFFFFFFF; 
    
    GlobalDebugTable->EventCount[EventArrayIndex] = EventCount;
    
    if(DebugState)
    {
        if(Memory->ExecutableReloaded)
        {
            RestartCollation(DebugState);
        }
        if(!DebugState->FreezeCollation)
        {
            CollateDebugEvents(DebugState, (u32)GlobalDebugTable->CurrentEventArrayIndex);
        }
#if 0        
        //TODO(Alex): ClearScreen when on pause here?
        if(Globalpause)
        {
            Clear(DebugState->RenderGroup, Vector4(0.3f, 0.4f, 0.3f, 1.0f));
        }
#endif
        
        loaded_bitmap DrawBuffer_ = {};
        loaded_bitmap * DrawBuffer = &DrawBuffer_;
        DrawBuffer->Memory = Buffer->Memory;
        DrawBuffer->Width = Buffer->Width;
        DrawBuffer->Height = Buffer->Height;
        DrawBuffer->Pitch = Buffer->Pitch;
        
        DEBUGEnd(DebugState, DrawBuffer, Input, Memory);
    }
    
    
#endif
    
    return GlobalDebugTable; 
}

