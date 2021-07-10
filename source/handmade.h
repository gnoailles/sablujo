#if !defined(HANDMADE_H)

#include <stdint.h>

#define internal static
#define local_persist static
#define global_variable static 

#define Kilobytes(Value) ((Value)*1024)
#define Megabytes(Value) (Kilobytes(Value)*1024)
#define Gigabytes(Value) (Megabytes(Value)*1024)
#define Terabytes(Value) (Gigabytes(Value)*1024)

#if HANDMADE_SLOW
// TODO: Complete assertion macro - don't worry everyone!
#define Assert(Expression) if(!(Expression)) {*(int32_t *)0 = 0;}
#else
#define Assert(Expression)
#endif


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
    matrix4 PerspectiveProjection;
    matrix4 World;
    float AspectRatio;
    bool IsInitialized;
};

namespace std
{
    class thread;
}

struct game_state
{
    camera Camera;
    float YRot;
};


typedef void game_update_and_render(game_memory* Memory, game_offscreen_buffer* Buffer);

#define HANDMADE_H
#endif