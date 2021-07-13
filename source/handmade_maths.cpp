#include "handmade_maths.h"


vector3 MultPointMatrix(matrix4* Matrix, vector3* Vector)
{
    vector3 Result;
    Result.X = Vector->X * Matrix->val[0][0] + Vector->Y * Matrix->val[1][0] + Vector->Z * Matrix->val[2][0] + Vector->Padding * Matrix->val[3][0];
    Result.Y = Vector->X * Matrix->val[0][1] + Vector->Y * Matrix->val[1][1] + Vector->Z * Matrix->val[2][1] + Vector->Padding * Matrix->val[3][1];
    Result.Z = Vector->X * Matrix->val[0][2] + Vector->Y * Matrix->val[1][2] + Vector->Z * Matrix->val[2][2] + Vector->Padding * Matrix->val[3][2];
    return Result;
}

vector4 MultPointMatrix(matrix4* Matrix, vector4* Vector)
{
    // Result.vec = _mm_shuffle_ps(Vector->vec, Vector->vec, _MM_SHUFFLE(0,0,0,0));
    // Result.vec = _mm_mul_ps(Result.vec, Matrix->vecs[0]);

    // __m128 vTemp = _mm_shuffle_ps(Vector->vec, Vector->vec, _MM_SHUFFLE(1,1,1,1));
    // vTemp = _mm_mul_ps(vTemp, Matrix->vecs[1]);
    
	// Result.vec = _mm_add_ps(Result.vec, vTemp);
    // vTemp = _mm_shuffle_ps(Vector->vec, Vector->vec, _MM_SHUFFLE(2,2,2,2));
    
	// vTemp = _mm_mul_ps(vTemp, Matrix->vecs[2]);
    // Result.vec = _mm_add_ps(Result.vec, vTemp);
    
	// Result.vec = _mm_add_ps(Result.vec, Matrix->vecs[3]);
	// return Result;
    Assert(Vector->W == 1.0f);
    vector4 Result;
    Result.X = Vector->X * Matrix->val[0][0] + Vector->Y * Matrix->val[1][0] + Vector->Z * Matrix->val[2][0] + Matrix->val[3][0];
    Result.Y = Vector->X * Matrix->val[0][1] + Vector->Y * Matrix->val[1][1] + Vector->Z * Matrix->val[2][1] + Matrix->val[3][1];
    Result.Z = Vector->X * Matrix->val[0][2] + Vector->Y * Matrix->val[1][2] + Vector->Z * Matrix->val[2][2] + Matrix->val[3][2];
    Result.W = 1.0f;
    return Result;
}

vector4 MultVecMatrix(matrix4* Matrix, vector4* Vector)
{
    vector4 Result;
    Result.X = Vector->X * Matrix->val[0][0] + Vector->Y * Matrix->val[1][0] + Vector->Z * Matrix->val[2][0] + Vector->W * Matrix->val[3][0];
    Result.Y = Vector->X * Matrix->val[0][1] + Vector->Y * Matrix->val[1][1] + Vector->Z * Matrix->val[2][1] + Vector->W * Matrix->val[3][1];
    Result.Z = Vector->X * Matrix->val[0][2] + Vector->Y * Matrix->val[1][2] + Vector->Z * Matrix->val[2][2] + Vector->W * Matrix->val[3][2];
    Result.W  = Vector->X * Matrix->val[0][3] + Vector->Y * Matrix->val[1][3] + Vector->Z * Matrix->val[2][3] + Vector->W * Matrix->val[3][3];

    if (Result.W != 1.0f)
    {
        Result.X /= Result.W;
        Result.Y /= Result.W;
        Result.Z /= Result.W;
        Result.W /= Result.W;
    }
    return Result;
}


matrix4 MultMatrixMatrix(matrix4* A, matrix4* B)
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

matrix4 MultMatrixMatrixIntrinsics(matrix4* A, matrix4* B)
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



#define MakeShuffleMask(x,y,z,w)           (x | (y<<2) | (z<<4) | (w<<6))

// vec(0, 1, 2, 3) -> (vec[x], vec[y], vec[z], vec[w])
#define VecSwizzleMask(vec, mask)          _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(vec), mask))
#define VecSwizzle(vec, x, y, z, w)        VecSwizzleMask(vec, MakeShuffleMask(x,y,z,w))
#define VecSwizzle1(vec, x)                VecSwizzleMask(vec, MakeShuffleMask(x,x,x,x))
// special swizzle
#define VecSwizzle_0022(vec)               _mm_moveldup_ps(vec)
#define VecSwizzle_1133(vec)               _mm_movehdup_ps(vec)

// return (vec1[x], vec1[y], vec2[z], vec2[w])
#define VecShuffle(vec1, vec2, x,y,z,w)    _mm_shuffle_ps(vec1, vec2, MakeShuffleMask(x,y,z,w))
// special shuffle
#define VecShuffle_0101(vec1, vec2)        _mm_movelh_ps(vec1, vec2)
#define VecShuffle_2323(vec1, vec2)        _mm_movehl_ps(vec2, vec1)

// for row major matrix
// we use __m128 to represent 2x2 matrix as A = | A0  A1 |
//                                              | A2  A3 |
// 2x2 row major Matrix multiply A*B
inline __m128 Mat2Mul(__m128 vec1, __m128 vec2)
{
	return
		_mm_add_ps(_mm_mul_ps(                     vec1, VecSwizzle(vec2, 0,3,0,3)),
		           _mm_mul_ps(VecSwizzle(vec1, 1,0,3,2), VecSwizzle(vec2, 2,1,2,1)));
}
// 2x2 row major Matrix adjugate multiply (A#)*B
inline __m128 Mat2AdjMul(__m128 vec1, __m128 vec2)
{
	return
		_mm_sub_ps(_mm_mul_ps(VecSwizzle(vec1, 3,3,0,0), vec2),
		           _mm_mul_ps(VecSwizzle(vec1, 1,1,2,2), VecSwizzle(vec2, 2,3,0,1)));

}
// 2x2 row major Matrix multiply adjugate A*(B#)
inline __m128 Mat2MulAdj(__m128 vec1, __m128 vec2)
{
	return
		_mm_sub_ps(_mm_mul_ps(                     vec1, VecSwizzle(vec2, 3,0,3,0)),
		           _mm_mul_ps(VecSwizzle(vec1, 1,0,3,2), VecSwizzle(vec2, 2,1,2,1)));
}


matrix4 InverseMatrix(matrix4* Matrix)
{
	// use block matrix method
	// A is a matrix, then i(A) or iA means inverse of A, A# (or A_ in code) means adjugate of A, |A| (or detA in code) is determinant, tr(A) is trace

	// sub matrices
	__m128 A = VecShuffle_0101(Matrix->vecs[0], Matrix->vecs[1]);
	__m128 B = VecShuffle_2323(Matrix->vecs[0], Matrix->vecs[1]);
	__m128 C = VecShuffle_0101(Matrix->vecs[2], Matrix->vecs[3]);
	__m128 D = VecShuffle_2323(Matrix->vecs[2], Matrix->vecs[3]);

#if 0
	__m128 detA = _mm_set1_ps(inM.m[0][0] * inM.m[1][1] - inM.m[0][1] * inM.m[1][0]);
	__m128 detB = _mm_set1_ps(inM.m[0][2] * inM.m[1][3] - inM.m[0][3] * inM.m[1][2]);
	__m128 detC = _mm_set1_ps(inM.m[2][0] * inM.m[3][1] - inM.m[2][1] * inM.m[3][0]);
	__m128 detD = _mm_set1_ps(inM.m[2][2] * inM.m[3][3] - inM.m[2][3] * inM.m[3][2]);
#else
	// determinant as (|A| |B| |C| |D|)
	__m128 detSub = _mm_sub_ps(
		_mm_mul_ps(VecShuffle(Matrix->vecs[0], Matrix->vecs[2], 0,2,0,2), VecShuffle(Matrix->vecs[1], Matrix->vecs[3], 1,3,1,3)),
		_mm_mul_ps(VecShuffle(Matrix->vecs[0], Matrix->vecs[2], 1,3,1,3), VecShuffle(Matrix->vecs[1], Matrix->vecs[3], 0,2,0,2))
	);
	__m128 detA = VecSwizzle1(detSub, 0);
	__m128 detB = VecSwizzle1(detSub, 1);
	__m128 detC = VecSwizzle1(detSub, 2);
	__m128 detD = VecSwizzle1(detSub, 3);
#endif

	// let iM = 1/|M| * | X  Y |
	//                  | Z  W |

	// D#C
	__m128 D_C = Mat2AdjMul(D, C);
	// A#B
	__m128 A_B = Mat2AdjMul(A, B);
	// X# = |D|A - B(D#C)
	__m128 X_ = _mm_sub_ps(_mm_mul_ps(detD, A), Mat2Mul(B, D_C));
	// W# = |A|D - C(A#B)
	__m128 W_ = _mm_sub_ps(_mm_mul_ps(detA, D), Mat2Mul(C, A_B));

	// |M| = |A|*|D| + ... (continue later)
	__m128 detM = _mm_mul_ps(detA, detD);

	// Y# = |B|C - D(A#B)#
	__m128 Y_ = _mm_sub_ps(_mm_mul_ps(detB, C), Mat2MulAdj(D, A_B));
	// Z# = |C|B - A(D#C)#
	__m128 Z_ = _mm_sub_ps(_mm_mul_ps(detC, B), Mat2MulAdj(A, D_C));

	// |M| = |A|*|D| + |B|*|C| ... (continue later)
	detM = _mm_add_ps(detM, _mm_mul_ps(detB, detC));

	// tr((A#B)(D#C))
	__m128 tr = _mm_mul_ps(A_B, VecSwizzle(D_C, 0,2,1,3));
	tr = _mm_hadd_ps(tr, tr);
	tr = _mm_hadd_ps(tr, tr);
	// |M| = |A|*|D| + |B|*|C| - tr((A#B)(D#C)
	detM = _mm_sub_ps(detM, tr);

	const __m128 adjSignMask = _mm_setr_ps(1.f, -1.f, -1.f, 1.f);
	// (1/|M|, -1/|M|, -1/|M|, 1/|M|)
	__m128 rDetM = _mm_div_ps(adjSignMask, detM);

	X_ = _mm_mul_ps(X_, rDetM);
	Y_ = _mm_mul_ps(Y_, rDetM);
	Z_ = _mm_mul_ps(Z_, rDetM);
	W_ = _mm_mul_ps(W_, rDetM);

	matrix4 Result;
	// apply adjugate and store, here we combine adjugate shuffle and store shuffle
	Result.vecs[0] = VecShuffle(X_, Y_, 3,1,3,1);
	Result.vecs[1] = VecShuffle(X_, Y_, 2,0,2,0);
	Result.vecs[2] = VecShuffle(Z_, W_, 3,1,3,1);
	Result.vecs[3] = VecShuffle(Z_, W_, 2,0,2,0);

	return Result;
}

matrix4 TransposeMatrix(matrix4* Matrix)
{
	matrix4 Result;
    __m128 A0 = _mm_unpacklo_ps(Matrix->vecs[0], Matrix->vecs[1]);
    __m128 A1 = _mm_unpackhi_ps(Matrix->vecs[0], Matrix->vecs[1]);
    __m128 A2 = _mm_unpacklo_ps(Matrix->vecs[2], Matrix->vecs[3]);
    __m128 A3 = _mm_unpackhi_ps(Matrix->vecs[2], Matrix->vecs[3]);
    Result.vecs[0] = _mm_movelh_ps(A0, A2);
    Result.vecs[1] = _mm_movehl_ps(A2, A0);
    Result.vecs[2] = _mm_movelh_ps(A1, A3);
    Result.vecs[3] = _mm_movehl_ps(A3, A1);
    return Result;
}