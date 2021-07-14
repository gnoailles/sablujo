#if !defined(HANDMADE_MATHS_H)

#define PI_FLOAT 3.141592653589793238463f
#define PI 3.141592653589793238463

#ifndef MAX
#define MAX(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#include <immintrin.h>
#include <stdint.h>
//TODO: Replace call to math.h
#include <math.h>

#include "handmade_defines.h"

struct matrix4
{
    union
    {
        float val[4][4];
        __m128 vecs[4];
    };
};

struct vector3
{
    union
    {
        struct  
        {
            float X;
            float Y;
            float Z;
            float Padding;
        };
        __m128 vec;
    };
    
};


struct vector4
{
    union
    {
        struct  
        {
            float X;
            float Y;
            float Z;
            float W;
        };
        __m128 vec;
    };
    
};

struct vector4i
{ 
    union
    {
        struct  
        {
            int32_t X;
            int32_t Y;
            int32_t Z;
            int32_t W;
        };
        __m128i vec;
    };
};

struct vector2
{
    float X;
    float Y;
};

struct vector2u
{
    uint32_t X;
    uint32_t Y;
};

struct vector2i
{
    int32_t X;
    int32_t Y;
};

inline float SquareRoot(float Value)
{
    return _mm_sqrt_ps(_mm_set_ps1(Value)).m128_f32[0];
}

/*
 * Taken from https://github.com/OpenImageIO/oiio/blob/6835a0cd72ca628dcef7c3acb5f4a6af9edc6093/src/include/OpenImageIO/fmath.h */
global_variable __m128 ZeroPointFive = _mm_set_ps1(0.5f);
global_variable __m128 One = _mm_set_ps1(1.0f);
global_variable __m128i Const0x7F = _mm_set1_epi32(0x7F);
// The smallest non denormalized float number
global_variable int32_t MinNormPosBit = 0x00800000;
global_variable __m128 MinNormPos = _mm_load_ps1((float*)&MinNormPosBit);
global_variable int32_t InverseMantissaBitMask = ~0x7f800000;
global_variable __m128 InverseMantissaMask = _mm_load_ps1((float*)&InverseMantissaBitMask);

global_variable __m128 ExpHi = _mm_set_ps1(88.3762626647949f);
global_variable __m128 ExpLo = _mm_set_ps1(-88.3762626647949f);

global_variable __m128 LOG2EF = _mm_set_ps1(1.44269504088896341f);
global_variable __m128 ExpC1 = _mm_set_ps1(0.693359375f);
global_variable __m128 ExpC2 = _mm_set_ps1(-2.12194440e-4f);

global_variable __m128 ExpP0 = _mm_set_ps1(1.9875691500E-4f);
global_variable __m128 ExpP1 = _mm_set_ps1(1.3981999507E-3f);
global_variable __m128 ExpP2 = _mm_set_ps1(8.3334519073E-3f);
global_variable __m128 ExpP3 = _mm_set_ps1(4.1665795894E-2f);
global_variable __m128 ExpP4 = _mm_set_ps1(1.6666665459E-1f);
global_variable __m128 ExpP5 = _mm_set_ps1(5.0000001201E-1f);



inline vector4 fast_exp(__m128 Value) 
{
    vector4 Result;
    
    // clamp to safe range
    Value = _mm_min_ps(Value, ExpHi);
    Value = _mm_max_ps(Value, ExpLo);
    
    // Express exp(x) as exp(g + n*log(2))
    //TODO: use fma?
    __m128 Fx = _mm_mul_ps(Value,LOG2EF);
    Fx = _mm_add_ps(Fx, ZeroPointFive);
    // Floor
    __m128i M = _mm_cvtps_epi32(Value);
    __m128 Temp = _mm_cvtepi32_ps(M);
    
    // If greater, substract one
    __m128 Mask = _mm_cmpgt_ps(Temp, Fx);    
    Mask = _mm_and_ps(Mask, One);
    Fx = _mm_sub_ps(Temp, Mask);
    
    Temp = _mm_mul_ps(Fx, ExpC1);
    __m128 Z = _mm_mul_ps(Fx, ExpC2);
    Value = _mm_sub_ps(Value, Temp);
    Value = _mm_sub_ps(Value, Z);
    
    Z = _mm_mul_ps(Value, Value);
    
    Result.vec = ExpP0;
    Result.vec = _mm_fmadd_ps(Result.vec, Value, ExpP1);
    Result.vec = _mm_fmadd_ps(Result.vec, Value, ExpP2);
    Result.vec = _mm_fmadd_ps(Result.vec, Value, ExpP3);
    Result.vec = _mm_fmadd_ps(Result.vec, Value, ExpP4);
    Result.vec = _mm_fmadd_ps(Result.vec, Value, ExpP5);
    Result.vec = _mm_fmadd_ps(Result.vec, Z, Value);
    Result.vec = _mm_add_ps(Result.vec, One);
    
    // build 2^n
    M = _mm_cvttps_epi32(Fx);
    M = _mm_add_epi32(M, Const0x7F);
    M = _mm_slli_epi32(M, 23);
    __m128 Pow2n = _mm_castsi128_ps(M);
    
    Result.vec = _mm_mul_ps(Result.vec, Pow2n);
    return Result;
}

global_variable __m128 SQRTHF = _mm_set_ps1(0.707106781186547524f);
global_variable __m128 LogP0 = _mm_set_ps1(7.0376836292E-2f);
global_variable __m128 LogP1 = _mm_set_ps1(-1.1514610310E-1f);
global_variable __m128 LogP2 = _mm_set_ps1(1.1676998740E-1f);
global_variable __m128 LogP3 = _mm_set_ps1(-1.2420140846E-1f);
global_variable __m128 LogP4 = _mm_set_ps1(1.4249322787E-1f);
global_variable __m128 LogP5 = _mm_set_ps1(-1.6668057665E-1f);
global_variable __m128 LogP6 = _mm_set_ps1(2.0000714765E-1f);
global_variable __m128 LogP7 = _mm_set_ps1(-2.4999993993E-1f);
global_variable __m128 LogP8 = _mm_set_ps1(3.3333331174E-1f);

global_variable __m128 LogQ1 = _mm_set_ps1(-2.12194440E-4f);
global_variable __m128 LogQ2 = _mm_set_ps1(0.693359375f);

// Natural logarithm computed for 4 simultaneous float
// return NaN for Value <= 0
inline vector4 fast_log(vector4 Value) 
{
    vector4 Result;
    
    __m128 InvalidMask = _mm_cmple_ps(Value.vec, _mm_setzero_ps());
    Result.vec = _mm_max_ps(Value.vec, MinNormPos);  // cut off denormalized stuff
    
    // part 1: Value = frexpf(Value, &e);
    __m128i M = _mm_srli_epi32(_mm_castps_si128(Result.vec), 23);
    // keep only the fractional part
    Result.vec = _mm_and_ps(Result.vec, InverseMantissaMask);
    Result.vec = _mm_or_ps(Result.vec, ZeroPointFive);
    
    // now e contain the real base-2 exponent
    M = _mm_sub_epi32(M, Const0x7F);
    __m128 e = _mm_cvtepi32_ps(M);
    
    e = _mm_add_ps(e, One);
    
    // part2: 
    // if( x < SQRTHF ) 
    // {
    //     e -= 1;
    //     x = x + x - 1.0;
    // } 
    // else 
    // { x = x - 1.0; }
    
    __m128 Mask = _mm_cmplt_ps(Result.vec, SQRTHF);
    __m128 Temp = _mm_and_ps(Result.vec, Mask);
    Result.vec = _mm_sub_ps(Result.vec, One);
    e = _mm_sub_ps(e, _mm_and_ps(One, Mask));
    Result.vec = _mm_add_ps(Result.vec, Temp);
    
    __m128 Z = _mm_mul_ps(Result.vec, Result.vec);
    
    __m128 Y = LogP0;
    Y = _mm_fmadd_ps(Y, Result.vec, LogP1);
    Y = _mm_fmadd_ps(Y, Result.vec, LogP2);
    Y = _mm_fmadd_ps(Y, Result.vec, LogP3);
    Y = _mm_fmadd_ps(Y, Result.vec, LogP4);
    Y = _mm_fmadd_ps(Y, Result.vec, LogP5);
    Y = _mm_fmadd_ps(Y, Result.vec, LogP6);
    Y = _mm_fmadd_ps(Y, Result.vec, LogP7);
    Y = _mm_fmadd_ps(Y, Result.vec, LogP8);
    Y = _mm_mul_ps(Y, Result.vec);
    
    Y = _mm_mul_ps(Y, Z);
    
    Temp = _mm_mul_ps(e, LogQ1);
    Y = _mm_add_ps(Y, Temp);
    
    Temp = _mm_mul_ps(Z, ZeroPointFive);
    Y= _mm_sub_ps(Y, Temp);
    
    Temp = _mm_mul_ps(e, LogQ2);
    Result.vec = _mm_add_ps(Result.vec, Y);
    Result.vec = _mm_add_ps(Result.vec, Temp);
    
    
    Result.vec = _mm_or_ps(Result.vec, InvalidMask); // negative arg will be NAN
    
    return Result;
}

inline vector4 FastPositivePower(vector4 Value, float Exponent)
{
    return fast_exp(_mm_mul_ps(_mm_set_ps1(Exponent), fast_log(Value).vec));
}

inline vector4 FastPositivePower(vector4 Value, vector4 Exponent)
{
    return fast_exp(_mm_mul_ps(Exponent.vec, fast_log(Value).vec));
}

inline float Cosine(float Value)
{
    float Result = cosf(Value);
    return Result;
}

inline float Sine(float Value)
{
    float Result = sinf(Value);
    return Result;
}

inline float Tangent(float Value)
{
    float Result = tanf(Value);
    return Result;
}

// IMPORTANT: Only use for affine transformation where points are sure to be set to w = 1 
vector3 MultPointMatrix(matrix4* Matrix, vector3* Vector);
vector4 MultPointMatrix(matrix4* Matrix, vector4* Vector);

vector4 MultVecMatrix(matrix4* Matrix, vector4* Vector);

matrix4 MultMatrixMatrix(matrix4* A, matrix4* B);
matrix4 MultMatrixMatrixIntrinsics(matrix4* A, matrix4* B);

matrix4 InverseMatrix(matrix4* Matrix);
matrix4 TransposeMatrix(matrix4* Matrix);




inline matrix4 GetXRotationMatrix(float AngleInRadians)
{
    matrix4 Result = {};
    float Cos = Cosine(AngleInRadians);
    float Sin = Sine(AngleInRadians);
    Result.val[0][0] = 1.0f;
    Result.val[1][1] = Cos;
    Result.val[2][2] = Cos;
    Result.val[3][3] = 1.0f;
    
    Result.val[1][2] = -Sin;
    Result.val[2][1] = Sin;
    return Result;
}

inline matrix4 GetYRotationMatrix(float AngleInRadians)
{
    matrix4 Result = {};
    float Cos = Cosine(AngleInRadians);
    float Sin = Sine(AngleInRadians);
    Result.val[0][0] = Cos;
    Result.val[1][1] = 1.0f;
    Result.val[2][2] = Cos;
    Result.val[3][3] = 1.0f;
    
    Result.val[0][2] = Sin;
    Result.val[2][0] = -Sin;
    return Result;
}


inline matrix4 GetZRotationMatrix(float AngleInRadians)
{
    matrix4 Result = {};
    float Cos = Cosine(AngleInRadians);
    float Sin = Sine(AngleInRadians);
    Result.val[0][0] = Cos;
    Result.val[1][1] = Cos;
    Result.val[2][2] = 1.0f;
    Result.val[3][3] = 1.0f;
    
    Result.val[0][1] = -Sin;
    Result.val[1][0] = Sin;
    return Result;
}

// Vector 3
// FUNCTIONS

inline float Sum(vector3* Vector)
{
    // Quick testing seemed to show that the SIMD version is performing worse
#if 0
    Assert(Vector->Padding == 0.0f);
    __m128 Shuf = _mm_movehdup_ps(Vector->vec); // broadcast elements 3,1 to 2,0
    __m128 Sums = _mm_add_ps(Vector->vec, Shuf);
    Shuf        = _mm_movehl_ps(Shuf, Sums);    // high half -> low half
    Sums        = _mm_add_ss(Sums, Shuf);
    return        _mm_cvtss_f32(Sums);
#else
    return Vector->X + Vector->Y + Vector->Z;
#endif
}

inline float Sum(__m128 Vector)
{
    // Quick testing seemed to show that the SIMD version is performing worse
#if 0
    __m128 Shuf = _mm_movehdup_ps(Vector); // broadcast elements 3,1 to 2,0
    __m128 Sums = _mm_add_ps(Vector, Shuf);
    Shuf        = _mm_movehl_ps(Shuf, Sums);    // high half -> low half
    Sums        = _mm_add_ss(Sums, Shuf);
    return        _mm_cvtss_f32(Sums);
#else
    return Vector.m128_f32[0] + Vector.m128_f32[1] + Vector.m128_f32[2];
#endif
}

inline float MagnitudeSq(vector3* Vector)
{
    Assert(Vector->Padding == 0.0f);
    __m128 Square = _mm_mul_ps(Vector->vec, Vector->vec);
    return Sum(Square);
}

inline float Magnitude(vector3* Vector)
{
    return SquareRoot(MagnitudeSq(Vector));
}

inline void NormalizeVector(vector3* Vector)
{
    float Length = Magnitude(Vector);
    if(Length != 1.0f)
    {
        __m128 LengthVec = _mm_set_ps1(Length);
        Vector->vec = _mm_div_ps(Vector->vec, LengthVec);
        Vector->Padding = 0.0f;
    }
}

inline float DotProduct(vector3* A, vector3* B)
{
#if 1
    Assert(A->Padding == 0.0f || B->Padding == 0.0f);
    return _mm_dp_ps(A->vec, B->vec, 0xFF).m128_f32[0];
#else
    __m128 Mult = _mm_mul_ps(A->vec, B->vec);
    Assert(Mult.m128_f32[3] == 0.0f);
    return Sum(Mult);
#endif
}

// ADD
inline vector3 operator+(vector3 lhs, vector3 rhs)
{
    vector3 Result;
    Result.vec = _mm_add_ps(lhs.vec, rhs.vec);
    return Result;
    // return vector3{lhs.X + rhs.X, lhs.Y + rhs.Y, lhs.Z + rhs.Z, lhs.Padding + rhs.Padding};
}

// SUB
inline vector3 operator-(vector3 lhs, vector3 rhs)
{
    vector3 Result;
    Result.vec = _mm_sub_ps(lhs.vec, rhs.vec);
    return Result;
    // return vector3{lhs.X - rhs.X, lhs.Y - rhs.Y, lhs.Z - rhs.Z, lhs.Padding - rhs.Padding};
}

inline vector3 operator-(int32_t lhs, vector3 rhs)
{
    vector3 Result;
    Result.vec = _mm_sub_ps(_mm_set1_ps((float)lhs), rhs.vec);
    return Result;
    // return vector3{lhs - rhs.X, lhs - rhs.Y, lhs - rhs.Z, lhs - rhs.Padding};
}

// MULTIPLY
inline vector3 operator*(int32_t lhs, vector3 rhs)
{
    vector3 Result;
    Result.vec = _mm_mul_ps(_mm_set1_ps((float)lhs), rhs.vec);
    return Result;
    // return vector3{rhs.X * lhs, rhs.Y * lhs, rhs.Z * lhs, rhs.Padding * lhs};
}

inline vector3 operator*(float lhs, vector3 rhs)
{
    vector3 Result;
    Result.vec = _mm_mul_ps(_mm_set1_ps(lhs), rhs.vec);
    return Result;
    // return vector3{lhs * rhs.X, lhs * rhs.Y, lhs * rhs.Z, lhs * rhs.Padding};
}

inline vector3 operator*(vector3 lhs, float rhs)
{
    vector3 Result;
    Result.vec = _mm_mul_ps(lhs.vec, _mm_set1_ps(rhs));
    return Result;
    // return vector3{lhs.X * rhs, lhs.Y * rhs, lhs.Z * rhs, lhs.Padding * rhs};
}

inline vector3 operator*(vector3 lhs, vector3 rhs)
{
    vector3 Result;
    Result.vec = _mm_mul_ps(lhs.vec, rhs.vec);
    return Result;
    // return vector3{lhs.X * rhs.X, lhs.Y * rhs.Y, lhs.Z * rhs.Z, lhs.Padding * rhs.Padding};
}

// Vector 4

// ADD
inline vector4i operator+(vector4i lhs, vector4i rhs)
{
    vector4i Result;
    Result.vec = _mm_add_epi32(lhs.vec, rhs.vec);
    return Result;
    // return vector4i{lhs.X + rhs.X, lhs.Y + rhs.Y, lhs.Z + rhs.Z, lhs.W + rhs.W};
}
inline vector4i& operator+=(vector4i& lhs, vector4i rhs)
{
    lhs.vec = _mm_add_epi32(lhs.vec, rhs.vec);
    return lhs;
}

// MULTIPLY
inline vector4i operator*(int32_t lhs, vector4i rhs)
{
    // vector4i Result;
    // Result.vec = _mm_mullo_epi32(_mm_set1_epi32(lhs), rhs.vec);
    // return Result;
    return vector4i{rhs.X * lhs, rhs.Y * lhs, rhs.Z * lhs, rhs.W * lhs};
}

inline vector4i operator*(vector4i lhs, vector4i rhs)
{
    vector4i Result;
    Result.vec = _mm_mullo_epi32(lhs.vec, rhs.vec);
    return Result;
}

// BITWISE
inline vector4i operator|(const vector4i& lhs, const vector4i& rhs)
{
    vector4i Result;
    Result.vec = _mm_or_si128(lhs.vec, rhs.vec);
    return Result;
    // return vector4i{lhs.X | rhs.X, lhs.Y | rhs.Y, lhs.Z | rhs.Z, lhs.W | rhs.W};
}

inline bool VectorIsAnyPositive(vector4i* Vector)
{
    return Vector->X >= 0 || Vector->Y >= 0 || Vector->Z >= 0 || Vector->W >= 0;
}

#define HANDMADE_MATHS_H
#endif