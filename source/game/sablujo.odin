package sablujo 

import "core:fmt"
import "core:mem"

import "../common"

//////////////////
// Game Specific
//////////////////

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
    VerticesCount: []u32,
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
    // _game_state: ^Game_State = transmute(^Game_State)(memory.permanent_storage)
    clear_buffer(buffer)
    fmt.println("Hello world")
    // SetupAndRenderRasterizer(Memory, Buffer, GameState)
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