#include "handmade.h"

#include <thread>

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

struct color
{
    float R;
    float G;
    float B;
};

internal uint32_t ColorToUInt32(color Color)
{
    return (uint32_t)(uint8_t(Color.R * 255) << 16 | uint8_t(Color.G * 255) << 8 | uint8_t(Color.B * 255));
}

internal void ClearBuffer(game_offscreen_buffer* Buffer)
{
    Assert(Buffer->Height * Buffer->Width % 2 == 0);

    uint64_t* DoublePixel = (uint64_t*)Buffer->Memory;
    uint64_t* EndPointer = &DoublePixel[Buffer->Height * Buffer->Width / 2];
    while(DoublePixel != EndPointer)
    {
        *DoublePixel++ = 0;
    }
}

internal void RenderRectangle(game_offscreen_buffer* Buffer, 
                              vector2u Min, vector2u Max,
                              color Color)
{
    for(uint32_t Y = Min.Y; Y < Max.Y; ++Y)
    {
        // uint32_t* Pixel = (uint32_t*)(&((uint8_t*)Buffer->Memory)[Y * Buffer->Pitch + Min.X]);
        for(uint32_t X = Min.X; X < Max.X; ++X)
        {
            ((uint32_t*)Buffer->Memory)[Y * Buffer->Width + X] = ColorToUInt32(Color);
            // *Pixel++ = ColorToUInt32(Color);
        }
    }
}

internal void InitializeCamera(camera* Camera, int32_t ImageWidth, int32_t ImageHeight)
{
    float FOV = 90.0f; 
    float Near = 0.1f; 
    float Far = 100.0f; 
    Camera->AspectRatio = (float)ImageWidth / (float)ImageHeight; 

    // float ScaleATanRad[4] = {};
    float HalfFOVRad = FOV * 0.5f * PI_FLOAT / 180.0f;
    float Scale = Tangent(HalfFOVRad) * Near; 
    float Right = Camera->AspectRatio * Scale;
    float Left = -Right; 
    float Top = Scale;
    float Bottom = -Top; 

    Camera->PerspectiveProjection = {};

    Camera->PerspectiveProjection.val[0][0] = 2 * Near / (Right - Left); 
    // Camera->PerspectiveProjection.val[0][1] = 0.0f; 
    // Camera->PerspectiveProjection.val[0][2] = 0.0f; 
    // Camera->PerspectiveProjection.val[0][3] = 0.0f; 

    // Camera->PerspectiveProjection.val[1][0] = 0.0f; 
    Camera->PerspectiveProjection.val[1][1] = 2 * Near / (Top - Bottom); 
    // Camera->PerspectiveProjection.val[1][2] = 0.0f; 
    // Camera->PerspectiveProjection.val[1][3] = 0.0f; 

    Camera->PerspectiveProjection.val[2][0] = (Right + Left) / (Right - Left); 
    Camera->PerspectiveProjection.val[2][1] = (Top + Bottom) / (Top - Bottom); 
    Camera->PerspectiveProjection.val[2][2] = -(Far + Near) / (Far - Near); 
    Camera->PerspectiveProjection.val[2][3] = -1; 

    // Camera->PerspectiveProjection.val[3][0] = 0.0f; 
    // Camera->PerspectiveProjection.val[3][1] = 0.0f; 
    Camera->PerspectiveProjection.val[3][2] = -2 * Far * Near / (Far - Near); 
    // Camera->PerspectiveProjection.val[3][3] = 0.0f;
    Camera->IsInitialized = true;

    Camera->World = {};
    Camera->World.val[0][0] = 1.0f; 
    Camera->World.val[1][1] = 1.0f; 
    Camera->World.val[2][2] = 1.0f; 
    Camera->World.val[3][3] = 1.0f;

    float AngleRad = 40.0f * PI_FLOAT / 180.0f;
    matrix4 YRotMatrix = GetYRotationMatrix(AngleRad);
    AngleRad = -20.0f * PI_FLOAT / 180.0f;
    matrix4 XRotMatrix = GetXRotationMatrix(AngleRad);
    matrix4 Rotation = multMatrixMatrix(&YRotMatrix,&XRotMatrix);
    Camera->World = multMatrixMatrix(&Rotation, &Camera->World);
}


float EdgeFunction(vector2 A, vector2 B, vector2 C)
{
    return (C.X - A.X) * (B.Y - A.Y) - (C.Y - A.Y) * (B.X - A.X);
}
    
internal void 
RasterizeRegion(game_offscreen_buffer* Buffer, 
                int32_t StartWidth, int32_t StartHeight,
                int32_t EndWidth, int32_t EndHeight,
                vector2 V0, vector2 V1, vector2 V2)
{
    for (int32_t j = StartHeight; j < EndHeight; ++j) 
    { 
        for (int32_t i = StartWidth; i < EndWidth; ++i) 
        { 
            vector2 p = {i + 0.5f, j + 0.5f}; 
            float w0 = EdgeFunction(V1, V2, p); 
            float w1 = EdgeFunction(V2, V0, p); 
            float w2 = EdgeFunction(V0, V1, p); 
            if (w0 >= 0 && w1 >= 0 && w2 >= 0) 
            { 
                float Area = EdgeFunction(V0, V1, V2);
                w0 /= Area; 
                w1 /= Area; 
                w2 /= Area; 
                color c0 = { 1.0f, 0.0f, 0.0f };
                color c1 = { 0.0f, 1.0f, 0.0f };
                color c2 = { 0.0f, 0.0f, 1.0f };

                color EndColor;
                EndColor.R = w0 * c0.R + w1 * c1.R + w2 * c2.R; 
                EndColor.G = w0 * c0.G + w1 * c1.G + w2 * c2.G; 
                EndColor.B = w0 * c0.B + w1 * c1.B + w2 * c2.B; 
                ((uint32_t*)Buffer->Memory)[j * Buffer->Width + i] = ColorToUInt32(EndColor); 
            } 
        }
    }
}


#include "teapot_vertices.h"

extern "C" void GameUpdateAndRender(game_memory* Memory, game_offscreen_buffer* Buffer)
{
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    camera* Camera = &GameState->Camera;
    
    if(!Camera->IsInitialized)
    {
        InitializeCamera(Camera, Buffer->Width, Buffer->Height);
    }
    ClearBuffer(Buffer);
    // vector2u Min = {5, 5};
    // vector2u Max = {500, 500};
    // color Color = {};
    // Color.R = 0.9f;
    // Color.G = 0.5f;
    // Color.B = 0.1f;
    // RenderRectangle(Buffer, Min, Max, Color);
    // RenderWeirdGradient(Buffer, GameState->XOffset++, GameState->YOffset++);

    float AngleRad = GameState->YRot * PI_FLOAT / 180.0f;
    matrix4 YRotMatrix = GetYRotationMatrix(AngleRad);
    AngleRad = -45.0f * PI_FLOAT / 180.0f;
    matrix4 XRotMatrix = GetXRotationMatrix(AngleRad);
    matrix4 Rotation = multMatrixMatrix(&YRotMatrix,&XRotMatrix);

    matrix4 Translation = {};
    Translation.val[0][0] = 1.0f;
    Translation.val[1][1] = 1.0f;
    Translation.val[2][2] = 1.0f;
    Translation.val[3][3] = 1.0f;
    Translation.val[3][1] = 0.5f;
    Translation.val[3][2] = -2.5f;

    Camera->World = multMatrixMatrix(&Rotation, &Translation);
    GameState->YRot += 2.5f;


    for (uint32_t i = 0; i < CubeNumVertices; i+=3) 
    {
        
        vector2 TriangleVertices[3];
        for (uint32_t j = 0; j < 3; j++) 
        {
            vector3 CameraSpaceVertex = multVecMatrix(&Camera->World, &CubeVertices[i + j]);
            vector3 ProjectedVertex = multVecMatrix(&Camera->PerspectiveProjection, &CameraSpaceVertex);

            // convert to raster space and mark the position of the vertex in the image with a simple dot
            uint32_t x = MIN((uint32_t)Buffer->Width - 1, (uint32_t)((ProjectedVertex.X + 1) * 0.5 * Buffer->Width)); 
            uint32_t y = MIN((uint32_t)Buffer->Height - 1, (uint32_t)((1 - (ProjectedVertex.Y + 1) * 0.5) * Buffer->Height)); 
            // ((uint32_t*)Buffer->Memory)[y * Buffer->Width + x] = 0xFFFFFFFF;

            TriangleVertices[j] = {(float)x, (float)y};
        }

        vector2 V0 = TriangleVertices[0];
        vector2 V1 = TriangleVertices[1];
        vector2 V2 = TriangleVertices[2];

        int32_t MinX = (int32_t)MIN(V0.X, MIN(V1.X, V2.X));
        int32_t MinY = (int32_t)MIN(V0.Y, MIN(V1.Y, V2.Y));
        int32_t MaxX = (int32_t)MAX(V0.X, MAX(V1.X, V2.X));
        int32_t MaxY = (int32_t)MAX(V0.Y, MAX(V1.Y, V2.Y));

        // Clip against screen bounds
        MinX = MAX(MinX, 0);
        MinY = MAX(MinY, 0);
        MaxX = MIN(MaxX, Buffer->Width - 1);
        MaxY = MIN(MaxY, Buffer->Height - 1);
        RasterizeRegion(Buffer, MinX, MinY, MaxX, MaxY, V0, V1, V2);
    }


#if HANDMADE_MULTITHREADING
    float ThreadCountSqrt = sqrtf((float)std::thread::hardware_concurrency());
    uint32_t ThreadPerSide = (uint32_t)ThreadCountSqrt;
    uint32_t ThreadCount = ThreadPerSide * ThreadPerSide;

    int32_t ThreadRegionWidth = Buffer->Width / ThreadPerSide;
    int32_t ThreadRegionHeight = Buffer->Height / ThreadPerSide;

    Assert(ThreadCount <= std::thread::hardware_concurrency());
    Assert(Memory->TransientStorageSize >=  sizeof(std::thread) * ThreadCount);
    std::thread* Threads =  (std::thread*)Memory->TransientStorage;
    for(uint32_t i = 0; i < ThreadPerSide; ++i)
    {
        for(uint32_t j = 0; j < ThreadPerSide; ++j)
        {
             Threads[i * ThreadPerSide + j] = std::move(std::thread(RasterizeRegion, Buffer, ThreadRegionWidth * i, ThreadRegionHeight * j, ThreadRegionWidth * (i + 1), ThreadRegionHeight * (j + 1)));
        }
    }

     for(uint32_t i = 0; i < ThreadCount; ++i)
     {
          Threads[i].join();
     }
#else
    // RasterizeRegion(0, 0, Buffer->Width, Buffer->Height);
#endif
}