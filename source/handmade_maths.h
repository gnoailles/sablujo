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

inline vector3
operator+(vector3 lhs, vector3 rhs)
{
    vector3 Result;
    Result.vec = _mm_add_ps(lhs.vec, rhs.vec);
    return Result;
}

inline vector3
operator*(float lhs, vector3 rhs)
{
    vector3 Result;
    Result.vec = _mm_mul_ps(_mm_set1_ps(lhs), rhs.vec);
    return Result;
}

// Vector 4

// ADD
inline vector4i operator+(vector4i lhs, vector4i rhs)
{
    vector4i Result;
    Result.vec = _mm_add_epi32(lhs.vec, rhs.vec);
    return Result;
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
}

inline bool VectorIsAnyPositive(vector4i* Vector)
{
    return Vector->X >= 0 || Vector->Y >= 0 || Vector->Z >= 0 || Vector->W >= 0;
}

#define HANDMADE_MATHS_H
#endif