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
// Vector 2

inline float MagnitudeSq(vector2 A)
{
    return A.X * A.X + A.Y * A.Y;
};

inline vector2
Normalize(vector2 A)
{
    __m128 Vector = _mm_setr_ps(A.X, A.Y, 0.0f, 0.0f);
    float LengthSq = MagnitudeSq(A);
    if(LengthSq > 0.0000001f)
    {
        __m128 InvLength = _mm_rsqrt_ps(_mm_set_ps1(LengthSq));
        Vector = _mm_mul_ps(Vector, InvLength);
        A.X = Vector.m128_f32[0];
        A.Y = Vector.m128_f32[1];
    }
    return A;
}

inline float
Clamp(float A, float LowerBound, float UpperBound)
{
    return MAX(MIN(A, UpperBound), LowerBound);
}

inline uint32_t
Clamp(uint32_t A, uint32_t LowerBound, uint32_t UpperBound)
{
    return MAX(MIN(A, UpperBound), LowerBound);
}

// Operators
inline vector2 operator+(vector2 lhs, vector2 rhs)
{
    vector2 Result;
    Result.X = lhs.X + rhs.X;
    Result.Y = lhs.Y + rhs.Y;
    return Result;
}

inline vector2 operator+(vector2 lhs, float rhs)
{
    vector2 Result;
    Result.X = lhs.X + rhs;
    Result.Y = lhs.Y + rhs;
    return Result;
}

inline void operator+=(vector2& lhs, vector2 rhs)
{
    lhs.X += rhs.X;
    lhs.Y += rhs.Y;
}

inline vector2 operator-(vector2 lhs, vector2 rhs)
{
    vector2 Result;
    Result.X = lhs.X - rhs.X;
    Result.Y = lhs.Y - rhs.Y;
    return Result;
}

inline vector2 operator-(vector2 lhs, float rhs)
{
    vector2 Result;
    Result.X = lhs.X - rhs;
    Result.Y = lhs.Y - rhs;
    return Result;
}

inline void operator-=(vector2& lhs, vector2 rhs)
{
    lhs.X -= rhs.X;
    lhs.Y -= rhs.Y;
}

inline vector2 operator*(vector2 lhs, vector2 rhs)
{
    vector2 Result;
    Result.X = lhs.X * rhs.X;
    Result.Y = lhs.Y * rhs.Y;
    return Result;
}

inline vector2 operator*(vector2 lhs, float rhs)
{
    vector2 Result;
    Result.X = lhs.X * rhs;
    Result.Y = lhs.Y * rhs;
    return Result;
}

inline vector2 operator/(vector2 lhs, vector2 rhs)
{
    vector2 Result;
    Result.X = lhs.X / rhs.X;
    Result.Y = lhs.Y / rhs.Y;
    return Result;
}

inline vector2 operator/(vector2 lhs, float rhs)
{
    vector2 Result;
    if(rhs < EPSILON)
    {
        rhs = EPSILON;
    }
    Result.X = lhs.X / rhs;
    Result.Y = lhs.Y / rhs;
    return Result;
}

inline void operator/=(vector2& lhs, float rhs)
{
    lhs.X /= rhs;
    lhs.Y /= rhs;
}

inline bool operator==(vector2 lhs, vector2 rhs)
{
    return lhs.X == rhs.X && lhs.Y == rhs.Y;
}

inline bool operator!=(vector2 lhs, vector2 rhs)
{
    return lhs.X != rhs.X && lhs.Y != rhs.Y;
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

#define SABLUJO_MATHS_H
#endif