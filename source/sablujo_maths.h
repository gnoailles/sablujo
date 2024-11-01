#if !defined(SABLUJO_MATHS_H)

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

#include "sablujo_defines.h"


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
    
    inline float  operator[](int32_t Index)
    {
        Assert(Index < 3);
        return vec.m128_f32[Index];
    }
    
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
    
    vector4() : X{0.0f}, Y{0.0f}, Z{0.0f}, W{0.0f}
    {}
    vector4(float x,float y, float z, float w) : X{x}, Y{y}, Z{z}, W{w}
    {}
    vector4(vector3 xyz, float w) : X{xyz.X}, Y{xyz.Y}, Z{xyz.Z}, W{w}
    {}
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


struct random_series
{
    uint32_t State;
};

internal uint32_t XorShift32(random_series* Series)
{
	uint32_t x = Series->State;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
    Series->State = x;
    
    return x;
}

internal float
RandomUnilateral(random_series* Series)
{
    return (float)XorShift32(Series) / (float)UINT32_MAX;
}

internal float
RandomBilateral(random_series* Series)
{
    return 2.0f * RandomUnilateral(Series) - 1.0f;
}

inline vector3 Min(vector3 A, vector3 B)
{
    return vector3{MIN(A.X, B.X), MIN(A.Y, B.Y), MIN(A.Z, B.Z), 0.0f};
}

inline vector3 Max(vector3 A, vector3 B)
{
    return vector3{MAX(A.X, B.X), MAX(A.Y, B.Y), MAX(A.Z, B.Z), 0.0f};
}

inline vector3 Min(vector3 A, float B)
{
    return vector3{MIN(A.X, B), MIN(A.Y, B), MIN(A.Z, B), 0.0f};
}

inline vector3 Max(vector3 A, float B)
{
    return vector3{MAX(A.X, B), MAX(A.Y, B), MAX(A.Z, B), 0.0f};
}

matrix4 LookAt(vector3 Eye, vector3 Target, vector3 Up);

// IMPORTANT: Only use for affine transformation where points are sure to be set to w = 1 
vector3 MultPointMatrix(matrix4* Matrix, vector3* Vector);
vector4 MultPointMatrix(matrix4* Matrix, vector4* Vector);

vector4 MultVecMatrix(matrix4* Matrix, vector4* Vector);

matrix4 MultMatrixMatrix(matrix4* A, matrix4* B);
matrix4 MultMatrixMatrixIntrinsics(matrix4* A, matrix4* B);

matrix4 InverseMatrix(matrix4* Matrix);
matrix4 TransposeMatrix(matrix4* Matrix);

inline matrix4 GetIdentityMatrix()
{
    matrix4 Result = {};
    Result.val[0][0] = 1.0f;
    Result.val[1][1] = 1.0f;
    Result.val[2][2] = 1.0f;
    Result.val[3][3] = 1.0f;
    return Result;
}

inline matrix4 GetXRotationMatrix(float AngleInRadians)
{
    matrix4 Result = {};
    float Cos = Cosine(AngleInRadians);
    float Sin = Sine(AngleInRadians);
    Result.val[0][0] = 1.0f;
    Result.val[1][1] = Cos;
    Result.val[2][2] = Cos;
    Result.val[3][3] = 1.0f;
    
    Result.val[1][2] = Sin;
    Result.val[2][1] = -Sin;
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
    
    Result.val[0][2] = -Sin;
    Result.val[2][0] = Sin;
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
    
    Result.val[0][1] = Sin;
    Result.val[1][0] = -Sin;
    return Result;
}

inline matrix4 GetTranslationMatrix(vector3 Translation)
{
    matrix4 Result = GetIdentityMatrix();
    Result.val[3][0] = Translation.X;
    Result.val[3][1] = Translation.Y;
    Result.val[3][2] = Translation.Z;
    return Result;
}

// Vector 3
// OPERATORS
inline vector3
operator-(float lhs, vector3 rhs)
{
    vector3 Result;
    Result.vec = _mm_sub_ps(_mm_set1_ps(lhs), rhs.vec);
    return Result;
}

inline vector3
operator-(vector3 rhs)
{
    vector3 Result;
    Result.vec = _mm_sub_ps(_mm_setzero_ps(), rhs.vec);
    return Result;
}

inline vector3
operator-(vector3 lhs, vector3 rhs)
{
    vector3 Result;
    Result.vec = _mm_sub_ps(lhs.vec, rhs.vec);
    return Result;
}

inline vector3
operator+(vector3 lhs, vector3 rhs)
{
    vector3 Result;
    Result.vec = _mm_add_ps(lhs.vec, rhs.vec);
    return Result;
}

inline void
operator+=(vector3& lhs, vector3 rhs)
{
    lhs.vec = _mm_add_ps(lhs.vec, rhs.vec);
}

inline vector3
operator*(float lhs, vector3 rhs)
{
    vector3 Result;
    Result.vec = _mm_mul_ps(_mm_set1_ps(lhs), rhs.vec);
    return Result;
}

inline vector3
operator*(vector3 lhs, float rhs)
{
    vector3 Result;
    Result.vec = _mm_mul_ps(_mm_set1_ps(rhs), lhs.vec);
    return Result;
}

inline vector3
operator/(vector3 lhs, float rhs)
{
    vector3 Result;
    Result.vec = _mm_div_ps(lhs.vec, _mm_set1_ps(rhs));
    return Result;
}

inline vector3
operator/(float lhs, vector3 rhs)
{
    vector3 Result;
    Result.vec = _mm_div_ps(_mm_set1_ps(lhs), rhs.vec);
    return Result;
}

#if LANE_WIDTH != 1

inline vector3
operator*(vector3 lhs, vector3 rhs)
{
    vector3 Result;
    Result.vec = _mm_mul_ps(lhs.vec, rhs.vec);
    return Result;
}

inline float MagnitudeSq(vector3 A)
{
    return A.X * A.X + A.Y * A.Y + A.Z * A.Z;
}

inline float Magnitude(vector3 A)
{
    return SquareRoot(MagnitudeSq(A));
}

inline vector3 Normalize(vector3 A)
{
    vector3 Result;
    Result = A / Magnitude(A);
    return Result;
}

inline float
DotProduct(vector3 A, vector3 B)
{
    return A.X *B.X + A.Y * B.Y + A.Z * B.Z;
}

#endif

inline vector3
CrossProduct(vector3 A, vector3 B)
{
    vector3 Result;
    Result.X = A.Y *B.Z - A.Z * B.Y;
    Result.Y = A.Z *B.X - A.X * B.Z;
    Result.Z = A.X *B.Y - A.Y * B.X;
    return Result;
}

inline vector3 Lerp(vector3 A, vector3 B, float t)
{
    return (1.0f-t)*A + t*B;
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

#define SABLUJO_MATHS_H
#endif