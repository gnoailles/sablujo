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
VertexStage(game_state* GameState, mesh* Mesh,
            int32_t ScreenWidth, int32_t ScreenHeight, 
            vector2i* OutputVertices, vector3* OutputPositions, vector3* OutputNormals)
{
    for (uint32_t j = 0; j < Mesh->IndicesCount; j++) 
    {
        Assert(Mesh->Indices[j] < Mesh->VerticesCount);
        vector4 ModelVertex       = MultPointMatrix(&Mesh->Transform, &Mesh->Vertices[Mesh->Indices[j]]);
        vector4 CameraSpaceVertex = MultPointMatrix(&GameState->Camera.View, &ModelVertex);
        vector4 ProjectedVertex   = MultVecMatrix(&GameState->Camera.Projection, &CameraSpaceVertex);
        
        vector3 TransformedNormal = MultPointMatrix(&Mesh->InverseTransform, &Mesh->Normals[Mesh->Indices[j]]);
        
        // convert to raster space and mark the position of the vertex in the image with a simple dot
        int32_t x = MIN(ScreenWidth - 1, (int32_t)((ProjectedVertex.X + 1) * 0.5f * ScreenWidth));
        int32_t y = MIN(ScreenHeight - 1, (int32_t)((1 - (ProjectedVertex.Y + 1) * 0.5f) * ScreenHeight));
        
        OutputVertices[j]  = {x, y};
        OutputPositions[j] = vector3{ModelVertex.X, ModelVertex.Y, ModelVertex.Z};
        OutputNormals[j]   = TransformedNormal;
#if SABLUJO_INTERNAL
        ++GameState->RenderStats.VerticesCount;
#endif
    }
}

internal lane_v3 
FragmentStage(lane_v3 Position, lane_v3 Normal)
{
    lane_v3 LightPos = InitLaneV3(-3.0f, -8.0f, 0.0f);
    lane_v3 CamPos = {};                
    
    lane_v3 LightDir = LightPos - Position;
    lane_v3 CamDir = CamPos - Position;
    
    LightDir = Normalize(LightDir);
    CamDir = Normalize(CamDir);
    
    lane_v3 HalfAngles = CamDir + LightDir;
    HalfAngles = Normalize(HalfAngles);
    
    lane_f32 NdotL = DotProduct(Normal, LightDir);
    NdotL = Clamp(NdotL, LaneZeroF32, LaneOneF32);
    
    lane_f32 NdotH = DotProduct(Normal, HalfAngles);
    NdotH = Clamp(NdotH, LaneZeroF32, LaneOneF32);
    
    lane_f32 SpecularHighlight = Pow(NdotH, 32u);
    
    lane_v3 DiffuseCol = InitLaneV3(1.0f, 0.0f, 0.0f);
    lane_f32 LightIntensity = InitLaneF32(40.0f);
    
    lane_v3 Diffuse = DiffuseCol * NdotL * LightIntensity;
    
    lane_v3 SpecularColor = InitLaneV3(1.0f, 1.0f, 1.0f);
    lane_f32 SpecularIntensity = InitLaneF32(8.0f);
    
    lane_v3 Specular = SpecularColor * SpecularHighlight * SpecularIntensity;
    
    lane_v3 AmbientCol = InitLaneV3(0.1f, 0.0f, 0.0f);
    lane_v3 FinalColor = AmbientCol + Diffuse + Specular;
    
    FinalColor.X = LinearToSRGB(FinalColor.X);
    FinalColor.Y = LinearToSRGB(FinalColor.Y);
    FinalColor.Z = LinearToSRGB(FinalColor.Z);
    
    FinalColor.X = Min(FinalColor.X, LaneOneF32);
    FinalColor.Y = Min(FinalColor.Y, LaneOneF32);
    FinalColor.Z = Min(FinalColor.Z, LaneOneF32);
    return FinalColor;
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

inline float srgb_to_linear(float x) 
{
    return (x <= 0.04045f) ? x / 12.92f : powf((x + 0.055f) / 1.055f, 2.4f);
}

inline float linear_to_srgb(float x) 
{
    return (x <= 0.0031308f) ? x * 12.92f : 1.055f * powf(x, 1.0f / 2.4f);
}

struct edge 
{
    // Dimensions of our pixel group
    static const int StepXSize = LANE_WIDTH > 1 ? LANE_WIDTH / 2 : 1;
    static const int StepYSize = LANE_WIDTH > 1 ? LANE_WIDTH / StepXSize : 1;
    
    //static const int StepXSize = LANE_WIDTH;
    //static const int StepYSize = 1;
    
    lane_i32 OneStepX;
    lane_i32 OneStepY;
};

internal lane_i32
InitEdge(edge* Edge, const vector2i& V0, const vector2i&V1, const vector2i& Origin)
{
    // Edge setup
    int32_t A = V0.Y - V1.Y;
    int32_t B = V1.X - V0.X;
    int32_t C = V0.X * V1.Y - V0.Y * V1.X;
    
    // Step deltas
    Edge->OneStepX = InitLaneI32(A * edge::StepXSize);
    Edge->OneStepY = InitLaneI32(B * edge::StepYSize);
    
    // x/y values for initial pixel block
    int32_t XValues[LANE_WIDTH];
    int32_t YValues[LANE_WIDTH];
    int32_t LaneCounter = 0;
    for(int32_t YOffset = 0; YOffset < edge::StepYSize; ++YOffset)
    {
        for(int32_t XOffset = 0; XOffset < edge::StepXSize; ++XOffset)
        {
            XValues[LaneCounter] = Origin.X + XOffset;
            YValues[LaneCounter] = Origin.Y + YOffset;
            ++LaneCounter;
        }
    }
    
    
    
    lane_i32 x = LoadLaneI32(XValues);
    lane_i32 y = LoadLaneI32(YValues);
    
    // Edge function values at origin
    return A * x + B * y + InitLaneI32(C);
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
RasterizeRegion(game_state* GameState,
                game_offscreen_buffer* Buffer, 
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
    
    lane_i32 W0Row = InitEdge(&E12, V1, V2, P);
    lane_i32 W1Row = InitEdge(&E20, V2, V0, P);
    lane_i32 W2Row = InitEdge(&E01, V0, V1, P);
    
    for (int32_t j = StartHeight; j <= EndHeight; j += edge::StepYSize) 
    { 
        // Barycentric coordinates at start of row
        lane_i32 W0 = W0Row;
        lane_i32 W1 = W1Row;
        lane_i32 W2 = W2Row;
        for (int32_t i = StartWidth; i <= EndWidth; i += edge::StepXSize) 
        {
            lane_i32 Mask = LaneZeroI32 < (W0 | W1 | W2);
            if (!IsAllZeros(Mask)) 
            {
                lane_i32 MaskedW0;
                lane_i32 MaskedW1;
                lane_i32 MaskedW2;
                ConditionalAssign(W0, &MaskedW0, Mask);
                ConditionalAssign(W1, &MaskedW1, Mask);
                ConditionalAssign(W2, &MaskedW2, Mask);
                lane_f32 W0ratio = ConvertLaneI32ToF32(MaskedW0);
                lane_f32 W1ratio = ConvertLaneI32ToF32(MaskedW1);
                lane_f32 W2ratio = ConvertLaneI32ToF32(MaskedW2);
                
                lane_f32 AreaVec = InitLaneF32(Area);
                W0ratio = W0ratio / AreaVec;
                W1ratio = W1ratio / AreaVec;
                W2ratio = W2ratio / AreaVec;
                
                vector3 PositionsWide[LANE_WIDTH];
                vector3 NormalsWide[LANE_WIDTH];
                for(uint32_t k = 0; k < LANE_WIDTH; ++k)
                {
                    PositionsWide[k] = GetLane(W0ratio, k) * Positions[IndexOffset] + GetLane(W1ratio, k) * Positions[IndexOffset + 1] + GetLane(W2ratio, k) * Positions[IndexOffset + 2];
                    
                    NormalsWide[k] = GetLane(W0ratio, k) * Normals[IndexOffset] + GetLane(W1ratio, k) * Normals[IndexOffset + 1]   + GetLane(W2ratio, k) * Normals[IndexOffset + 2];
                }
                
                lane_v3 LanePositions = LoadLaneV3(PositionsWide);
                lane_v3 LaneNormals = LoadLaneV3(NormalsWide);
                LaneNormals = Normalize(LaneNormals);
                
                lane_v3 FragmentColor = FragmentStage(LanePositions, LaneNormals);
                
                int32_t LaneCount = 0;
#if SABLUJO_INTERNAL
                GameState->RenderStats.PixelsComputed += LANE_WIDTH;
                int32_t Waste = LANE_WIDTH;
#endif
                for(int32_t YOffset = 0; YOffset < edge::StepYSize; ++YOffset)
                {
                    for(int32_t XOffset = 0; XOffset < edge::StepXSize; ++XOffset)
                    {
                        if(GetLane(Mask, LaneCount))
                        {
                            ((uint32_t*)Buffer->Memory)[(j + YOffset) * Buffer->Width + i + XOffset] = ColorToUInt32({GetLane(FragmentColor.X, LaneCount), GetLane(FragmentColor.Y, LaneCount), GetLane(FragmentColor.Z, LaneCount)});
#if SABLUJO_INTERNAL
                            --Waste;
#endif
                        }
                        ++LaneCount;
                    }
                }
#if SABLUJO_INTERNAL
                GameState->RenderStats.PixelsWasted += Waste;
#endif
            }
#if SABLUJO_INTERNAL
            else
            {
                GameState->RenderStats.PixelsSkipped += edge::StepYSize * edge::StepXSize;
            }
#endif
            // One step to the right
            W0 += E12.OneStepX;
            W1 += E20.OneStepX;
            W2 += E01.OneStepX;       
        }
        
        // One row step
        W0Row += E12.OneStepY;
        W1Row += E20.OneStepY;
        W2Row += E01.OneStepY;
    }
}

internal void 
RasterizeMesh(game_state* GameState,
              game_memory* Memory, 
              game_offscreen_buffer* Buffer, 
              mesh* Mesh)
{
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
    
    VertexStage(GameState, Mesh, 
                Buffer->Width, Buffer->Height, 
                TriangleVertices, TrianglePositions, TriangleNormals);
    
    for (uint32_t i = 0; i < Mesh->IndicesCount; i+=3) 
    {
        vector2i V0 = TriangleVertices[i+0];
        vector2i V1 = TriangleVertices[i+1];
        vector2i V2 = TriangleVertices[i+2];
#if SABLUJO_INTERNAL
        ++GameState->RenderStats.TrianglesCount;
#endif
        
        int32_t MinX = MIN(V0.X, MIN(V1.X, V2.X));
        int32_t MinY = MIN(V0.Y, MIN(V1.Y, V2.Y));
        int32_t MaxX = MAX(V0.X, MAX(V1.X, V2.X));
        int32_t MaxY = MAX(V0.Y, MAX(V1.Y, V2.Y));
        
        // Clip against screen bounds
        MinX = MAX(MinX, 0);
        MinY = MAX(MinY, 0);
        MaxX = MIN(MaxX, Buffer->Width - 1);
        MaxY = MIN(MaxY, Buffer->Height - 1);
        RasterizeRegion(GameState, Buffer, MinX, MinY, MaxX, MaxY, i, TriangleVertices, TrianglePositions, TriangleNormals);
    }
}

extern "C" void GameUpdateAndRender(game_memory* Memory, game_offscreen_buffer* Buffer)
{
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
    game_state *GameState = (game_state *)Memory->PermanentStorage;
#if SABLUJO_INTERNAL
    GameState->RenderStats = {};
#endif
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
    GameState->YRot += .5f;
    
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
        RasterizeMesh(GameState, Memory, Buffer, &GameState->Meshes[i]);
    }
    
#if SABLUJO_INTERNAL
    uint32_t PixelsComputed = GameState->RenderStats.PixelsComputed;
    uint32_t PixelsWasted = GameState->RenderStats.PixelsWasted;
    char StatsMessage [256];
    
    Memory->Platform.DEBUGFormatString(StatsMessage,
                                       256,
                                       "Fragments (%dx%d)\nPixels Skipped: %d\nPixels Computed: %d\nPixels Computation Wasted: %d(%.3f%%)\n" , edge::StepXSize, edge::StepYSize, 
                                       GameState->RenderStats.PixelsSkipped, 
                                       PixelsComputed, 
                                       PixelsWasted,
                                       100.0f * (float)PixelsWasted / (float)PixelsComputed);
    Memory->Platform.DEBUGPrintLine(StatsMessage);
#endif
}