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

struct platform_calls
{
#if SABLUJO_INTERNAL
    debug_platform_format_string* DEBUGFormatString;
    debug_platform_print_line* DEBUGPrintLine;
#endif
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

struct viewport
{
    uint32_t Width;
    uint32_t Height;
};

struct game_offscreen_buffer
{
    void* Memory;
    int32_t Width;
    int32_t Height;
    int32_t Pitch;
};

typedef void game_update_and_render(game_memory* Memory, viewport* Viewport);


//////////////////
// Game Specific
//////////////////

struct camera
{
    matrix4 View;
    matrix4 Projection;
    float AspectRatio;
    bool IsInitialized;
};

struct mesh
{
    vector3* Vertices;
    vector3* Normals;
    uint32_t* Indices;
    uint32_t VerticesCount;
    uint32_t IndicesCount;
    matrix4 Transform;
    matrix4 InverseTransform;
};

#define SPHERE_SUBDIV 28 
#define SPHERE_VERTEX_COUNT (SPHERE_SUBDIV * SPHERE_SUBDIV + 2)
#define SPHERE_INDEX_COUNT (SPHERE_SUBDIV * 3 * 2 + (SPHERE_SUBDIV - 1) * (SPHERE_SUBDIV - 1) * 6)

#if SABLUJO_INTERNAL
struct render_stats
{
    uint32_t VerticesCount;
    uint32_t TrianglesCount;
    uint32_t PixelsSkipped;
    uint32_t PixelsComputed;
    uint32_t PixelsWasted;
};
#endif
struct game_state
{
#if SABLUJO_INTERNAL
    render_stats RenderStats;
#endif
    camera Camera;
    mesh Meshes[2];
    float YRot;
};

#define SABLUJO_H
#endif