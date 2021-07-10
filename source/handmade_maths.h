#if !defined(HANDMADE_MATHS_H)

#define PI_FLOAT 3.14159265358979323846f

#ifndef MAX
#define MAX(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#include <immintrin.h>
#include <stdint.h>

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
            float W;
        };
        __m128 vec;
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



//TODO: Replace call to math.h
#include <math.h>
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

inline int32_t GCD(int32_t A, int32_t B)
{
    while (B != 0)
    {
        A %= B;
        A ^= B;
        B ^= A;
        A ^= B;
    }

    return A;
}

vector3 multVecMatrix(matrix4* Matrix, vector3* Vector);
matrix4 multMatrixMatrix(matrix4* A, matrix4* B);
matrix4 multMatrixMatrixIntrinsics(matrix4* A, matrix4* B);




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



// inline float sin(float Value)
// {
//     __m128 ValueVec = _mm_load_ps1(&Value);
//     _mm_store_ss(&Value, ValueVec);
//     return Value;
// }

#define HANDMADE_MATHS_H
#endif