#if !defined(GEOMETRY_H)

#include "sablujo_maths.h"

void CreateSphere(uint32_t LatitudeCount, uint32_t LongitudeCount, 
                  vector3* OutputVertices, vector3* OutputNormals, uint32_t* OutputIndices,
                  uint32_t OutVerticesSize, uint32_t OutIndicesSize);

const uint32_t CubeVerticesCount = 8;
/*
global_variable vector4 CubeVertices[CubeVerticesCount] = 
{
    
    { -0.5f, -0.5f,  0.5f, 1.0f },
	{  0.5f, -0.5f,  0.5f, 1.0f },
	{  0.5f,  0.5f,  0.5f, 1.0f },
	{ -0.5f,  0.5f,  0.5f, 1.0f },
    // Back
	{ -0.5f, -0.5f, -0.5f, 1.0f },
	{  0.5f, -0.5f, -0.5f, 1.0f },
	{  0.5f,  0.5f, -0.5f, 1.0f },
	{ -0.5f,  0.5f, -0.5f, 1.0f }
};

global_variable vector3 CubeNormals[CubeVerticesCount] = 
{
    // Front
    { -0.5f, -0.5f,  0.5f, 0.0f },
    {  0.5f, -0.5f,  0.5f, 0.0f },
    {  0.5f,  0.5f,  0.5f, 0.0f },
    { -0.5f,  0.5f,  0.5f, 0.0f },
    // Back
    { -0.5f, -0.5f, -0.5f, 0.0f },
    {  0.5f, -0.5f, -0.5f, 0.0f },
    {  0.5f,  0.5f, -0.5f, 0.0f },
    { -0.5f,  0.5f, -0.5f, 0.0f }
};

*/

global_variable float CubeVertices[CubeVerticesCount][7] = 
{
    // POSITION                  NORMAL
    // Front
    { -0.5f, -0.5f,  0.5f, 1.0f, -0.5f, -0.5f,  0.5f },
	{  0.5f, -0.5f,  0.5f, 1.0f,  0.5f, -0.5f,  0.5f },
	{  0.5f,  0.5f,  0.5f, 1.0f,  0.5f,  0.5f,  0.5f },
	{ -0.5f,  0.5f,  0.5f, 1.0f, -0.5f,  0.5f,  0.5f },
    // Back
	{ -0.5f, -0.5f, -0.5f, 1.0f, -0.5f, -0.5f, -0.5f },
	{  0.5f, -0.5f, -0.5f, 1.0f,  0.5f, -0.5f, -0.5f },
	{  0.5f,  0.5f, -0.5f, 1.0f,  0.5f,  0.5f, -0.5f },
	{ -0.5f,  0.5f, -0.5f, 1.0f, -0.5f,  0.5f, -0.5f }
};

/*
global_variable vector3 CubeNormals[CubeVerticesCount] = 
{
    // Front
    { -0.5f, -0.5f,  0.5f },
    {  0.5f, -0.5f,  0.5f },
    {  0.5f,  0.5f,  0.5f },
    { -0.5f,  0.5f,  0.5f },
    // Back
    { -0.5f, -0.5f, -0.5f },
    {  0.5f, -0.5f, -0.5f },
    {  0.5f,  0.5f, -0.5f },
    { -0.5f,  0.5f, -0.5f }
};
*/

const uint32_t CubeIndicesCount = 36;
global_variable uint32_t CubeIndices[CubeIndicesCount] =
/*
{
    // Front
    0,1,2,
    2,3,0,
    
    // Top
    1,5,6,
    6,2,1,
    
    // Back
    7,6,5,
    5,4,7,
    
    // Bottom
    4,0,3,
    3,7,4,
    
    // Left
    4,5,1,
    1,0,4,
    
    // Right
    3,2,6,
    6,7,3
};
*/
{
    // Front
    2,1,0,
    0,3,2,
    
    // Top
    6,5,1,
    1,2,6,
    
    // Back
    5,6,7,
    7,4,5,
    
    // Bottom
    3,0,4,
    4,7,3,
    
    // Left
    1,5,4,
    4,0,1,
    
    // Right
    6,2,3,
    3,7,6
};

#define GEOMETRY_H
#endif