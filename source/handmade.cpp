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

struct edge {
    // Dimensions of our pixel group
    static const int StepXSize = 4;
    static const int StepYSize = 1;

    vector4i OneStepX;
    vector4i OneStepY;
};

vector4i InitEdge(edge* Edge, const vector2i& V0, const vector2i&V1, const vector2i& Origin)
{
    // Edge setup
    int32_t A = V0.Y - V1.Y;
    int32_t B = V1.X - V0.X;
    int32_t C = V0.X * V1.Y - V0.Y * V1.X;

    // Step deltas
    Edge->OneStepX = vector4i{A * edge::StepXSize, A * edge::StepXSize, A * edge::StepXSize, A * edge::StepXSize};
    Edge->OneStepY = vector4i{B * edge::StepYSize, B * edge::StepYSize, B * edge::StepYSize, B * edge::StepYSize};

    // x/y values for initial pixel block
    vector4i x = vector4i{Origin.X, Origin.X + 1, Origin.X + 2, Origin.X + 3};
    vector4i y = vector4i{Origin.Y, Origin.Y, Origin.Y, Origin.Y};

    // Edge function values at origin
    return A * x + B * y + vector4i{C,C,C,C};
}


int32_t EdgeFunction(vector2i A, vector2i B, vector2i C)
{
    return (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
}

float EdgeFunction(vector2 A, vector2 B, vector2 C)
{
    return (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
}
    
internal void 
RasterizeRegion(game_offscreen_buffer* Buffer, 
                int32_t StartWidth, int32_t StartHeight,
                int32_t EndWidth, int32_t EndHeight,
                vector2i V0, vector2i V1, vector2i V2)
{
    vector2i P = { StartWidth, StartHeight };

    edge E01, E12, E20;
    vector4i w0_row = InitEdge(&E12, V1, V2, P);
    vector4i w1_row = InitEdge(&E20, V2, V0, P);
    vector4i w2_row = InitEdge(&E01, V0, V1, P);

    for (int32_t j = StartHeight; j <= EndHeight; j += edge::StepYSize) 
    { 
          // Barycentric coordinates at start of row
        vector4i w0 = w0_row;
        vector4i w1 = w1_row;
        vector4i w2 = w2_row;
        for (int32_t i = StartWidth; i <= EndWidth; i += edge::StepXSize) 
        {
            vector4i mask = {};
            mask.vec = _mm_cmplt_epi32(mask.vec, _mm_or_si128(_mm_or_si128(w0.vec, w1.vec),w2.vec));
            if (!_mm_test_all_zeros(mask.vec, mask.vec)) 
            { 
                // uint32_t Area = EdgeFunction(V0, V1, V2);
                // color c0 = { 1.0f, 0.0f, 0.0f };
                // color c1 = { 0.0f, 1.0f, 0.0f };
                // color c2 = { 0.0f, 0.0f, 1.0f };

                color EndColor = {1.0f, 0.0f, 0.0f};
                // color EndColor;
                // EndColor.R = w0 * c0.R + w1 * c1.R + w2 * c2.R; 
                // EndColor.G = w0 * c0.G + w1 * c1.G + w2 * c2.G; 
                // EndColor.B = w0 * c0.B + w1 * c1.B + w2 * c2.B; 
                // ColorToUInt32(EndColor)
                if(mask.X)
                {
                    ((uint32_t*)Buffer->Memory)[j * Buffer->Width + i] = 0x00FF0000; 
                }
                if(mask.Y)
                {
                    ((uint32_t*)Buffer->Memory)[j * Buffer->Width + i + 1] = 0x00FF0000; 
                }
                if(mask.Z)
                {
                    ((uint32_t*)Buffer->Memory)[j * Buffer->Width + i + 2] = 0x00FF0000; 
                }
                if(mask.W)
                {
                    ((uint32_t*)Buffer->Memory)[j * Buffer->Width + i + 3] = 0x00FF0000; 
                }
            }
             // One step to the right
            w0.vec = _mm_add_epi32(w0.vec, E12.OneStepX.vec);
            w1.vec = _mm_add_epi32(w1.vec, E20.OneStepX.vec);
            w2.vec = _mm_add_epi32(w2.vec, E01.OneStepX.vec);
        }
        
        // One row step
        w0_row.vec = _mm_add_epi32(w0_row.vec, E12.OneStepY.vec);
        w1_row.vec = _mm_add_epi32(w1_row.vec, E20.OneStepY.vec);
        w2_row.vec = _mm_add_epi32(w2_row.vec, E01.OneStepY.vec);
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
    Translation.val[3][2] = -5.0f;

    Camera->World = multMatrixMatrix(&Rotation, &Translation);
    GameState->YRot += .5f;


    for (uint32_t i = 0; i < CubeNumVertices; i+=3) 
    {
        
        vector2i TriangleVertices[3];
        for (uint32_t j = 0; j < 3; j++) 
        {
            vector3 CameraSpaceVertex = multVecMatrix(&Camera->World, &CubeVertices[i + j]);
            vector3 ProjectedVertex = multVecMatrix(&Camera->PerspectiveProjection, &CameraSpaceVertex);

            // convert to raster space and mark the position of the vertex in the image with a simple dot
            int32_t x = MIN(Buffer->Width - 1, (int32_t)((ProjectedVertex.X + 1) * 0.5f * Buffer->Width));
            int32_t y = MIN(Buffer->Height - 1, (int32_t)((1 - (ProjectedVertex.Y + 1) * 0.5f) * Buffer->Height));

            TriangleVertices[j] = {x, y};
        }

        vector2i V0 = TriangleVertices[0];
        vector2i V1 = TriangleVertices[1];
        vector2i V2 = TriangleVertices[2];

        int32_t MinX = MIN(V0.X, MIN(V1.X, V2.X));
        int32_t MinY = MIN(V0.Y, MIN(V1.Y, V2.Y));
        int32_t MaxX = MAX(V0.X, MAX(V1.X, V2.X));
        int32_t MaxY = MAX(V0.Y, MAX(V1.Y, V2.Y));

        // Clip against screen bounds
        MinX = MAX(MinX, 0);
        MinY = MAX(MinY, 0);
        MaxX = MIN(MaxX, Buffer->Width - 1);
        MaxY = MIN(MaxY, Buffer->Height - 1);
        RasterizeRegion(Buffer, MinX, MinY, MaxX, MaxY, V0, V1, V2);
        // ((uint32_t*)Buffer->Memory)[V0.Y * Buffer->Width + V0.X] = 0x0000FFFF;
        // ((uint32_t*)Buffer->Memory)[V1.Y * Buffer->Width + V1.X] = 0x0000FFFF;
        // ((uint32_t*)Buffer->Memory)[V2.Y * Buffer->Width + V2.X] = 0x0000FFFF;
        // ((uint32_t*)Buffer->Memory)[(V0.Y + 1) * Buffer->Width + V0.X] = 0x0000FFFF;
        // ((uint32_t*)Buffer->Memory)[(V1.Y + 1) * Buffer->Width + V1.X] = 0x0000FFFF;
        // ((uint32_t*)Buffer->Memory)[(V2.Y + 1) * Buffer->Width + V2.X] = 0x0000FFFF;
        // ((uint32_t*)Buffer->Memory)[V0.Y * Buffer->Width + (V0.X + 1)] = 0x0000FFFF;
        // ((uint32_t*)Buffer->Memory)[V1.Y * Buffer->Width + (V1.X + 1)] = 0x0000FFFF;
        // ((uint32_t*)Buffer->Memory)[V2.Y * Buffer->Width + (V2.X + 1)] = 0x0000FFFF;
        // ((uint32_t*)Buffer->Memory)[(V0.Y + 1) * Buffer->Width + (V0.X + 1)] = 0x0000FFFF;
        // ((uint32_t*)Buffer->Memory)[(V1.Y + 1) * Buffer->Width + (V1.X + 1)] = 0x0000FFFF;
        // ((uint32_t*)Buffer->Memory)[(V2.Y + 1) * Buffer->Width + (V2.X + 1)] = 0x0000FFFF;
        
        // ((uint32_t*)Buffer->Memory)[(V0.Y - 1) * Buffer->Width + V0.X] = 0x0000FFFF;
        // ((uint32_t*)Buffer->Memory)[(V1.Y - 1) * Buffer->Width + V1.X] = 0x0000FFFF;
        // ((uint32_t*)Buffer->Memory)[(V2.Y - 1) * Buffer->Width + V2.X] = 0x0000FFFF;
        // ((uint32_t*)Buffer->Memory)[V0.Y * Buffer->Width + (V0.X - 1)] = 0x0000FFFF;
        // ((uint32_t*)Buffer->Memory)[V1.Y * Buffer->Width + (V1.X - 1)] = 0x0000FFFF;
        // ((uint32_t*)Buffer->Memory)[V2.Y * Buffer->Width + (V2.X - 1)] = 0x0000FFFF;
        // ((uint32_t*)Buffer->Memory)[(V0.Y - 1) * Buffer->Width + (V0.X - 1)] = 0x0000FFFF;
        // ((uint32_t*)Buffer->Memory)[(V1.Y - 1) * Buffer->Width + (V1.X - 1)] = 0x0000FFFF;
        // ((uint32_t*)Buffer->Memory)[(V2.Y - 1) * Buffer->Width + (V2.X - 1)] = 0x0000FFFF;
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