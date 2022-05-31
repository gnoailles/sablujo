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
    
    Camera->View.val[3][1] = -1.0f;
    Camera->View.val[3][2] = -2.0f;
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

global_variable mesh_handle CubeMesh;

extern "C" void GameUpdateAndRender(game_memory* Memory, viewport* Viewport)
{
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
    game_state *GameState = (game_state *)Memory->PermanentStorage;
#if SABLUJO_INTERNAL
    GameState->RenderStats = {};
#endif
    
    camera* Camera = &GameState->Camera;
    if(!Camera->IsInitialized)
    {
        InitializeCamera(Camera, Viewport);
    }
    
    if(Memory->Renderer.CreateVertexBuffer != nullptr && CubeMesh == INVALID_HANDLE)
    {
        Assert(Memory->Renderer.CreateVertexBuffer);
        CubeMesh = Memory->Renderer.CreateVertexBuffer(&CubeVertices[0][0], CubeIndices, sizeof(float) * 7, CubeVerticesCount, CubeIndicesCount);
    }
    
#if 0
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
#endif
    Assert(Memory->Renderer.SetViewProjection);
    matrix4 ViewProj = MultMatrixMatrix(&Camera->View, &Camera->Projection);
    Memory->Renderer.SetViewProjection(&ViewProj.val[0][0]);
    Assert(Memory->Renderer.SubmitForRender);
    Memory->Renderer.SubmitForRender(CubeMesh);
    Memory->Renderer.SubmitForRender(CubeMesh);
    Memory->Renderer.SubmitForRender(CubeMesh);
    Memory->Renderer.SubmitForRender(CubeMesh);
    
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
