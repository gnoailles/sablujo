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

internal void 
VertexStage(game_state* GameState, mesh* Mesh,
            int32_t ScreenWidth, int32_t ScreenHeight, 
            vector2i* OutputVertices, vector3* OutputPositions, vector3* OutputNormals)
{
    for (uint32_t j = 0; j < Mesh->IndicesCount; j++) 
    {
        Assert(Mesh->Indices[j] < Mesh->VerticesCount);
        vector4 Vertex = vector4(Mesh->Vertices[Mesh->Indices[j]], 1.0f);
        vector4 ModelVertex       = MultPointMatrix(&Mesh->Transform, &Vertex);
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

internal inline uint32_t 
ColorToUInt32(color Color)
{
    return (uint32_t)(uint8_t(Color.X * 255) << 16 | uint8_t(Color.Y * 255) << 8 | uint8_t(Color.Z * 255));
}

internal void 
InitializeCamera(camera* Camera, viewport* Viewport)
{
    float FOV = 90.0f; 
    float Near = 0.1f; 
    float Far = 100.0f; 
    Camera->AspectRatio = (float)Viewport->Width / (float)Viewport->Height; 
    
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

global_variable mesh_handle CubeVertexBuffer;

extern "C" void GameUpdateAndRender(game_memory* Memory, viewport* Viewport)
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
    
    /*
    if(Memory->Renderer.CreateVertexBuffer != nullptr && CubeVertexBuffer == INVALID_HANDLE)
    {
        CubeVertexBuffer = Memory->Renderer.CreateVertexBuffer(CubeVertices, CubeNormals, CubeVerticesCount);
    }
    */
    vector3* SphereVertices = (vector3*)((uint8_t*)Memory->PermanentStorage + sizeof(game_state));
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
        InitializeCamera(Camera, Viewport);
        CreateSphere(SPHERE_SUBDIV, SPHERE_SUBDIV, 
                     Sphere->Vertices, Sphere->Normals, Sphere->Indices, 
                     SPHERE_VERTEX_COUNT, SPHERE_INDEX_COUNT);
    }
    
    
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
    
#if SABLUJO_INTERNAL
    /*
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
*/
#endif
}