#if !defined(SABLUJO_H)

#include <stdint.h>
#include "sablujo_defines.h"
#include "sablujo_maths.h"
#include "sablujo_memory.h"

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

struct mesh; 

struct mesh_instance
{
    handle<mesh> Mesh;
    matrix4 Transform;
};

struct camera
{
    matrix4 View;
    matrix4 Projection;
    float AspectRatio;
    bool IsInitialized;
};


typedef handle<mesh> create_vertex_buffer(float* Vertices, uint32_t* Indices, 
                                          uint32_t VertexSize, uint32_t VerticesCount, uint32_t IndicesCount);
typedef void set_view_projection(float* View, float* Projection);
typedef void submit_for_render(mesh_instance MeshInstance);
struct renderer_calls
{
    create_vertex_buffer* CreateVertexBuffer;
    set_view_projection* SetViewProjection;
    submit_for_render* SubmitForRender;
};

struct inputs
{
    uint8_t KeyStates;
};

struct game_memory
{
    uint64_t PermanentStorageSize;
    uint64_t TransientStorageSize;
    void* PermanentStorage;
    void* TransientStorage;
    
    float DeltaTime;
    inputs Inputs;
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
    float YRot;
};

#define SABLUJO_H
#endif