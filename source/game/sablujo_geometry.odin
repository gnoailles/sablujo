package sablujo 

import "core:math"

CUBE_VERTICES_COUNT :: 8
/*
CubeVertices : [CUBE_VERTICES_COUNT][4]f32 :
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

CubeNormals : [CUBE_VERTICES_COUNT][3]f32 :
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
CubeVertices : [CUBE_VERTICES_COUNT][3]f32 :
{
    
    { -0.5, -0.5,  0.5 },
	{  0.5, -0.5,  0.5 },
	{  0.5,  0.5,  0.5 },
	{ -0.5,  0.5,  0.5 },
    // Back
	{ -0.5, -0.5, -0.5 },
	{  0.5, -0.5, -0.5 },
	{  0.5,  0.5, -0.5 },
	{ -0.5,  0.5, -0.5 },
}

CubeNormals : [CUBE_VERTICES_COUNT][3]f32 :
{
    // Front
    { -0.5, -0.5,  0.5 },
    {  0.5, -0.5,  0.5 },
    {  0.5,  0.5,  0.5 },
    { -0.5,  0.5,  0.5 },
    // Back
    { -0.5, -0.5, -0.5 },
    {  0.5, -0.5, -0.5 },
    {  0.5,  0.5, -0.5 },
    { -0.5,  0.5, -0.5 },
}

CUBE_INDICES_COUNT :: 36
CubeIndices : [CUBE_INDICES_COUNT]u32 :
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
    6,7,3,
}

create_sphere :: proc (latitude_count: u32, longitude_count: u32, output_vertices: [][3]f32, output_normals: [][3]f32, output_indices: []u32) {
    assert(u32(len(output_vertices)) >= latitude_count * longitude_count)
    assert(u32(len(output_indices)) >= longitude_count * 3 * 2 + (latitude_count - 1) * (longitude_count - 1) * 6)

    radius : f32 = 0.5
    output_offset := 0

    //North Cap
    output_vertices[output_offset] = {0, radius, 0}
    output_normals[output_offset] = {0, 1, 0}
    output_offset += 1

    for latitude in 0..<latitude_count {
        theta : f64 = f64(latitude) * math.PI / f64(latitude_count)
        sin_theta := math.sin(theta)
        cos_theta := math.cos(theta)

        for longitude in 0..<longitude_count {
            assert(output_offset < len(output_vertices))
            phi : f64 = f64(longitude) * 2 * math.PI / f64(longitude_count)
            sin_phi := math.sin(phi)
            cos_phi := math.cos(phi)

            x := f32(cos_phi * sin_theta)
            z := f32(sin_phi * sin_theta)

            output_vertices[output_offset] = {x * radius, f32(cos_theta) * radius, z * radius}
            output_normals[output_offset] = {x, f32(cos_theta), z}
            output_offset += 1
        }
    }
    //South Cap
    output_vertices[output_offset] = {0, -radius, 0}
    output_normals[output_offset] = {0, -1, 0}

    output_offset = 0
    for latitude in 0..<latitude_count {
        for longitude in 0..<longitude_count {
            assert(output_offset < len(output_indices))

            first := 1 + ((latitude - 1) * longitude_count) + longitude
            second := first + longitude_count

            if(latitude == 0) {
                output_indices[output_offset] = (1 + longitude + 1) % longitude_count
                output_offset += 1
                output_indices[output_offset] = 1 + longitude
                output_offset += 1
                output_indices[output_offset] = latitude // 0
                output_offset += 1
            } else if(latitude == latitude_count - 1) {
                output_indices[output_offset] = first + 1
                output_offset += 1
                output_indices[output_offset] = 1 + latitude * longitude_count
                output_offset += 1
                output_indices[output_offset] = first
                output_offset += 1
            } else {
                output_indices[output_offset] = first + 1
                output_offset += 1
                output_indices[output_offset] = second
                output_offset += 1
                output_indices[output_offset] = first
                output_offset += 1
                
                output_indices[output_offset] = first + 1
                output_offset += 1
                output_indices[output_offset] = second + 1
                output_offset += 1
                output_indices[output_offset] = second
                output_offset += 1
            }
        }
    }
    output_offset += 1
}