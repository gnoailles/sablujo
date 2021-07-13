#if !defined(HANDMADE_H)

#include <stdint.h>
#include "handmade_defines.h"
#include "handmade_maths.h"

struct game_memory
{
    uint64_t PermanentStorageSize;
    uint64_t TransientStorageSize;
    void* PermanentStorage;
    void* TransientStorage;
};

struct game_offscreen_buffer
{
    void* Memory;
    int32_t Width;
    int32_t Height;
    int32_t Pitch;
};

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

namespace std
{
    class thread;
}

#define SPHERE_SUBDIV 28 
#define SPHERE_VERTEX_COUNT (SPHERE_SUBDIV * SPHERE_SUBDIV + 2)
#define SPHERE_INDEX_COUNT (SPHERE_SUBDIV * 3 * 2 + (SPHERE_SUBDIV - 1) * (SPHERE_SUBDIV - 1) * 6)
struct game_state
{
    camera Camera;
    mesh Meshes[2];
    float YRot;
};


typedef void game_update_and_render(game_memory* Memory, game_offscreen_buffer* Buffer);

#define HANDMADE_H
#endif