#include "handmade_maths.h"

vector3 multVecMatrix(matrix4* Matrix, vector3* Vector)
{
    vector3 Result;

    Result.X = Vector->X * Matrix->val[0][0] + Vector->Y * Matrix->val[1][0] + Vector->Z * Matrix->val[2][0] + /*Vector->W = 1*/ Matrix->val[3][0];
    Result.Y = Vector->X * Matrix->val[0][1] + Vector->Y * Matrix->val[1][1] + Vector->Z * Matrix->val[2][1] + /*Vector->W = 1*/ Matrix->val[3][1];
    Result.Z = Vector->X * Matrix->val[0][2] + Vector->Y * Matrix->val[1][2] + Vector->Z * Matrix->val[2][2] + /*Vector->W = 1*/ Matrix->val[3][2];
    float w  = Vector->X * Matrix->val[0][3] + Vector->Y * Matrix->val[1][3] + Vector->Z * Matrix->val[2][3] + /*Vector->W = 1*/ Matrix->val[3][3];

    if (w != 1.0f)
    {
        Result.X /= w;
        Result.Y /= w;
        Result.Z /= w;
    }
    return Result;
}


matrix4 multMatrixMatrix(matrix4* A, matrix4* B)
{
    matrix4 Result;
    for(int32_t i = 0; i < 4; ++i)
    {
        for(int32_t j = 0; j < 4; ++j)
        {
            Result.val[i][j] = A->val[i][0] * B->val[0][j] + A->val[i][1] * B->val[1][j] + A->val[i][2] * B->val[2][j] + A->val[i][3] * B->val[3][j];
        }
    }
    return Result;
}

matrix4 multMatrixMatrixIntrinsics(matrix4* A, matrix4* B)
{
    matrix4 Result;
    __m256 vec_multi_res = _mm256_setzero_ps(); //Initialize vector to zero
    __m256 vec_mat1 = _mm256_setzero_ps(); //Initialize vector to zero
    __m256 vec_mat2 = _mm256_setzero_ps(); //Initialize vector to zero

    int32_t i, j, k;
    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 4; ++j)
        {
            //Stores one element in mat1 and use it in all computations needed before proceeding
            //Stores as vector to increase computations per cycle
            vec_mat1 = _mm256_set1_ps(A->val[i][j]);

            for (k = 0; k < 4; k += 8)
            {
                vec_mat2 = _mm256_loadu_ps(&B->val[j][k]); //Stores row of second matrix (eight in each iteration)
                vec_multi_res = _mm256_loadu_ps(&B->val[i][k]); //Loads the result matrix row as a vector
                vec_multi_res = _mm256_add_ps(vec_multi_res ,_mm256_mul_ps(vec_mat1, vec_mat2));//Multiplies the vectors and adds to th the result vector

                _mm256_storeu_ps(&Result.val[i][k], vec_multi_res); //Stores the result vector into the result array
            }
        }
    }
    return Result;
}