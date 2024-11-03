package sablujo 

import "core:mem"
import "core:fmt"
import "core:math"
import "core:math/linalg"
import "base:intrinsics"

import "../common"

//////////////////
// Game Specific
//////////////////

StepXSize := common.LANE_WIDTH > 1 ? common.LANE_WIDTH / 2 : 1
StepYSize := common.LANE_WIDTH > 1 ? common.LANE_WIDTH / StepXSize : 1

Camera :: struct
{
    view: matrix[4,4]f32,
    projection: matrix[4,4]f32,
    aspect_ratio: f32,
    is_initialized: bool,
}

Mesh :: struct
{
    Vertices: [][3]f32,
    Normals: [][3]f32,
    Indices: []u32,
    VerticesCount: u32,
    IndicesCount: u32,
    Transform: matrix[4,4]f32,
    InverseTransform: matrix[4,4]f32,
}

SPHERE_SUBDIV :: 28 
SPHERE_VERTEX_COUNT :: (SPHERE_SUBDIV * SPHERE_SUBDIV + 2)
SPHERE_INDEX_COUNT :: (SPHERE_SUBDIV * 3 * 2 + (SPHERE_SUBDIV - 1) * (SPHERE_SUBDIV - 1) * 6)

when common.SABLUJO_INTERNAL {
    Render_Stats :: struct
    {
        VerticesCount: u32,
        TrianglesCount: u32,
        PixelsSkipped: u32,
        PixelsComputed: u32,
        PixelsWasted: u32,
    }

    Game_State :: struct
    {
        render_stats: Render_Stats,
        camera: Camera,
        meshes: [2]Mesh,
        y_rot: f32,
        is_initialized: bool,
    }
} else {
    Game_State :: struct
    {
        camera: Camera,
        meshes: [2]Mesh,
        y_rot: f32,
    }
}

@(export)
game_update_and_render :: proc (memory: ^common.Game_Memory, buffer: ^common.Game_Offscreen_Buffer) {
    
    assert(size_of(Game_State) <= memory.permanent_storage_size)
    game_state: ^Game_State = cast(^Game_State)(memory.permanent_storage)
    clear_buffer(buffer)
    setup_and_render_rasterizer(memory, buffer, game_state)
}

clear_buffer :: proc (buffer: ^common.Game_Offscreen_Buffer) {
    assert(buffer.height * buffer.width % 2 == 0)
    
    double_pixel : ^u64 = (^u64)(buffer.memory)
    
    EndPointer : ^u64 = mem.ptr_offset(double_pixel, buffer.height * buffer.width / 2)
    for(double_pixel != EndPointer) {
        double_pixel^ = 0xFFFFFFFFFFFFFFFF
        // double_pixel^ = 0
        double_pixel = mem.ptr_offset(double_pixel, 1)
    }
}

initialize_camera :: proc (camera: ^Camera, image_width: i32, image_height: i32) {
    if camera.is_initialized{
        return
    }

    fov : f32 = 90 
    near : f32 = 0.1
    far : f32 = 100.0
    camera.aspect_ratio = f32(image_width) / f32(image_height)
    
    half_fov_rad : f32 = fov * 0.5 * math.PI / 180.0
    scale : f32 = math.tan(half_fov_rad) * near
    right : f32 = camera.aspect_ratio * scale
    left : f32 = -right
    top : f32 = scale
    bottom : f32 = -top
    
    camera.projection = {}
    
    camera.projection[0, 0] = 2 * near / (right - left)
    camera.projection[1, 1] = 2 * near / (top - bottom)
    
    camera.projection[2, 0] = (right + left) / (right - left)
    camera.projection[2, 1] = (top + bottom) / (top - bottom)
    camera.projection[2, 2] = -(far + near) / (far - near)
    camera.projection[2, 3] = -1
    
    
    camera.projection[3, 2] = -2 * far * near / (far - near)
    
    camera.view = {}
    camera.view[0, 0] = 1
    camera.view[1, 1] = 1
    camera.view[2, 2] = 1
    camera.view[3, 3] = 1
    
    camera.view[3, 1] = 0
    camera.view[3, 2] = 0
    angle_rad : f32 = -10.0 * math.PI / 180.0
    x_rot_matrix := linalg.matrix4_from_euler_angles_xy_f32(angle_rad, 0)
    camera.view = camera.view * x_rot_matrix
    camera.is_initialized = true
}

setup_and_render_rasterizer :: proc (memory: ^common.Game_Memory, buffer: ^common.Game_Offscreen_Buffer, game_state: ^Game_State) {
when common.SABLUJO_INTERNAL {
    game_state.render_stats = {}
}
    cube := &game_state.meshes[0]
    sphere := &game_state.meshes[1]
    
    if !game_state.is_initialized {
        game_state.is_initialized = true
        initialize_camera(&game_state.camera, buffer.width, buffer.height)
        game_state.meshes[0] = {}
        game_state.meshes[1] = {}

        cube_vertices_data := (^[3]f32)(mem.ptr_offset((^u8)(memory.permanent_storage), size_of(Game_State)))
        cube_normals_data := (^[3]f32)(mem.ptr_offset((^u8)(cube_vertices_data), size_of([3]f32) * CUBE_VERTICES_COUNT))
        cube_indices_data := (^u32)(mem.ptr_offset((^u8)(cube_normals_data), size_of([3]f32) * CUBE_VERTICES_COUNT))
        assert((uintptr(cube_indices_data) + CUBE_INDICES_COUNT) <= (uintptr(memory.permanent_storage) + uintptr(memory.permanent_storage_size)))
    
    
        cube.Vertices = mem.slice_ptr(cube_vertices_data, CUBE_VERTICES_COUNT)
        cube.Normals = mem.slice_ptr(cube_normals_data, CUBE_VERTICES_COUNT)
        cube.Indices = mem.slice_ptr(cube_indices_data, CUBE_INDICES_COUNT)
        cube.VerticesCount = CUBE_VERTICES_COUNT
        cube.IndicesCount = CUBE_INDICES_COUNT
    
        SphereVertices := (^[3]f32)(mem.ptr_offset((^u8)(memory.permanent_storage), size_of(Game_State)))
        SphereNormals := (^[3]f32)(mem.ptr_offset((^u8)(SphereVertices), size_of([3]f32) * SPHERE_VERTEX_COUNT))
        SphereIndices := (^u32)(mem.ptr_offset((^u8)(SphereNormals), size_of([3]f32) * SPHERE_VERTEX_COUNT))
        assert(uintptr(SphereIndices) + SPHERE_INDEX_COUNT <= uintptr(memory.permanent_storage) + uintptr(memory.permanent_storage_size))
        
        sphere.Vertices = mem.slice_ptr(SphereVertices, SPHERE_VERTEX_COUNT)
        sphere.Normals = mem.slice_ptr(SphereNormals, SPHERE_VERTEX_COUNT)
        sphere.Indices = mem.slice_ptr(SphereIndices, SPHERE_INDEX_COUNT)
        sphere.VerticesCount = SPHERE_VERTEX_COUNT
        sphere.IndicesCount = SPHERE_INDEX_COUNT

        cube_vertices := CubeVertices
        cube_normals := CubeNormals
        cube_indices := CubeIndices
        copy_slice(cube.Vertices, cube_vertices[:])
        copy_slice(cube.Normals, cube_normals[:])
        copy_slice(cube.Indices, cube_indices[:])
        create_sphere(SPHERE_SUBDIV, SPHERE_SUBDIV, sphere.Vertices, sphere.Normals, sphere.Indices)
    }
    
    y_angle_rad : f32 = 0.0 + game_state.y_rot * math.PI / 180.0
    x_angle_rad : f32 = 0.0 * math.PI / 180.0
    rotation := linalg.matrix4_from_euler_angles_yx_f32(y_angle_rad, x_angle_rad)
    game_state.y_rot += 0.5
    
    translation := linalg.matrix4_translate_f32([3]f32{-1.0, 0.5, 2.0})
    
    sphere.Transform = rotation * translation
    sphere.InverseTransform = linalg.matrix4x4_inverse(sphere.Transform)
    sphere.InverseTransform = intrinsics.transpose(sphere.InverseTransform)

    translation = linalg.matrix4_translate_f32([3]f32{1.0, 0.0, 2.0})

    cube.Transform = rotation * translation
    cube.InverseTransform = linalg.matrix4x4_inverse(cube.Transform)
    cube.InverseTransform = intrinsics.transpose(cube.InverseTransform)
    
    for &mesh in game_state.meshes {
        rasterize_mesh(memory, buffer, game_state, &mesh)
    }
    
when common.SABLUJO_INTERNAL {
    PixelsComputed := game_state.render_stats.PixelsComputed
    PixelsWasted := game_state.render_stats.PixelsWasted
    fmt.printfln("Fragments (%dx%d)\nPixels Skipped: %d\nPixels Computed: %d\nPixels Computation Wasted: %d(%.3f%%)",   StepXSize, StepYSize, 
                                                                                                                        game_state.render_stats.PixelsSkipped, 
                                                                                                                        PixelsComputed, 
                                                                                                                        PixelsWasted,
                                                                                                                        100 * f32(PixelsWasted) / f32(PixelsComputed))
}
}

rasterize_mesh :: proc (memory: ^common.Game_Memory, buffer: ^common.Game_Offscreen_Buffer, game_state: ^Game_State, mesh: ^Mesh) {
    assert(u64(size_of([2]i32) + size_of([3]f32) * 2) * u64(mesh.IndicesCount) <= memory.transient_storage_size)
//     void* AssignPointer = Memory->TransientStorage;
//     vector2i* TriangleVertices = (vector2i*)AssignPointer;
//     AssignPointer = (vector2i*)AssignPointer + Mesh->IndicesCount;    
    
//     vector3* TrianglePositions = (vector3*)AssignPointer;
//     AssignPointer = (vector3*)AssignPointer + Mesh->IndicesCount;    
    
//     vector3* TriangleNormals = (vector3*)AssignPointer;
//     AssignPointer = (vector3*)AssignPointer + Mesh->IndicesCount;
//     //    vector3 TrianglePositions[Mesh->IndicesCount];
//     //    vector3 TriangleNormals[Mesh->IndicesCount];
    
//     VertexStage(GameState, Mesh, 
//                 Buffer->Width, Buffer->Height, 
//                 TriangleVertices, TrianglePositions, TriangleNormals);
    
//     for (uint32_t i = 0; i < Mesh->IndicesCount; i+=3) 
//     {
//         vector2i V0 = TriangleVertices[i+0];
//         vector2i V1 = TriangleVertices[i+1];
//         vector2i V2 = TriangleVertices[i+2];
// #if SABLUJO_INTERNAL
//         ++GameState->RenderStats.TrianglesCount;
// #endif
        
//         int32_t MinX = MIN(V0.X, MIN(V1.X, V2.X));
//         int32_t MinY = MIN(V0.Y, MIN(V1.Y, V2.Y));
//         int32_t MaxX = MAX(V0.X, MAX(V1.X, V2.X));
//         int32_t MaxY = MAX(V0.Y, MAX(V1.Y, V2.Y));
        
//         // Clip against screen bounds
//         MinX = MAX(MinX, 0);
//         MinY = MAX(MinY, 0);
//         MaxX = MIN(MaxX, Buffer->Width - 1);
//         MaxY = MIN(MaxY, Buffer->Height - 1);
//         RasterizeRegion(GameState, Buffer, MinX, MinY, MaxX, MaxY, i, TriangleVertices, TrianglePositions, TriangleNormals);
//     }
}
