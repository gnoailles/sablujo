#if !defined(SABLUJO_H)

#include <stdint.h>
#include "sablujo_defines.h"
#include "sablujo_maths.h"

/////////////////////////
// Platform abstraction
/////////////////////////
typedef int32_t debug_platform_format_string(char* Buffer,
                                             size_t BufferSize,
                                             const char *format,
                                             ...);
typedef void debug_platform_print_line(char* String);
typedef vector2i platform_get_cursor_position();

struct platform_calls
{
#if SABLUJO_INTERNAL
    debug_platform_format_string* DEBUGFormatString;
    debug_platform_print_line* DEBUGPrintLine;
#endif
    platform_get_cursor_position* GetCursorPosition;
};

#ifdef INVALID_HANDLE
#undef INVALID_HANDLE
#endif

/*
#ifdef HANDLE
#undef HANDLE
#endif
*/
#if SABLUJO_INTERNAL
struct mesh_handle
{
    uint16_t Handle = UINT16_MAX;
};

inline bool
operator==(mesh_handle lhs, mesh_handle rhs)
{
    return lhs.Handle == rhs.Handle;
}
#define INVALID_HANDLE mesh_handle{UINT16_MAX}

#else
using mesh_handle = uint16_t;
#endif

typedef mesh_handle create_vertex_buffer(vector3* Vertices, vector3* Normals, uint32_t VerticesCount);

struct renderer_calls
{
    create_vertex_buffer* CreateVertexBuffer;
};

struct game_memory
{
    uint64_t PermanentStorageSize;
    uint64_t TransientStorageSize;
    void* PermanentStorage;
    void* TransientStorage;
    
    platform_calls Platform;
    renderer_calls Renderer;
};

struct game_offscreen_buffer
{
    void* Memory;
    int32_t Width;
    int32_t Height;
    int32_t Pitch;
};

typedef void game_update_and_render(game_memory* Memory, game_offscreen_buffer* Buffer, float dt);


//////////////////
// Game Specific
//////////////////
#define BOID_COUNT 2000
#include "quadtree.h"

struct boid
{
    vector2 Position;
    vector2 Direction;
};

struct game_state
{
    bool Init;
    //vector2 BoidsPositions[BOID_COUNT];
    //vector2 BoidsDirections[BOID_COUNT];
    boid Boids[BOID_COUNT];
    //quadtree QuadTree;
};

#define SABLUJO_H
#endif