#if !defined(HANDMADE_INTRINSICS_H)

//
// TODO(casey): Convert all of these to platform-efficient versions
// and remove math.h
//

#include "math.h"

inline int32
RoundReal32ToInt32(real32 Real32)
{
    int32 Result = (int32)roundf(Real32);
    return(Result);
}

inline uint32
RoundReal32ToUInt32(real32 Real32)
{
    uint32 Result = (uint32)roundf(Real32);
    return(Result);
}

inline int32 
FloorReal32ToInt32(real32 Real32)
{
    int32 Result = (int32)floorf(Real32);
    return(Result);
}

inline int32 CeilingFloatToInt32(float Value)
{
    int32 Result = (int32)ceilf(Value);
    return Result;
}

inline uint32 CeilingFloatToUInt32(float Value)
{
    uint32 Result = (uint32)ceilf(Value);
    return Result;
}

inline int32
TruncateReal32ToInt32(real32 Real32)
{
    int32 Result = (int32)Real32;
    return(Result);
}

inline real32
Sin(real32 Angle)
{
    real32 Result = sinf(Angle);
    return(Result);
}

inline real32
Cos(real32 Angle)
{
    real32 Result = cosf(Angle);
    return(Result);
}

inline real32
ATan2(real32 Y, real32 X)
{
    real32 Result = atan2f(Y, X);
    return(Result);
}

static uint32 FindFirstSetBitIndex(uint32 Value)
{
    uint32 Result = 0;
    for(uint32 Counter = 0; Counter < 32; ++Counter)
    {
        if(Value & (1 << Counter))
        {
            Result = Counter;
            break;
        }
    }
    return Result;
}

inline float AbsoluteValue(float Value)
{
    float Result = fabsf(Value);
    return Result;
}

inline float SquareRoot(float X)
{
    float Result = sqrtf(X);
    return Result;
}

inline int32 SignBit(int32 Value)
{
    int32 Result = (Value >= 0) ? 1 : -1;
    return Result;
}

#define HANDMADE_INTRINSICS_H
#endif
