#if !defined(HANDMADE_H)

#include <stdint.h>
#include "handmade_defines.h"
#include "handmade_maths.h"

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
#if HANDMADE_INTERNAL
    debug_platform_format_string* DEBUGFormatString;
    debug_platform_print_line* DEBUGPrintLine;
#endif
};

struct game_memory
{
    uint64_t PermanentStorageSize;
    uint64_t TransientStorageSize;
    void* PermanentStorage;
    void* TransientStorage;
    
    platform_calls Platform;
};

struct game_offscreen_buffer
{
    void* Memory;
    int32_t Width;
    int32_t Height;
    int32_t Pitch;
};

typedef void game_update_and_render(game_memory* Memory, game_offscreen_buffer* Buffer);

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
    vector4* Vertices;
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

#if HANDMADE_INTERNAL
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
#if HANDMADE_INTERNAL
    render_stats RenderStats;
#endif
    camera Camera;
    mesh Meshes[2];
    float YRot;
};

#define HANDMADE_H
#endif