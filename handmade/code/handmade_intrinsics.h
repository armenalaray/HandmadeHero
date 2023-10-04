#ifndef HANDMADE_INTRINSICS_H

#include "Math.h"


inline real32  
SignOf(real32 Value)
{
    real32 Result = (Value >= 0.0f) ? 1.0f : -1.0f;
    return Result;
}

inline uint32
RotateLeft(uint32 Value, int32 Shift)
{
#if COMPILER_MSVC
    uint32 Result = _rotl(Value, Shift);
#else
    Shift &= 31;
    uint32 Result = ((Value << Shift) | (Value >> (32 - Shift)));
#endif
    return Result;
}


inline uint32
RotateRight(uint32 Value, int32 Shift)
{
#if COMPILER_MSVC
    uint32 Result = _rotr(Value, Shift);
#else
    Shift &= 31;
    uint32 Result = (Value >> Shift)| (Value << (32 - Shift));
#endif
    return Result;
}

inline real32 
AbsValue(real32 Real32)
{
    real32 Result = (real32)fabs(Real32);
    return Result;	
}

inline int32 
RoundReal32ToInt32(real32 Real32)
{
    int32 Result = (int32)roundf(Real32);
    return Result;
}

inline int32 
RoundReal32ToUInt32(real32 Real32)
{
    uint32 Result = (uint32)roundf(Real32);
    return Result;
}	

inline int32 
FloorReal32ToInt32(real32 Real32)
{
    int32 Result = (int32)floorf(Real32);
    return Result; 
}

inline int32 
CeilReal32ToInt32(real32 Real32)
{
    int32 Result = (int32)ceilf(Real32);
    return Result; 
}

inline int32 
TruncateReal32ToInt32(real32 Real32)
{
    int32 Result = (int32)Real32;
    return Result;
}

inline real32 
SquareRoot(real32 Real32)
{
    real32 Result = sqrtf(Real32);
    return Result;
}

inline real32 
Sin(real32 Angle)
{
    real32 Result = sinf(Angle);
    return Result;
}

inline real32 
Cos(real32 Angle)
{
    real32 Result = cosf(Angle);
    return Result;
}

inline real32 
ATan2(real32 Y,real32 X )
{
    
    real32 Result = atan2f(Y,X);
    return Result;
}



struct bit_scan_result  
{
    bool32 Found;
    uint32 Index;
};

inline bit_scan_result 
FindLeastSignificandSetBit(uint32 Value)
{
    bit_scan_result Result = {};
    
#if COMPILER_MSVC
    
    Result.Found = _BitScanForward((unsigned long *)&Result.Index, Value);
    
#else	
    for (uint32 BitCount = 0; BitCount < 32; ++BitCount)
    {
        if((1 << BitCount) & Value)
        {
            //Least significand bit found
            Result.Index = BitCount;
            Result.Found = true;
            break;
        }
    }
    
    
#endif
    return Result;
}

#define HANDMADE_INTRINSICS_H
#endif