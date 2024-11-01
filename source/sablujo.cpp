#include "sablujo.h"
#include "sablujo_geometry.h"
#include "sablujo_sse.h"

// internal void
// RenderWeirdGradient(game_offscreen_buffer* Buffer, int32_t XOffset, int32_t YOffset)
// {
//     uint8_t* Row = (uint8_t*)Buffer->Memory;
//     for (int32_t Y = 0; Y < Buffer->Height; ++Y)
//     {
//         uint32_t* Pixel = (uint32_t*)Row;
//         for (int32_t X = 0; X < Buffer->Width; ++X)
//         {
//             *Pixel++ = (uint8_t)(X + XOffset) << 8 | (uint8_t)(Y + YOffset);
//         }
//         Row += Buffer->Pitch;
//     }
// }

using color = vector3;

internal inline uint32_t 
ColorToUInt32(color Color)
{
    return (uint32_t)(uint8_t(Color.X * 255) << 16 | uint8_t(Color.Y * 255) << 8 | uint8_t(Color.Z * 255));
}

internal void 
ClearBuffer(game_offscreen_buffer* Buffer)
{
    Assert(Buffer->Height * Buffer->Width % 2 == 0);
    
    uint64_t* DoublePixel = (uint64_t*)Buffer->Memory;
    uint64_t* EndPointer = &DoublePixel[Buffer->Height * Buffer->Width / 2];
    while(DoublePixel != EndPointer)
    {
        *DoublePixel++ = 0;
    }
}

internal void 
RenderRectangle(game_offscreen_buffer* Buffer, 
                vector2u Min, vector2u Max,
                color Color)
{
    for(uint32_t Y = Min.Y; Y < Max.Y; ++Y)
    {
        for(uint32_t X = Min.X; X < Max.X; ++X)
        {
            ((uint32_t*)Buffer->Memory)[Y * Buffer->Width + X] = ColorToUInt32(Color);
        }
    }
}

#include <time.h>
#include <stdlib.h>
#include <float.h>
unsigned int fp_state;
unsigned int fp_control_state = _controlfp_s(&fp_state, _EM_INEXACT, _MCW_EM);

float RandomUnilateral()
{
    return (float)rand() / (float)RAND_MAX;
}


float RandomBilateral()
{
    return 2.0f * RandomUnilateral() - 1.0f;
}

void
DrawBoid(game_offscreen_buffer* Buffer, vector2 Position)
{
    vector2u MinPos = vector2u{
        (uint32_t)Clamp(Position.X - 5, 0.0f, (float)Buffer->Width), 
        (uint32_t)Clamp(Position.Y - 5, 0.0f, (float)Buffer->Height)};
    vector2u MaxPos = vector2u{
        Clamp(MinPos.X + 10, 0, Buffer->Width), 
        Clamp(MinPos.Y + 10, 0, Buffer->Height)
    };
    
    if(MinPos.X >= 0 && MinPos.Y >= 0 &&
       MaxPos.X < (uint32_t)Buffer->Width && MaxPos.Y < (uint32_t)Buffer->Height)
    {
        RenderRectangle(Buffer, 
                        MinPos, 
                        MaxPos,
                        color{0.7f, 0.5f, 0.4f});
    }
}

#define ALIGNMENT_DISTANCE 200.0f
#define AVOIDANCE_DISTANCE 100.0f
#define MAX_STEERING_FORCE 1500.0f
#define MIN_SPEED 200.0f
#define MAX_SPEED 800.0f

vector2 SteerDirection(vector2 Steering, vector2 Direction)
{
    Steering= Normalize(Steering) * MAX_SPEED - Direction;
    if(MagnitudeSq(Steering) > MAX_STEERING_FORCE * MAX_STEERING_FORCE)
    {
        Steering = Normalize(Steering) * MAX_STEERING_FORCE;
    }
    return Steering;
}


extern "C" void GameUpdateAndRender(game_memory* Memory, game_offscreen_buffer* Buffer, float dt)
{
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    ClearBuffer(Buffer);
    //time_t t;
    //srand((unsigned)time(&t));
    
    uint32_t BoidsCount = ArrayCount(GameState->Boids);
    if(!GameState->Init)
    {
        for(uint32_t i = 0; i < BoidsCount; ++i)
        {
            GameState->Boids[i].Position = vector2{RandomUnilateral() * Buffer->Width, RandomUnilateral() * Buffer->Height};
            GameState->Boids[i].Direction = Normalize(vector2{RandomUnilateral(), RandomUnilateral() * 0.3f}) * MAX_SPEED;
        }
        
        GameState->Init = true;
    }
    
    ResetQuadtree();
    quadtree QuadTree{AABB{vector2{}, vector2{(float)Buffer->Width, (float)Buffer->Height}}};
    for(uint32_t i = 0; i < BoidsCount; ++i)
    {
        QuadTree.Insert(&GameState->Boids[i]);
    }
    
    vector2i MousePos = Memory->Platform.GetCursorPosition();
    vector2 Target = vector2{(float)MousePos.X, (float)MousePos.Y};
    
    bool validTarget = Target.X >= 0 && Target.X < Buffer->Width &&
        Target.Y >= 0 && Target.Y < Buffer->Height;
    validTarget = false;
    
    boid** FoundBoids = (boid**)Memory->TransientStorage;
    
    for(uint32_t i = 0; i < BoidsCount; ++i)
    {
        uint32_t FlockCount = 0;
        vector2 AverageAlignmentDirection = {};
        vector2 AverageAvoidanceDirection = {};
        vector2 FlockCenter = {};
        vector2 Acceleration = {};
        
        QuadTree.QueryRange(AABB{GameState->Boids[i].Position - ALIGNMENT_DISTANCE * 0.5f, 
                                GameState->Boids[i].Position + ALIGNMENT_DISTANCE * 0.5f}, 
                            FoundBoids, &FlockCount);
        
        
        for(uint32_t j = 0; j < FlockCount; ++j)
        {
            if(i == j)
                continue;
            
            vector2 BoidsOffset = GameState->Boids[i].Position - GameState->Boids[j].Position;
            float DistanceSq = MagnitudeSq(BoidsOffset);
            
            AverageAlignmentDirection+= GameState->Boids[j].Direction;
            FlockCenter += GameState->Boids[j].Position;
            if(DistanceSq < AVOIDANCE_DISTANCE * AVOIDANCE_DISTANCE)
            {
                AverageAvoidanceDirection += BoidsOffset / DistanceSq;
            }
        }
        if(validTarget)
        {
            Acceleration = SteerDirection(Target - GameState->Boids[i].Position, GameState->Boids[i].Direction);
        }
        
        
        if(FlockCount > 0)
        {
            Acceleration += SteerDirection(AverageAlignmentDirection, GameState->Boids[i].Direction);
            Acceleration += SteerDirection(AverageAvoidanceDirection, GameState->Boids[i].Direction);
            
            FlockCenter /= (float)FlockCount;
            FlockCenter -= GameState->Boids[i].Position;
            Acceleration += SteerDirection(FlockCenter, GameState->Boids[i].Direction);
        }
        GameState->Boids[i].Direction += Acceleration * dt;
    }
    
    for(uint32_t i = 0; i < BoidsCount; ++i)
    {
        float SpeedSq = MagnitudeSq(GameState->Boids[i].Direction);
        if(SpeedSq > (MAX_SPEED * MAX_SPEED))
        {
            GameState->Boids[i].Direction = Normalize(GameState->Boids[i].Direction) * MAX_SPEED;
        }
        else if (SpeedSq < MIN_SPEED * MIN_SPEED)
        {
            GameState->Boids[i].Direction = Normalize(GameState->Boids[i].Direction) * MIN_SPEED;
        }
        
        GameState->Boids[i].Position += GameState->Boids[i].Direction * dt;
        
        if(GameState->Boids[i].Position.X < 0)
        {
            GameState->Boids[i].Position.X += Buffer->Width;
        }
        else if(GameState->Boids[i].Position.X >= Buffer->Width)
        {
            GameState->Boids[i].Position.X -= Buffer->Width;
        }
        
        if(GameState->Boids[i].Position.Y < 0)
        {
            GameState->Boids[i].Position.Y += Buffer->Height;
        }
        else if(GameState->Boids[i].Position.Y >= Buffer->Height)
        {
            GameState->Boids[i].Position.Y -= Buffer->Height;
        }
    }
    
    //Alignment
    vector2u MinPos = vector2u{
        (uint32_t)Clamp(GameState->Boids[0].Position.X - ALIGNMENT_DISTANCE * 0.5f, 0.0f, (float)Buffer->Width), 
        (uint32_t)Clamp(GameState->Boids[0].Position.Y - ALIGNMENT_DISTANCE * 0.5f, 0.0f, (float)Buffer->Height)};
    vector2u MaxPos = vector2u{
        Clamp(MinPos.X + (uint32_t)ALIGNMENT_DISTANCE, 0, Buffer->Width), 
        Clamp(MinPos.Y + (uint32_t)ALIGNMENT_DISTANCE, 0, Buffer->Height)
    };
    RenderRectangle(Buffer, 
                    MinPos, 
                    MaxPos,
                    color{0.2f, 0.2f, 0.2f});
    
    //Avoidance
    MinPos = vector2u{
        (uint32_t)Clamp(GameState->Boids[0].Position.X - AVOIDANCE_DISTANCE * 0.5f, 0.0f, (float)Buffer->Width), 
        (uint32_t)Clamp(GameState->Boids[0].Position.Y - AVOIDANCE_DISTANCE * 0.5f, 0.0f, (float)Buffer->Height)};
    MaxPos = vector2u{
        Clamp(MinPos.X + (uint32_t)AVOIDANCE_DISTANCE, 0, Buffer->Width), 
        Clamp(MinPos.Y + (uint32_t)AVOIDANCE_DISTANCE, 0, Buffer->Height)
    };
    RenderRectangle(Buffer, 
                    MinPos, 
                    MaxPos,
                    color{0.8f, 0.2f, 0.2f});
    
    // Boids
    for(uint32_t i = 0; i < BoidsCount; ++i)
    {
        DrawBoid(Buffer, GameState->Boids[i].Position);
    }
    
    // Cursor
    MinPos = vector2u{
        (uint32_t)Clamp(Target.X - 2.0f, 0.0f, (float)Buffer->Width), 
        (uint32_t)Clamp(Target.Y - 2.0f, 0.0f, (float)Buffer->Height)};
    MaxPos = vector2u{
        Clamp(MinPos.X + 4, 0, Buffer->Width), 
        Clamp(MinPos.Y + 4, 0, Buffer->Height)
    };
    RenderRectangle(Buffer, 
                    MinPos, 
                    MaxPos,
                    color{1.0, 0.0, 0.0});
    /*
    //Alignment range boids
    vector2 MinRange = GameState->Boids[0].Position - ALIGNMENT_DISTANCE * 0.5f;
    vector2 MaxRange = GameState->Boids[0].Position + ALIGNMENT_DISTANCE * 0.5f;
    
    uint32_t FlockCount = 0;
    QuadTree.QueryRange(AABB{MinRange, 
                            MaxRange}, 
                        FoundBoids, &FlockCount);
    
    
    for(uint32_t j = 0; j < FlockCount; ++j)
    {
        MinPos = vector2u{
            (uint32_t)Clamp(FoundBoids[j]->Position.X - 5, 0.0f, (float)Buffer->Width), 
            (uint32_t)Clamp(FoundBoids[j]->Position.Y - 5, 0.0f, (float)Buffer->Height)};
        MaxPos = vector2u{
            Clamp(MinPos.X + 10, 0, Buffer->Width), 
            Clamp(MinPos.Y + 10, 0, Buffer->Height)
        };
        
        if(MinPos.X >= 0 && MinPos.Y >= 0 &&
           MaxPos.X < (uint32_t)Buffer->Width && MaxPos.Y < (uint32_t)Buffer->Height)
        {
            RenderRectangle(Buffer, 
                            MinPos, 
                            MaxPos,
                            color{0.0f, 1.0f, 0.0f});
        }
    }
    */
}