#include "handmade.h"
#include "geometry.h"

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
        // uint32_t* Pixel = (uint32_t*)(&((uint8_t*)Buffer->Memory)[Y * Buffer->Pitch + Min.X]);
        for(uint32_t X = Min.X; X < Max.X; ++X)
        {
            ((uint32_t*)Buffer->Memory)[Y * Buffer->Width + X] = ColorToUInt32(Color);
            // *Pixel++ = ColorToUInt32(Color);
        }
    }
}

internal void 
InitializeCamera(camera* Camera, int32_t ImageWidth, int32_t ImageHeight)
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
    
    Camera->Projection = {};
    
    Camera->Projection.val[0][0] = 2 * Near / (Right - Left); 
    Camera->Projection.val[1][1] = 2 * Near / (Top - Bottom);
    
    Camera->Projection.val[2][0] = (Right + Left) / (Right - Left); 
    Camera->Projection.val[2][1] = (Top + Bottom) / (Top - Bottom); 
    Camera->Projection.val[2][2] = -(Far + Near) / (Far - Near); 
    Camera->Projection.val[2][3] = -1; 
    
    
    Camera->Projection.val[3][2] = -2 * Far * Near / (Far - Near); 
    
    Camera->View = {};
    Camera->View.val[0][0] = 1.0f; 
    Camera->View.val[1][1] = 1.0f; 
    Camera->View.val[2][2] = 1.0f; 
    Camera->View.val[3][3] = 1.0f;
    
    Camera->View.val[3][1] = 0.0f;
    Camera->View.val[3][2] = 0.0f;
    float AngleRad = -10.0f * PI_FLOAT / 180.0f;
    matrix4 XRotMatrix = GetXRotationMatrix(AngleRad);
    Camera->View = MultMatrixMatrix(&Camera->View, &XRotMatrix);
    Camera->IsInitialized = true;
}

internal void 
VertexStage(camera* Camera, mesh* Mesh,
            int32_t ScreenWidth, int32_t ScreenHeight, 
            vector2i* OutputVertices, vector3* OutputPositions, vector3* OutputNormals)
{
    for (uint32_t j = 0; j < Mesh->IndicesCount; j++) 
    {
        Assert(Mesh->Indices[j] < Mesh->VerticesCount);
        vector4 ModelVertex       = MultPointMatrix(&Mesh->Transform, &Mesh->Vertices[Mesh->Indices[j]]);
        vector4 CameraSpaceVertex = MultPointMatrix(&Camera->View, &ModelVertex);
        vector4 ProjectedVertex   = MultVecMatrix(&Camera->Projection, &CameraSpaceVertex);
        
        vector3 TransformedNormal = MultPointMatrix(&Mesh->InverseTransform, &Mesh->Normals[Mesh->Indices[j]]);
        
        // convert to raster space and mark the position of the vertex in the image with a simple dot
        int32_t x = MIN(ScreenWidth - 1, (int32_t)((ProjectedVertex.X + 1) * 0.5f * ScreenWidth));
        int32_t y = MIN(ScreenHeight - 1, (int32_t)((1 - (ProjectedVertex.Y + 1) * 0.5f) * ScreenHeight));
        
        OutputVertices[j]  = {x, y};
        OutputPositions[j] = vector3{ModelVertex.X, ModelVertex.Y, ModelVertex.Z};
        OutputNormals[j]   = TransformedNormal;
    }
}

vector3 LightPosition = {-3.0f, -8.0f, 0.0f};
const color LightColor = {0.3f, 1.0f, 0.4f};
const float LightPower = 40.0;
const float SpecularCoefficient = 8.0;

const vector3 CamPosition = {0.0f, 0.0f, 0.0f};

const float Shininess = 32.0;

const color AmbientColor = {0.1f, 0.0f, 0.0f};
const color DiffuseColor = {1.0f, 0.0f, 0.0f};
const color SpecColor = {1.0f, 1.0f, 1.0f};

const float ScreenGamma = 2.2f;
const float InvScreenGamma = 1.0f / ScreenGamma;

internal color 
FragmentStage(vector3 WorldPosition, vector3 Normal)
{
#if 1
    vector3 LightDir = LightPosition - WorldPosition;
    vector3 CamDir = CamPosition - WorldPosition;
    float Distance = MagnitudeSq(&LightDir);
    NormalizeVector(&LightDir);
    NormalizeVector(&CamDir);
    
    vector3 HalfAngle = CamDir + LightDir;
    NormalizeVector(&HalfAngle);
    
    float NdotL = MIN(MAX(DotProduct(&Normal, &LightDir), 0.0f), 1.0f);
    float NdotH = MIN(MAX(DotProduct(&Normal, &HalfAngle), 0.0f), 1.0f);
    float SpecularHighlight = powf(NdotH, Shininess);
    
    vector3 Difffuse = DiffuseColor * NdotL * LightPower;
    vector3 Specular = (SpecColor * SpecularHighlight) * SpecularCoefficient;
    vector3 ColorLinear = AmbientColor + Difffuse + Specular;
    //#define FAST_POW 0
#ifdef FAST_POW
    vector4 ColorLinear4;
    ColorLinear4.vec = ColorLinear.vec;
    vector4 ColorGammaCorrected = FastPositivePower(ColorLinear4, InvScreenGamma);
#else
    // apply gamma correction (assume ambientColor, diffuseColor and specColor
    // have been linearized, i.e. have no gamma correction in them)
    vector3 ColorGammaCorrected = {powf(ColorLinear.X, InvScreenGamma), powf(ColorLinear.Y, InvScreenGamma), powf(ColorLinear.Z, InvScreenGamma)};
#endif
    //    return color{MAX(MIN(Normal.X, 1.0f), 0.0f), MAX(MIN(Normal.Y, 1.0f),0.0f), MAX(MIN(Normal.Z, 1.0f), 0.0f)};
    return color{MIN(ColorGammaCorrected.X, 1.0f), MIN(ColorGammaCorrected.Y, 1.0f), MIN(ColorGammaCorrected.Z, 1.0f)};
#else
    return color{1.0f, 0.0f, 0.0f};
#endif
}

struct edge {
    // Dimensions of our pixel group
    static const int StepXSize = 4;
    static const int StepYSize = 1;
    
    vector4i OneStepX;
    vector4i OneStepY;
};

internal vector4i 
InitEdge(edge* Edge, const vector2i& V0, const vector2i&V1, const vector2i& Origin)
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


internal int32_t 
EdgeFunction(vector2i A, vector2i B, vector2i C)
{
    return (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
}

internal float 
EdgeFunction(vector2 A, vector2 B, vector2 C)
{
    return (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
}

internal void 
RasterizeRegion(game_offscreen_buffer* Buffer, 
                int32_t StartWidth, int32_t StartHeight,
                int32_t EndWidth, int32_t EndHeight,
                uint32_t IndexOffset,
                vector2i* ScreenPositions,
                vector3* Positions,
                vector3* Normals)
{
    
    vector2i V0 = ScreenPositions[IndexOffset + 0];
    vector2i V1 = ScreenPositions[IndexOffset + 1];
    vector2i V2 = ScreenPositions[IndexOffset + 2];
    vector2i P = { StartWidth, StartHeight };
    
    float Area = (float)EdgeFunction(V0, V1, V2);
    
    edge E01, E12, E20;
    vector4i W0Row = InitEdge(&E12, V1, V2, P);
    vector4i W1Row = InitEdge(&E20, V2, V0, P);
    vector4i W2Row = InitEdge(&E01, V0, V1, P);
    
    for (int32_t j = StartHeight; j <= EndHeight; j += edge::StepYSize) 
    { 
        // Barycentric coordinates at start of row
        vector4i W0 = W0Row;
        vector4i W1 = W1Row;
        vector4i W2 = W2Row;
        for (int32_t i = StartWidth; i <= EndWidth; i += edge::StepXSize) 
        {
            vector4i Mask = {};
            Mask.vec = _mm_cmplt_epi32(Mask.vec, _mm_or_si128(_mm_or_si128(W0.vec, W1.vec),W2.vec));
            if (!_mm_test_all_zeros(Mask.vec, Mask.vec)) 
            {
                color EndColor = { 1.0f, 0.0f, 0.0f };
                color c0 = { 1.0f, 0.0f, 0.0f };
                color c1 = { 0.0f, 1.0f, 0.0f };
                color c2 = { 0.0f, 0.0f, 1.0f };
                
                __m128 W0ratio = _mm_cvtepi32_ps(W0.vec);
                __m128 W1ratio = _mm_cvtepi32_ps(W1.vec);
                __m128 W2ratio = _mm_cvtepi32_ps(W2.vec);
                
                __m128 AreaVec = _mm_set_ps1 (Area);
                W0ratio = _mm_div_ps(W0ratio, AreaVec);
                W1ratio = _mm_div_ps(W1ratio, AreaVec);
                W2ratio = _mm_div_ps(W2ratio, AreaVec);
                
                if(Mask.X)
                {
                    vector3 position = W0ratio.m128_f32[0] * Positions[IndexOffset] + W1ratio.m128_f32[0] * Positions[IndexOffset + 1] + W2ratio.m128_f32[0] * Positions[IndexOffset + 2];
                    vector3 normal   = W0ratio.m128_f32[0] * Normals[IndexOffset]   + W1ratio.m128_f32[0] * Normals[IndexOffset + 1]   + W2ratio.m128_f32[0] * Normals[IndexOffset + 2];
                    NormalizeVector(&normal);
                    ((uint32_t*)Buffer->Memory)[j * Buffer->Width + i] = ColorToUInt32(FragmentStage(position, normal));
                }
                if(Mask.Y)
                {
                    vector3 position = W0ratio.m128_f32[1] * Positions[IndexOffset] + W1ratio.m128_f32[1] * Positions[IndexOffset + 1] + W2ratio.m128_f32[1] * Positions[IndexOffset + 2];
                    vector3 normal   = W0ratio.m128_f32[1] * Normals[IndexOffset]   + W1ratio.m128_f32[1] * Normals[IndexOffset + 1]   + W2ratio.m128_f32[1] * Normals[IndexOffset + 2];
                    NormalizeVector(&normal);
                    ((uint32_t*)Buffer->Memory)[j * Buffer->Width + i + 1] = ColorToUInt32(FragmentStage(position, normal));
                }
                if(Mask.Z)
                {
                    vector3 position = W0ratio.m128_f32[2] * Positions[IndexOffset] + W1ratio.m128_f32[2] * Positions[IndexOffset + 1] + W2ratio.m128_f32[2] * Positions[IndexOffset + 2];
                    vector3 normal   = W0ratio.m128_f32[2] * Normals[IndexOffset]   + W1ratio.m128_f32[2] * Normals[IndexOffset + 1]   + W2ratio.m128_f32[2] * Normals[IndexOffset + 2];
                    NormalizeVector(&normal);
                    ((uint32_t*)Buffer->Memory)[j * Buffer->Width + i + 2] = ColorToUInt32(FragmentStage(position, normal));
                }
                if(Mask.W)
                {
                    vector3 position = W0ratio.m128_f32[3] * Positions[IndexOffset] + W1ratio.m128_f32[3] * Positions[IndexOffset + 1] + W2ratio.m128_f32[3] * Positions[IndexOffset + 2];
                    vector3 normal   = W0ratio.m128_f32[3] * Normals[IndexOffset]   + W1ratio.m128_f32[3] * Normals[IndexOffset + 1]   + W2ratio.m128_f32[3] * Normals[IndexOffset + 2];
                    NormalizeVector(&normal);
                    ((uint32_t*)Buffer->Memory)[j * Buffer->Width + i + 3] = ColorToUInt32(FragmentStage(position, normal));
                }
            }
            // One step to the right
            W0.vec = _mm_add_epi32(W0.vec, E12.OneStepX.vec);
            W1.vec = _mm_add_epi32(W1.vec, E20.OneStepX.vec);
            W2.vec = _mm_add_epi32(W2.vec, E01.OneStepX.vec);       
        }
        
        // One row step
        W0Row.vec = _mm_add_epi32(W0Row.vec, E12.OneStepY.vec);
        W1Row.vec = _mm_add_epi32(W1Row.vec, E20.OneStepY.vec);
        W2Row.vec = _mm_add_epi32(W2Row.vec, E01.OneStepY.vec);
    }
}

internal void RasterizeMesh(game_memory* Memory, 
                            game_offscreen_buffer* Buffer, 
                            camera* Camera, mesh* Mesh)
{
    /*const uint32_t IndexCount = SPHERE_INDEX_COUNT;
    uint32_t* Indices = Sphere->Indices;
    
    const uint32_t VertexCount = SPHERE_VERTEX_COUNT;
    vector4* Vertices = GameState->SphereVertices;
    const uint32_t NormalCount = SPHERE_VERTEX_COUNT;
    vector3* Normals = GameState->SphereNormals;*/
    Assert((sizeof(vector2i) + sizeof(vector3) * 2) * Mesh->IndicesCount <= Memory->TransientStorageSize);
    void* AssignPointer = Memory->TransientStorage;
    vector2i* TriangleVertices = (vector2i*)AssignPointer;
    AssignPointer = (vector2i*)AssignPointer + Mesh->IndicesCount;    
    
    vector3* TrianglePositions = (vector3*)AssignPointer;
    AssignPointer = (vector3*)AssignPointer + Mesh->IndicesCount;    
    
    vector3* TriangleNormals = (vector3*)AssignPointer;
    AssignPointer = (vector3*)AssignPointer + Mesh->IndicesCount;
    //    vector3 TrianglePositions[Mesh->IndicesCount];
    //    vector3 TriangleNormals[Mesh->IndicesCount];
    
    VertexStage(Camera, Mesh, 
                Buffer->Width, Buffer->Height, 
                TriangleVertices, TrianglePositions, TriangleNormals);
    
    for (uint32_t i = 0; i < Mesh->IndicesCount; i+=3) 
    {
        vector2i V0 = TriangleVertices[i+0];
        vector2i V1 = TriangleVertices[i+1];
        vector2i V2 = TriangleVertices[i+2];
        
        int32_t MinX = MIN(V0.X, MIN(V1.X, V2.X));
        int32_t MinY = MIN(V0.Y, MIN(V1.Y, V2.Y));
        int32_t MaxX = MAX(V0.X, MAX(V1.X, V2.X));
        int32_t MaxY = MAX(V0.Y, MAX(V1.Y, V2.Y));
        
        // Clip against screen bounds
        MinX = MAX(MinX, 0);
        MinY = MAX(MinY, 0);
        MaxX = MIN(MaxX, Buffer->Width - 1);
        MaxY = MIN(MaxY, Buffer->Height - 1);
        RasterizeRegion(Buffer, MinX, MinY, MaxX, MaxY, i, TriangleVertices, TrianglePositions, TriangleNormals);
        
#if 0
        for(uint32_t j = 0; j < 3; ++j)
        {           
            //            NormalizeVector(&TriangleNormals[i + j]);
            vector3 NPos = TrianglePositions[i + j] + TriangleNormals[i + j];
            vector4 NormalPosition  = {NPos.X, NPos.Y, NPos.Z, 1.0f};
            
            vector4 CameraSpaceVertex  = MultPointMatrix(&Camera->View, &NormalPosition);
            vector4 ProjectedVertex    = MultVecMatrix(&Camera->Projection, &CameraSpaceVertex);
            
            int32_t x = MAX(0, MIN(Buffer->Width - 1, (int32_t)((ProjectedVertex.X + 1) * 0.5f * Buffer->Width)));
            int32_t y = MAX(0, MIN(Buffer->Height - 1, (int32_t)((1 - (ProjectedVertex.Y + 1) * 0.5f) * Buffer->Height)));
            ((uint32_t*)Buffer->Memory)[y * Buffer->Width + x] = 0x00FF0000;
        }    
        
        if((V0.Y >= 0 && V0.Y < Buffer->Height) &&
           (V0.X >= 0 && V0.X < Buffer->Width))
        {
            ((uint32_t*)Buffer->Memory)[V0.Y * Buffer->Width + V0.X] = 0x0000FFFF;
        }
        if((V1.Y >= 0 && V1.Y < Buffer->Height) &&
           (V1.X >= 0 && V1.X < Buffer->Width))
        {
            ((uint32_t*)Buffer->Memory)[V1.Y * Buffer->Width + V1.X] = 0x0000FFFF;
        }
        if((V2.Y >= 0 && V2.Y < Buffer->Height) &&
           (V2.X >= 0 && V2.X < Buffer->Width))
        {
            ((uint32_t*)Buffer->Memory)[V2.Y * Buffer->Width + V2.X] = 0x0000FFFF;
        }
        /*        ((uint32_t*)Buffer->Memory)[(V0.Y + 1) * Buffer->Width + V0.X] = 0x0000FFFF;
                ((uint32_t*)Buffer->Memory)[(V1.Y + 1) * Buffer->Width + V1.X] = 0x0000FFFF;
                ((uint32_t*)Buffer->Memory)[(V2.Y + 1) * Buffer->Width + V2.X] = 0x0000FFFF;
                ((uint32_t*)Buffer->Memory)[V0.Y * Buffer->Width + (V0.X + 1)] = 0x0000FFFF;
                ((uint32_t*)Buffer->Memory)[V1.Y * Buffer->Width + (V1.X + 1)] = 0x0000FFFF;
                ((uint32_t*)Buffer->Memory)[V2.Y * Buffer->Width + (V2.X + 1)] = 0x0000FFFF;
                ((uint32_t*)Buffer->Memory)[(V0.Y + 1) * Buffer->Width + (V0.X + 1)] = 0x0000FFFF;
                ((uint32_t*)Buffer->Memory)[(V1.Y + 1) * Buffer->Width + (V1.X + 1)] = 0x0000FFFF;
                ((uint32_t*)Buffer->Memory)[(V2.Y + 1) * Buffer->Width + (V2.X + 1)] = 0x0000FFFF;
                
                ((uint32_t*)Buffer->Memory)[(V0.Y - 1) * Buffer->Width + V0.X] = 0x0000FFFF;
                ((uint32_t*)Buffer->Memory)[(V1.Y - 1) * Buffer->Width + V1.X] = 0x0000FFFF;
                ((uint32_t*)Buffer->Memory)[(V2.Y - 1) * Buffer->Width + V2.X] = 0x0000FFFF;
                ((uint32_t*)Buffer->Memory)[V0.Y * Buffer->Width + (V0.X - 1)] = 0x0000FFFF;
                ((uint32_t*)Buffer->Memory)[V1.Y * Buffer->Width + (V1.X - 1)] = 0x0000FFFF;
                ((uint32_t*)Buffer->Memory)[V2.Y * Buffer->Width + (V2.X - 1)] = 0x0000FFFF;
                ((uint32_t*)Buffer->Memory)[(V0.Y - 1) * Buffer->Width + (V0.X - 1)] = 0x0000FFFF;
                ((uint32_t*)Buffer->Memory)[(V1.Y - 1) * Buffer->Width + (V1.X - 1)] = 0x0000FFFF;
                ((uint32_t*)Buffer->Memory)[(V2.Y - 1) * Buffer->Width + (V2.X - 1)] = 0x0000FFFF;*/
#endif
    }
}

extern "C" void GameUpdateAndRender(game_memory* Memory, game_offscreen_buffer* Buffer)
{
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    camera* Camera = &GameState->Camera;
    GameState->Meshes[0] = {};
    GameState->Meshes[1] = {};
    mesh* Cube = &GameState->Meshes[0];
    mesh* Sphere = &GameState->Meshes[1];
    
    Cube->Vertices = CubeVertices;
    Cube->Normals = CubeNormals;
    Cube->Indices = CubeIndices;
    Cube->VerticesCount = CubeVerticesCount;
    Cube->IndicesCount = CubeIndicesCount;
    
    vector4* SphereVertices = (vector4*)((uint8_t*)Memory->PermanentStorage + sizeof(game_state));
    vector3* SphereNormals = (vector3*)(SphereVertices + sizeof(vector4) * SPHERE_VERTEX_COUNT);
    uint32_t* SphereIndices = (uint32_t*)(SphereNormals + sizeof(vector3) * SPHERE_VERTEX_COUNT);
    Assert((uintptr_t)(SphereIndices + SPHERE_INDEX_COUNT) <= (uintptr_t)Memory->PermanentStorage + Memory->PermanentStorageSize);
    
    Sphere->Vertices = SphereVertices;
    Sphere->Normals = SphereNormals;
    Sphere->Indices = SphereIndices;
    Sphere->VerticesCount = SPHERE_VERTEX_COUNT;
    Sphere->IndicesCount = SPHERE_INDEX_COUNT;
    
    if(!Camera->IsInitialized)
    {
        InitializeCamera(Camera, Buffer->Width, Buffer->Height);
        CreateSphere(SPHERE_SUBDIV, SPHERE_SUBDIV, 
                     Sphere->Vertices, Sphere->Normals, Sphere->Indices, 
                     SPHERE_VERTEX_COUNT, SPHERE_INDEX_COUNT);
    }
    
    ClearBuffer(Buffer);
    
    float AngleRad = 0.0f + GameState->YRot * PI_FLOAT / 180.0f;
    matrix4 YRotMatrix = GetYRotationMatrix(AngleRad);
    AngleRad = 0.0f * PI_FLOAT / 180.0f;
    matrix4 XRotMatrix = GetXRotationMatrix(AngleRad);
    matrix4 Rotation = MultMatrixMatrix(&YRotMatrix,&XRotMatrix);;
    //GameState->YRot += .5f;
    
    matrix4 Translation = {};
    Translation.val[0][0] = 1.0f;
    Translation.val[1][1] = 1.0f;
    Translation.val[2][2] = 1.0f;
    Translation.val[3][3] = 1.0f;
    
    Translation.val[3][0] = -1.0f;
    Translation.val[3][1] = 0.5f;
    Translation.val[3][2] = 2.0f;
    
    Sphere->Transform = MultMatrixMatrix(&Rotation, &Translation);
    Sphere->InverseTransform = InverseMatrix(&Sphere->Transform);
    Sphere->InverseTransform = TransposeMatrix(&Sphere->InverseTransform);
    
    Translation.val[3][0] = 1.0f;
    Translation.val[3][1] = 0.0f;
    Translation.val[3][2] = 2.0f;
    Cube->Transform = MultMatrixMatrix(&Rotation, &Translation);
    Cube->InverseTransform = InverseMatrix(&Cube->Transform);
    Cube->InverseTransform = TransposeMatrix(&Cube->InverseTransform);
    
    for(uint32_t i = 0; i < ArrayCount(GameState->Meshes); ++i)
    {
        RasterizeMesh(Memory, Buffer, Camera, &GameState->Meshes[i]);
    }
    
#if HANDMADE_MULTITHREADING
    float ThreadCountSqrt = SquareRoot((float)std::thread::hardware_concurrency());
    uint32_t ThreadPerSide = (uint32_t)ThreadCountSqrt;
    uint32_t ThreadCount = ThreadPerSide * ThreadPerSide;
    
    int32_t ThreadRegionWidth = Buffer->Width / ThreadPerSide;
    int32_t ThreadRegionHeight = Buffer->Height / ThreadPerSide;
    
    Assert(ThreadCount <= std::thread::hardware_concurrency());
    Assert(Memory->TransientStorageSize >= sizeof(std::thread) * ThreadCount);
    std::thread* Threads = (std::thread*)Memory->TransientStorage;
    for (uint32_t i = 0; i < ThreadPerSide; ++i)
    {
        for (uint32_t j = 0; j < ThreadPerSide; ++j)
        {
            Threads[i * ThreadPerSide + j] = std::move(std::thread(RasterizeRegion, Buffer, ThreadRegionWidth * i, ThreadRegionHeight * j, ThreadRegionWidth * (i + 1), ThreadRegionHeight * (j + 1)));
        }
    }
    
    for (uint32_t i = 0; i < ThreadCount; ++i)
    {
        Threads[i].join();
    }
#else
    // RasterizeRegion(0, 0, Buffer->Width, Buffer->Height);
#endif
}