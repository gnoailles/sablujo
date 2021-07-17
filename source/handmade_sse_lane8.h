#ifndef HANDMADE_SSE_LANE8_H

#include <immintrin.h>

using lane_f32 = __m256;
using lane_i32 = __m256i;

struct lane_v3
{
    lane_f32 X;
    lane_f32 Y;
    lane_f32 Z;
};

global_variable lane_i32 LaneZeroI32 = _mm256_setzero_si256();
global_variable lane_f32 LaneZeroF32 = _mm256_setzero_ps();

global_variable lane_i32 LaneOneI32 = _mm256_set1_epi32(1);
global_variable lane_f32 LaneOneF32 = _mm256_set1_ps(1.0f);
global_variable lane_f32 LaneZeroPointFive = _mm256_set1_ps(0.5f);

inline lane_i32 
InitLaneI32(int32_t Value)
{
    return _mm256_set1_epi32(Value);
}

inline lane_i32 
InitIncrementalLaneI32(int32_t BaseValue)
{
    return _mm256_setr_epi32(BaseValue, BaseValue + 1, BaseValue + 2, BaseValue + 3,  BaseValue + 4,  BaseValue + 5,  BaseValue + 6,  BaseValue + 7);
}

inline lane_i32
LoadLaneI32(int32_t* Values)
{
    return _mm256_setr_epi32(Values[0], Values[1], Values[2], Values[3],
                             Values[4], Values[5], Values[6], Values[7]);
}

inline int32_t
IsAllZeros(lane_i32 A)
{
    return _mm256_testz_si256(A, A);
}

inline lane_i32
operator+(lane_i32 A, lane_i32 B)
{
    return _mm256_add_epi32(A, B);
}

inline lane_i32
operator-(lane_i32 A, lane_i32 B)
{
    return _mm256_sub_epi32(A, B);
}

inline lane_i32
operator*(lane_i32 A, int32_t B)
{
    return _mm256_mullo_epi32(A, InitLaneI32(B));
}

inline lane_i32
operator*(int32_t A, lane_i32 B)
{
    return B * A;
}

inline lane_i32
operator*(lane_i32 A, lane_i32 B)
{
    return _mm256_mullo_epi32(A, B);
}

inline lane_i32
operator<(lane_i32 A, lane_i32 B)
{
    return _mm256_cmpgt_epi32(B, A);
}

inline lane_i32
operator|(lane_i32 A, lane_i32 B)
{
    return _mm256_or_si256(A,B);
}

inline lane_i32
operator&(lane_i32 A, lane_i32 B)
{
    return _mm256_and_si256(A,B);
}

inline lane_i32
AndNot(lane_i32 A, lane_i32 B)
{
    return _mm256_andnot_si256(A,B);
}

inline lane_i32
operator<<(lane_i32 A, int32_t B)
{
    return _mm256_slli_epi32(A, B);
}


inline lane_i32
operator>>(lane_i32 A, int32_t B)
{
    return _mm256_srli_epi32(A, B);
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
    return _mm256_set1_ps(Value);
}


inline float
GetLane(lane_f32 A, int32_t Lane)
{
    return A.m256_f32[Lane];
}

inline int32_t
GetLane(lane_i32 A, int32_t Lane)
{
    return A.m256i_i32[Lane];
}


inline lane_f32
RSquareRoot(lane_f32 A)
{
    return _mm256_rsqrt_ps(A);
}

inline lane_f32
Min(lane_f32 A, lane_f32 Bound)
{
    return _mm256_min_ps(A, Bound);
}

inline lane_f32
Max(lane_f32 A, lane_f32 Bound)
{
    return _mm256_max_ps(A, Bound);
}

inline lane_f32
Clamp(lane_f32 A, lane_f32 LowerBound, lane_f32 UpperBound)
{
    return _mm256_max_ps(_mm256_min_ps(A, UpperBound), LowerBound);
}

inline lane_f32
MultiplyAdd(lane_f32 A, lane_f32 B, lane_f32 C)
{
    return _mm256_fmadd_ps(A, B, C);
}

/*
inline lane_f32
Pow(lane_f32 A, float Power)
{
    A.m256_f32[0] = powf(A.m256_f32[0], Power);
    A.m256_f32[1] = powf(A.m256_f32[1], Power);
    A.m256_f32[2] = powf(A.m256_f32[2], Power);
    A.m256_f32[3] = powf(A.m256_f32[3], Power);
    A.m256_f32[4] = powf(A.m256_f32[4], Power);
    A.m256_f32[5] = powf(A.m256_f32[5], Power);
    A.m256_f32[6] = powf(A.m256_f32[6], Power);
    A.m256_f32[7] = powf(A.m256_f32[7], Power);
    return A;
}
*/

inline lane_f32
operator+(lane_f32 A, lane_f32 B)
{
    return _mm256_add_ps(A,B);
}

inline lane_f32
operator-(lane_f32 A, lane_f32 B)
{
    return _mm256_sub_ps(A,B);
}

inline lane_f32
operator*(lane_f32 A, lane_f32 B)
{
    return _mm256_mul_ps(A,B);
}

inline lane_f32
operator/(lane_f32 A, lane_f32 B)
{
    return _mm256_div_ps(A,B);
}

inline lane_f32
operator<(lane_f32 A, lane_f32 B)
{
    return _mm256_cmp_ps(A,B, _CMP_LT_OQ);
}

inline lane_f32
operator<=(lane_f32 A, lane_f32 B)
{
    return _mm256_cmp_ps(A,B, _CMP_LE_OQ);
}

inline lane_f32
operator>(lane_f32 A, lane_f32 B)
{
    return _mm256_cmp_ps(A,B, _CMP_GT_OQ);
}

inline lane_f32
operator&(lane_f32 A, lane_f32 B)
{
    return  _mm256_and_ps(A,B);
}

inline lane_f32
operator|(lane_f32 A, lane_f32 B)
{
    return  _mm256_or_ps(A,B);
}


inline lane_f32
AndNot(lane_f32 A, lane_f32 B)
{
    return  _mm256_andnot_ps(A,B);
}


/////////////
// Lane V3
/////////////

inline lane_v3
InitLaneV3(float X, float Y, float Z)
{
    lane_v3 Result;
    Result.X = _mm256_set1_ps(X);
    Result.Y = _mm256_set1_ps(Y);
    Result.Z = _mm256_set1_ps(Z);
    return Result;
}

inline lane_v3
LoadLaneV3(vector3* Values)
{
    lane_v3 Result;
    Result.X = _mm256_setr_ps(Values[0].X, Values[1].X, Values[2].X, Values[3].X, Values[4].X, Values[5].X, Values[6].X, Values[7].X);
    Result.Y = _mm256_setr_ps(Values[0].Y, Values[1].Y, Values[2].Y, Values[3].Y, Values[4].Y, Values[5].Y, Values[6].Y, Values[7].Y);
    Result.Z = _mm256_setr_ps(Values[0].Z, Values[1].Z, Values[2].Z, Values[3].Z, Values[4].Z, Values[5].Z, Values[6].Z, Values[7].Z);
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
    A.X = _mm256_add_ps(A.X, B.X);
    A.Y = _mm256_add_ps(A.Y, B.Y);
    A.Z = _mm256_add_ps(A.Z, B.Z);
    return A;
}

inline lane_v3
operator-(lane_v3 A, lane_v3 B)
{
    A.X = _mm256_sub_ps(A.X, B.X);
    A.Y = _mm256_sub_ps(A.Y, B.Y);
    A.Z = _mm256_sub_ps(A.Z, B.Z);
    return A;
}


inline lane_v3
operator*(lane_v3 A, lane_f32 B)
{
    A.X = _mm256_mul_ps(A.X, B);
    A.Y = _mm256_mul_ps(A.Y, B);
    A.Z = _mm256_mul_ps(A.Z, B);
    return A;
}

////////////////////////
// Casts & Conversions
////////////////////////

inline lane_f32
ConvertLaneI32ToF32(lane_i32 A)
{
    return _mm256_cvtepi32_ps(A);
}


inline lane_i32
ConvertLaneF32ToI32(lane_f32 A)
{
    return _mm256_cvtps_epi32(A);
}


inline lane_i32
CastLaneF32ToI32(lane_f32 A)
{
    return _mm256_castps_si256(A);
}

inline lane_f32
CastLaneI32ToF32(lane_i32 A)
{
    return _mm256_castsi256_ps(A);
}
/*
#define CastToLaneI32(A) (*(__m256i*)&(A))
#define CastToLaneF32(A) (*(__m256*)&(A))
*/
#define HANDMADE_SSE_LANE8_H
#endif //HANDMADE_SSE_LANE8_H
