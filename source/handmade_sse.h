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
global_variable lane_f32 LaneZeroPointFive = 0.5f;

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

inline lane_i32
LoadLaneI32(int32_t* Values)
{
    lane_i32 Result;
    Result = *Values;
    return Result;
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
Max(lane_f32 A, lane_f32 Bound)
{
    return MAX(A, Bound);
}


inline lane_f32
RSquareRoot(lane_f32 A)
{
    return _mm_rsqrt_ps(_mm_set_ps1(A)).m128_f32[0];
}

/*
inline lane_f32
Pow(lane_f32 A, float Power)
{
    return powf(A, Power);
}
*/

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

#define CastLaneF32ToI32(A) (*(int32_t*)&(A))
#define CastLaneI32ToF32(A) (*(float*)&(A))


inline lane_f32
And(lane_f32& A, lane_f32 B)
{
    lane_i32 Result = CastLaneF32ToI32(A) & CastLaneF32ToI32(B);
    return CastLaneI32ToF32(Result);
}


inline lane_f32
AndNot(lane_f32 A, lane_f32 B)
{
    lane_i32 ResultInt = (~CastLaneF32ToI32(A)) & CastLaneF32ToI32(B);
    return CastLaneI32ToF32(ResultInt);
}

inline lane_f32
Or(lane_f32& A, lane_f32 B)
{
    lane_i32 Result = CastLaneF32ToI32(A) | CastLaneF32ToI32(B);
    return CastLaneI32ToF32(Result);
}

inline lane_f32
ConvertLaneI32ToF32(lane_i32 A)
{
    return (float)A;
}

inline lane_i32
ConvertLaneF32ToI32(lane_f32 A)
{
    return (int32_t)A;
}


inline lane_f32
MultiplyAdd(lane_f32 A, lane_f32 B, lane_f32 C)
{
    return A * B + C;
}
#else
#error "Specified lane width not supported"
#endif


////////////////////
// Common Functions
////////////////////
#if LANE_WIDTH != 1
inline void
operator+=(lane_i32& A, lane_i32 B)
{
    A = A + B;
}

inline void
operator+=(lane_f32& A, lane_f32 B)
{
    A = A + B;
}

inline void
operator-=(lane_i32& A, lane_i32 B)
{
    A = A - B;
}

inline void
operator-=(lane_f32& A, lane_f32 B)
{
    A = A - B;
}

inline void
operator*=(lane_i32& A, lane_i32 B)
{
    A = A * B;
}

inline void
operator*=(lane_f32& A, lane_f32 B)
{
    A = A * B;
}
/*
inline void
operator/=(lane_i32& A, lane_i32 B)
{
    A = A / B;
}
*/
inline void
operator/=(lane_f32& A, lane_f32 B)
{
    A = A / B;
}

inline void
operator&=(lane_f32& A, lane_f32 B)
{
    A = A & B;
}

inline void
operator|=(lane_f32& A, lane_f32 B)
{
    A = A | B;
}
#endif


global_variable lane_f32 NormalizeThreshold = InitLaneF32(0.0000001f);
inline lane_v3
Normalize(lane_v3 A)
{
    lane_f32 LengthSq = MagnitudeSq(A);
    lane_f32 NormalizeMask = LengthSq > NormalizeThreshold;
    if(!IsAllZeros(CastLaneF32ToI32(NormalizeMask)))
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


global_variable lane_i32 Const0x7F = InitLaneI32(0x7F);

global_variable lane_f32 ExpHi = InitLaneF32(88.3762626647949f);
global_variable lane_f32 ExpLo = InitLaneF32(-88.3762626647949f);

global_variable lane_f32 LOG2EF = InitLaneF32(1.44269504088896341f);
global_variable lane_f32 ExpC1 = InitLaneF32(0.693359375f);
global_variable lane_f32 ExpC2 = InitLaneF32(-2.12194440e-4f);

global_variable lane_f32 ExpP0 = InitLaneF32(1.9875691500E-4f);
global_variable lane_f32 ExpP1 = InitLaneF32(1.3981999507E-3f);
global_variable lane_f32 ExpP2 = InitLaneF32(8.3334519073E-3f);
global_variable lane_f32 ExpP3 = InitLaneF32(4.1665795894E-2f);
global_variable lane_f32 ExpP4 = InitLaneF32(1.6666665459E-1f);
global_variable lane_f32 ExpP5 = InitLaneF32(5.0000001201E-1f);

inline lane_f32 fast_exp(lane_f32 Value) 
{
    lane_f32 Result;
    
    // clamp to safe range
    Value = Clamp(Value, ExpLo, ExpHi);
    
    // Express exp(x) as exp(g + n*log(2))
    //TODO: use fma?
    lane_f32 Fx = Value * LOG2EF; //_mm_mul_ps(Value,LOG2EF);
    Fx += LaneZeroPointFive; //_mm_add_ps(Fx, ZeroPointFive);
    // Floor
    lane_i32 M = ConvertLaneF32ToI32(Value);
    lane_f32 Temp = ConvertLaneI32ToF32(M);
    
    // If greater, substract one
    lane_f32 Mask = Temp > Fx; //_mm_cmpgt_ps(Temp, Fx);    
    Mask = And(Mask, LaneOneF32); //_mm_and_ps(Mask, One);
    Fx = Temp - Mask; //_mm_sub_ps(Temp, Mask);
    
    Temp = Fx * ExpC1; //_mm_mul_ps(Fx, ExpC1);
    lane_f32 Z = Fx * ExpC2;//_mm_mul_ps(Fx, ExpC2);
    Value -= Temp; //_mm_sub_ps(Value, Temp);
    Value -= Z; // _mm_sub_ps(Value, Z);
    
    Z = Value * Value; // _mm_mul_ps(Value, Value);
    
    Result = ExpP0;
    Result = MultiplyAdd(Result, Value, ExpP1);
    Result = MultiplyAdd(Result, Value, ExpP2);
    Result = MultiplyAdd(Result, Value, ExpP3);
    Result = MultiplyAdd(Result, Value, ExpP4);
    Result = MultiplyAdd(Result, Value, ExpP5);
    Result = MultiplyAdd(Result, Z, Value);
    Result += LaneOneF32;
    
    // build 2^n
    M = ConvertLaneF32ToI32(Fx);
    M += Const0x7F;// _mm_add_epi32(M, Const0x7F);
    M = M << 23; // _mm_slli_epi32(M, 23);
    lane_f32 Pow2n = CastLaneI32ToF32(M);
    
    Result *= Pow2n;
    return Result;
}


/*
 * Taken from https://github.com/OpenImageIO/oiio/blob/6835a0cd72ca628dcef7c3acb5f4a6af9edc6093/src/include/OpenImageIO/fmath.h */

//global_variable __m128 One = _mm_set_ps1(1.0f);
// The smallest non denormalized float number
global_variable int32_t MinNormPosBit = 0x00800000;
global_variable lane_f32 MinNormPos = InitLaneF32(*(float*)&MinNormPosBit);
global_variable int32_t InverseMantissaBitMask = ~0x7f800000;
global_variable lane_f32 InverseMantissaMask = InitLaneF32(*(float*)&InverseMantissaBitMask);

global_variable lane_f32 SQRTHF = InitLaneF32(0.707106781186547524f);
global_variable lane_f32 LogP0 = InitLaneF32(7.0376836292E-2f);
global_variable lane_f32 LogP1 = InitLaneF32(-1.1514610310E-1f);
global_variable lane_f32 LogP2 = InitLaneF32(1.1676998740E-1f);
global_variable lane_f32 LogP3 = InitLaneF32(-1.2420140846E-1f);
global_variable lane_f32 LogP4 = InitLaneF32(1.4249322787E-1f);
global_variable lane_f32 LogP5 = InitLaneF32(-1.6668057665E-1f);
global_variable lane_f32 LogP6 = InitLaneF32(2.0000714765E-1f);
global_variable lane_f32 LogP7 = InitLaneF32(-2.4999993993E-1f);
global_variable lane_f32 LogP8 = InitLaneF32(3.3333331174E-1f);

global_variable lane_f32 LogQ1 = InitLaneF32(-2.12194440E-4f);
global_variable lane_f32 LogQ2 = InitLaneF32(0.693359375f);

// Natural logarithm computed for 4 simultaneous float
// return NaN for Value <= 0
inline lane_f32 fast_log(lane_f32 Value) 
{
    lane_f32 Result;
    
    lane_f32 InvalidMask = Value <= LaneZeroF32;
    Result = Max(Value, MinNormPos);  // cut off denormalized stuff
    
    // part 1: Value = frexpf(Value, &e);
    lane_i32 M = CastLaneF32ToI32(Result) >> 23;
    // keep only the fractional part
    Result = And(Result, InverseMantissaMask);
    Result = Or(Result, LaneZeroPointFive);
    
    // now e contain the real base-2 exponent
    M -= Const0x7F;
    lane_f32 e = ConvertLaneI32ToF32(M);
    
    e += LaneOneF32;
    
    // part2: 
    // if( x < SQRTHF ) 
    // {
    //     e -= 1;
    //     x = x + x - 1.0;
    // } 
    // else 
    // { x = x - 1.0; }
    
    lane_f32 Mask = Result < SQRTHF;
    lane_f32 Temp = And(Result, Mask);
    Result -= LaneOneF32;
    e -= And(LaneOneF32, Mask);
    Result += Temp;
    
    lane_f32 Z = Result * Result;
    
    lane_f32 Y = LogP0;
    Y = MultiplyAdd(Y, Result, LogP1);
    Y = MultiplyAdd(Y, Result, LogP2);
    Y = MultiplyAdd(Y, Result, LogP3);
    Y = MultiplyAdd(Y, Result, LogP4);
    Y = MultiplyAdd(Y, Result, LogP5);
    Y = MultiplyAdd(Y, Result, LogP6);
    Y = MultiplyAdd(Y, Result, LogP7);
    Y = MultiplyAdd(Y, Result, LogP8);
    Y *= Result;
    
    Y *= Z;
    
    Temp = e * LogQ1;
    Y += Temp;
    
    Temp = Z * LaneZeroPointFive;
    Y -= Temp;
    
    Temp = e * LogQ2;
    Result += Y;
    Result += Temp;
    
    
    Result = Or(Result, InvalidMask); // negative arg will be NAN
    
    return Result;
}


inline lane_f32
Pow(lane_f32 A, float Power)
{
    return fast_exp(InitLaneF32(Power) * fast_log(A));
}


//TODO: Replace this stupid and horrible implementation
inline lane_f32
Pow(lane_f32 A, uint32_t Power)
{
#if 0
    return fast_exp(InitLaneF32((float)Power) * fast_log(A));
#else
    lane_f32 Result = A;
    for(uint32_t i = 0; i < Power; ++i)
    {   
        Result = Result *  A;
    }
    return Result;
#endif
}

global_variable lane_f32 SRGBThreshold = InitLaneF32(0.0031308f);
global_variable lane_f32 SRGBScale = InitLaneF32(12.92f);
global_variable lane_f32 SRGBExponentScale = InitLaneF32(1.055f);
global_variable float  SRGBExponent = (1.0f / 2.4f);

inline lane_f32
LinearToSRGB(lane_f32 A)
{
    lane_f32 Mask = (A < SRGBThreshold);
    lane_f32 SimpleScale = A * SRGBScale;
    lane_f32 ExponentialScale =  SRGBExponentScale * Pow(A, SRGBExponent);
    
    lane_f32 MaskedExpScale = AndNot(Mask, ExponentialScale);
    lane_i32 IntResult = CastLaneF32ToI32(SimpleScale) & CastLaneF32ToI32(Mask) | CastLaneF32ToI32(MaskedExpScale);
    return CastLaneI32ToF32(IntResult);
}


#define HANDMADE_SSE_H
#endif //HANDMADE_SSE_H
