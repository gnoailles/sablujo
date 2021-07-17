#ifndef HANDMADE_SSE_H

#define LANE_WIDTH 8

#if LANE_WIDTH == 8

#include "handmade_sse_lane8.h"

#elif LANE_WIDTH == 4

#include "handmade_sse_lane4.h"

#elif LANE_WIDTH == 1

using lane_v3 = vector3;
using lane_f32 = float;
using lane_i32 = int32_t;


global_variable lane_i32 LaneZeroI32 = 0;
global_variable lane_f32 LaneZeroF32 = 0.0f;

global_variable lane_i32 LaneOneI32 = 1;
global_variable lane_f32 LaneOneF32 = 1.0f;

inline float
GetLane(lane_f32 A, int32_t Lane)
{
    return A;
}

inline int32_t
GetLane(lane_i32 A, int32_t Lane)
{
    return A;
}

inline lane_i32
InitIncrementalLaneI32(int32_t Value)
{
    return Value;
}

inline lane_i32
InitLaneI32(int32_t Value)
{
    return Value;
}

inline lane_f32
InitLaneF32(float Value)
{
    return Value;
}


inline lane_v3
InitLaneV3(float X, float Y, float Z)
{
    lane_v3 Result;
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;
    Result.Padding = 0.0f;
    return Result;
}

inline lane_v3
LoadLaneV3(vector3* Values)
{
    lane_v3 Result;
    Result.X = Values[0].X;
    Result.Y = Values[0].Y;
    Result.Z = Values[0].Z;
    return Result;
}

inline int32_t
IsAllZeros(lane_i32 A)
{
    return A == 0;
}


inline void
ConditionalAssign(lane_i32 Source, lane_i32 *Dest, lane_i32 Mask)
{
    Mask = (Mask ? 0xFFFFFFFF : 0);
    *Dest = (-Mask & *Dest) | (Mask & Source);
}


inline lane_f32
Clamp(lane_f32 A, lane_f32 LowerBound, lane_f32 UpperBound)
{
    return MAX(MIN(A, UpperBound), LowerBound);
}


inline lane_f32
Min(lane_f32 A, lane_f32 Bound)
{
    return MIN(A, Bound);
}


inline lane_f32
RSquareRoot(lane_f32 A)
{
    return _mm_rsqrt_ps(_mm_set_ps1(A)).m128_f32[0];
}

inline lane_f32
Pow(lane_f32 A, float Power)
{
    return powf(A, Power);
}

inline float
Sum(lane_v3 Vector)
{
    return Vector.X + Vector.Y + Vector.Z;
}

inline float
MagnitudeSq(lane_v3 Vector)
{
    //Assert(Vector.Padding == 0.0f);
    Vector.vec = _mm_mul_ps(Vector.vec, Vector.vec);
    return Sum(Vector);
}



/*
inline float Sum(__m128 Vector)
{
    return Vector.m128_f32[0] + Vector.m128_f32[1] + Vector.m128_f32[2];
}*/


inline float 
Magnitude(lane_v3 Vector)
{
    return SquareRoot(MagnitudeSq(Vector));
}
/*
inline lane_v3
Normalize(lane_v3 Vector)
{
    float LengthSq = MagnitudeSq(Vector);
    if(LengthSq > 0.0000001f)
    {
        __m128 InvLength = _mm_rsqrt_ps(_mm_set_ps1(LengthSq));
        Vector.vec = _mm_mul_ps(Vector.vec, InvLength);
        Vector.Padding = 0.0f;
    }
    return Vector;
}
*/

/*
inline float
DotProduct(lane_v3 A, lane_v3 B)
{
    Assert(A.Padding == 0.0f || B.Padding == 0.0f);
    return _mm_dp_ps(A.vec, B.vec, 0xFF).m128_f32[0];
}
*/

// ADD
/*inline lane_v3
operator+(lane_v3 lhs, lane_v3 rhs)
{
    lane_v3 Result;
    Result.vec = _mm_add_ps(lhs.vec, rhs.vec);
    return Result;
}*/

// SUB
inline lane_v3
operator-(lane_v3 lhs, lane_v3 rhs)
{
    lane_v3 Result;
    Result.vec = _mm_sub_ps(lhs.vec, rhs.vec);
    return Result;
}

inline lane_v3
operator-(int32_t lhs, lane_v3 rhs)
{
    lane_v3 Result;
    Result.vec = _mm_sub_ps(_mm_set1_ps((float)lhs), rhs.vec);
    return Result;
}

// MULTIPLY
inline lane_v3
operator*(int32_t lhs, lane_v3 rhs)
{
    lane_v3 Result;
    Result.vec = _mm_mul_ps(_mm_set1_ps((float)lhs), rhs.vec);
    return Result;
}

/*
inline lane_v3
operator*(float lhs, lane_v3 rhs)
{
    lane_v3 Result;
    Result.vec = _mm_mul_ps(_mm_set1_ps(lhs), rhs.vec);
    return Result;
}
*/
inline lane_v3
operator*(lane_v3 lhs, float rhs)
{
    lane_v3 Result;
    Result.vec = _mm_mul_ps(lhs.vec, _mm_set1_ps(rhs));
    return Result;
}

inline void 
operator*=(lane_v3& lhs, float rhs)
{
    lhs.vec = _mm_mul_ps(lhs.vec, _mm_set1_ps(rhs));
}

inline lane_v3
operator*(lane_v3 lhs, lane_v3 rhs)
{
    lane_v3 Result;
    Result.vec = _mm_mul_ps(lhs.vec, rhs.vec);
    return Result;
}

#define CastToLaneI32(A) (*(int32_t*)&(A))
#define CastToLaneF32(A) (*(float*)&(A))

inline lane_f32
AndNot(lane_f32 A, lane_f32 B)
{
    lane_i32 ResultInt = (~CastToLaneI32(A)) & CastToLaneI32(B);
    return CastToLaneF32(ResultInt);
}
inline lane_f32
ConvertLaneI32ToF32(lane_i32 A)
{
    return (float)A;
}

#else
#error "Specified lane width not supported"
#endif

global_variable lane_f32 NormalizeThreshold = InitLaneF32(0.0000001f);
inline lane_v3
Normalize(lane_v3 A)
{
    lane_f32 LengthSq = MagnitudeSq(A);
    lane_f32 NormalizeMask = LengthSq > NormalizeThreshold;
    if(!IsAllZeros(CastToLaneI32(NormalizeMask)))
    {
        lane_f32 InvLengths = RSquareRoot(LengthSq);
        A.X = A.X * InvLengths;
        A.Y = A.Y * InvLengths;
        A.Z = A.Z * InvLengths;
    }
    return A;
}

inline lane_f32
DotProduct(lane_v3 A, lane_v3 B)
{
    return A.X *B.X + A.Y * B.Y + A.Z * B.Z;
}

//TODO: Replace this stupid and horrible implementation
inline lane_f32
Pow(lane_f32 A, uint32_t Power)
{
    lane_f32 Result = A;
    for(uint32_t i = 0; i < Power; ++i)
    {   
        Result = Result *  A;
    }
    return Result;
}

inline lane_f32
LinearToSRGB(lane_f32 A)
{
    lane_f32 Mask = (A < InitLaneF32(0.0031308f));
    lane_f32 SimpleScale = A * InitLaneF32(12.92f);
    lane_f32 ExponentialScale =  InitLaneF32(1.055f)* Pow(A, (1.0f / 2.4f));
    
    lane_f32 MaskedExpScale = AndNot(Mask, ExponentialScale);
    lane_i32 IntResult = CastToLaneI32(SimpleScale) & CastToLaneI32(Mask) | CastToLaneI32(MaskedExpScale);
    return CastToLaneF32(IntResult);
}


#define HANDMADE_SSE_H
#endif //HANDMADE_SSE_H
