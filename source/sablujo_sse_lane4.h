#ifndef SABLUJO_SSE_LANE4_H

#include <immintrin.h>

using lane_f32 = __m128;
using lane_i32 = __m128i;

struct lane_v3
{
    lane_f32 X;
    lane_f32 Y;
    lane_f32 Z;
};

/*
struct lane_f32
{
    __m128 Value;
}
*/
global_variable lane_i32 LaneZeroI32 = _mm_setzero_si128();
global_variable lane_f32 LaneZeroF32 = _mm_setzero_ps();

global_variable lane_i32 LaneOneI32 = _mm_set1_epi32(1);
global_variable lane_f32 LaneOneF32 = _mm_set_ps1(1.0f);
global_variable lane_f32 LaneZeroPointFive = _mm_set_ps1(0.5f);

inline lane_i32 
InitLaneI32(int32_t Value)
{
    return _mm_set1_epi32(Value);
}

inline lane_i32 
InitIncrementalLaneI32(int32_t BaseValue)
{
    return _mm_setr_epi32(BaseValue, BaseValue + 1, BaseValue + 2, BaseValue + 3);
}

inline lane_i32
LoadLaneI32(int32_t* Values)
{
    return _mm_setr_epi32(Values[0], Values[1], Values[2], Values[3]);
}

inline int32_t
IsAllZeros(lane_i32 A)
{
    return _mm_test_all_zeros(A, A);
}

inline lane_i32
operator-(lane_i32 A, lane_i32 B)
{
    return _mm_sub_epi32(A, B);
}

inline lane_i32
operator+(lane_i32 A, lane_i32 B)
{
    return _mm_add_epi32(A, B);
}

inline lane_i32
operator*(lane_i32 A, lane_i32 B)
{
    return _mm_mullo_epi32(A, B);
}

inline lane_i32
operator*(lane_i32 A, int32_t B)
{
    return _mm_mullo_epi32(A,InitLaneI32(B));
}

inline lane_i32
operator*(int32_t A, lane_i32 B)
{
    return B * A;
}
/*
inline lane_i32
operator/(lane_i32 A, lane_i32 B)
{
    return _mm_div_epi32(A, B);
}
*/
inline lane_i32
operator<(lane_i32 A, lane_i32 B)
{
    return _mm_cmplt_epi32(A, B);
}

inline lane_i32
operator|(lane_i32 A, lane_i32 B)
{
    return _mm_or_si128(A,B);
}

inline lane_i32
operator&(lane_i32 A, lane_i32 B)
{
    return _mm_and_si128(A,B);
}

inline lane_i32
operator<<(lane_i32 A, int32_t B)
{
    return _mm_slli_epi32(A, B);
}


inline lane_i32
operator>>(lane_i32 A, int32_t B)
{
    return _mm_srli_epi32(A, B);
}

inline lane_i32
AndNot(lane_i32 A, lane_i32 B)
{
    return _mm_andnot_si128(A,B);
}

inline void
ConditionalAssign(lane_i32 Source, lane_i32 *Dest, lane_i32 Mask)
{
    *Dest = AndNot(Mask, *Dest) | Mask & Source;
}

/////////////
// Lane F32
/////////////

inline lane_f32 
InitLaneF32(float Value)
{
    return _mm_set_ps1(Value);
}


inline float
GetLane(lane_f32 A, int32_t Lane)
{
    return A.m128_f32[Lane];
}

inline int32_t
GetLane(lane_i32 A, int32_t Lane)
{
    return A.m128i_i32[Lane];
}


inline lane_f32
RSquareRoot(lane_f32 A)
{
    return _mm_rsqrt_ps(A);
}

inline lane_f32
Min(lane_f32 A, lane_f32 Bound)
{
    return _mm_min_ps(A, Bound);
}

inline lane_f32
Max(lane_f32 A, lane_f32 Bound)
{
    return _mm_max_ps(A, Bound);
}

inline lane_f32
Clamp(lane_f32 A, lane_f32 LowerBound, lane_f32 UpperBound)
{
    return _mm_max_ps(_mm_min_ps(A, UpperBound), LowerBound);
}

inline lane_f32
MultiplyAdd(lane_f32 A, lane_f32 B, lane_f32 C)
{
    return _mm_fmadd_ps(A, B, C);
}

inline lane_f32
operator+(lane_f32 A, lane_f32 B)
{
    return _mm_add_ps(A,B);
}

inline lane_f32
operator-(lane_f32 A, lane_f32 B)
{
    return _mm_sub_ps(A,B);
}

inline lane_f32
operator*(lane_f32 A, lane_f32 B)
{
    return _mm_mul_ps(A,B);
}

inline lane_f32
operator/(lane_f32 A, lane_f32 B)
{
    return _mm_div_ps(A,B);
}

inline lane_f32
operator<(lane_f32 A, lane_f32 B)
{
    return  _mm_cmplt_ps(A,B);
}

inline lane_f32
operator<=(lane_f32 A, lane_f32 B)
{
    return  _mm_cmple_ps(A,B);
}

inline lane_f32
operator>(lane_f32 A, lane_f32 B)
{
    return  _mm_cmpgt_ps(A,B);
}

inline lane_f32
operator&(lane_f32 A, lane_f32 B)
{
    return  _mm_and_ps(A,B);
}

inline lane_f32
operator|(lane_f32 A, lane_f32 B)
{
    return  _mm_or_ps(A,B);
}

inline lane_f32
And(lane_f32 A, lane_f32 B)
{
    return  _mm_and_ps(A,B);
}

inline lane_f32
AndNot(lane_f32 A, lane_f32 B)
{
    return  _mm_andnot_ps(A,B);
}

inline lane_f32
Or(lane_f32 A, lane_f32 B)
{
    return  _mm_or_ps(A,B);
}
/////////////
// Lane V3
/////////////

inline lane_v3
InitLaneV3(float X, float Y, float Z)
{
    lane_v3 Result;
    Result.X = _mm_set_ps1(X);
    Result.Y = _mm_set_ps1(Y);
    Result.Z = _mm_set_ps1(Z);
    return Result;
}

inline lane_v3
LoadLaneV3(vector3* Values)
{
    lane_v3 Result;
    Result.X = _mm_setr_ps(Values[0].X, Values[1].X, Values[2].X, Values[3].X);
    Result.Y = _mm_setr_ps(Values[0].Y, Values[1].Y, Values[2].Y, Values[3].Y);
    Result.Z = _mm_setr_ps(Values[0].Z, Values[1].Z, Values[2].Z, Values[3].Z);
    return Result;
}

inline lane_f32
MagnitudeSq(lane_v3 A)
{
    lane_f32 Result;
    Result = A.X * A.X + A.Y * A.Y + A.Z * A.Z;
    return Result;
}

inline lane_v3
operator+(lane_v3 A, lane_v3 B)
{
    A.X = _mm_add_ps(A.X, B.X);
    A.Y = _mm_add_ps(A.Y, B.Y);
    A.Z = _mm_add_ps(A.Z, B.Z);
    return A;
}

inline lane_v3
operator-(lane_v3 A, lane_v3 B)
{
    A.X = _mm_sub_ps(A.X, B.X);
    A.Y = _mm_sub_ps(A.Y, B.Y);
    A.Z = _mm_sub_ps(A.Z, B.Z);
    return A;
}


inline lane_v3
operator*(lane_v3 A, lane_f32 B)
{
    A.X = _mm_mul_ps(A.X, B);
    A.Y = _mm_mul_ps(A.Y, B);
    A.Z = _mm_mul_ps(A.Z, B);
    return A;
}

////////////////////////
// Casts & Conversions
////////////////////////

inline lane_i32
ConvertLaneF32ToI32(lane_f32 A)
{
    return _mm_cvtps_epi32(A);
}

inline lane_f32
ConvertLaneI32ToF32(lane_i32 A)
{
    return _mm_cvtepi32_ps(A);
}

inline lane_i32
CastLaneF32ToI32(lane_f32 A)
{
    return _mm_castps_si128(A);
}

inline lane_f32
CastLaneI32ToF32(lane_i32 A)
{
    return _mm_castsi128_ps(A);
}

#define SABLUJO_SSE_LANE4_H
#endif //SABLUJO_SSE_LANE4_H
