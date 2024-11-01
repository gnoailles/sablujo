#include "sablujo.h"
#include "sablujo_geometry.h"
#include "sablujo_sse.h"
#include "../vendor/glm/glm.hpp"
#include "../vendor/glm/ext/matrix_clip_space.hpp"
//#include "../vendor/glm/ext/matrix_transform.hpp"
#include "../vendor/glm/gtx/transform.hpp"
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
    float HalfFOVRad = FOV * 0.5f * PI_FLOAT / 180.0f;
    Camera->AspectRatio = (float)Viewport->Width / (float)Viewport->Height; 
    
    float H = Cosine(0.5f * HalfFOVRad) / Sine( 0.5f * HalfFOVRad);
    float W = H * (float)Viewport->Height / (float)Viewport->Width;
    
    /*
    float Scale = Tangent(HalfFOVRad) * Near; 
    float Right = Camera->AspectRatio * Scale;
    float Left = -Right; 
    float Top = Scale;
    float Bottom = -Top; 
    */
    Camera->Projection = {};
    
    Camera->Projection.val[0][0] = W; 
    Camera->Projection.val[1][1] = H;
    Camera->Projection.val[2][2] = Far / (Far - Near); 
    Camera->Projection.val[2][3] = 1; 
    Camera->Projection.val[3][2] = -(Far * Near) / (Far - Near); 
    
    glm::mat4 GLMMatrix = glm::perspectiveFovLH_ZO(HalfFOVRad, (float)Viewport->Width, (float)Viewport->Height, Near, Far);
    
    Camera->View = {};
    Camera->View.val[0][0] = 1.0f; 
    Camera->View.val[1][1] = 1.0f; 
    Camera->View.val[2][2] = 1.0f; 
    Camera->View.val[3][3] = 1.0f;
    
    Camera->View.val[3][1] = 0.0f;
    Camera->View.val[3][2] = -2.0f;
    
    Camera->View = LookAt({0.5f, -0.5f, -5.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    /*float AngleRad = -10.0f * PI_FLOAT / 180.0f;
    matrix4 XRotMatrix = GetXRotationMatrix(AngleRad);
    Camera->View = MultMatrixMatrix(&Camera->View, &XRotMatrix);
    glm::mat4 GLMXRot = glm::rotate(AngleRad, glm::vec3{1.0f, 0.0f, 0.0f});
    glm::mat4 GLMView = glm::translate(glm::vec3{0.0f, 0.0f, -2.0f});
    GLMMatrix = GLMView * GLMXRot;
    */Camera->IsInitialized = true;
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

global_variable handle<mesh> CubeMesh;

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
        if(Memory->Renderer.CreateVertexBuffer != nullptr)
        {
            Assert(Memory->Renderer.CreateVertexBuffer);
            CubeMesh = Memory->Renderer.CreateVertexBuffer(&CubeVertices[0][0], CubeIndices, sizeof(float) * 7, CubeVerticesCount, CubeIndicesCount);
        }
    }
    else
    {
        if (Memory->Inputs.KeyStates & SABLUJO_KEY_W)
        {
            Camera->View.val[3][2] += -1.0f * Memory->DeltaTime;
        }
        if (Memory->Inputs.KeyStates & SABLUJO_KEY_A)
        {
            Camera->View.val[3][0] += 1.0f * Memory->DeltaTime;
        }
        if (Memory->Inputs.KeyStates & SABLUJO_KEY_S)
        {
            Camera->View.val[3][2] += 1.0f * Memory->DeltaTime;
        }
        if (Memory->Inputs.KeyStates & SABLUJO_KEY_D)
        {
            Camera->View.val[3][0] += -1.0f * Memory->DeltaTime;
        }
        if (Memory->Inputs.KeyStates & SABLUJO_KEY_Q)
        {
            Camera->View.val[3][1] += -1.0f * Memory->DeltaTime;
        }
        if (Memory->Inputs.KeyStates & SABLUJO_KEY_E)
        {
            Camera->View.val[3][1] += 1.0f * Memory->DeltaTime;
        }
        
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
    matrix4 ViewProj = MultMatrixMatrix(&Camera->Projection, &Camera->View);
    Memory->Renderer.SetViewProjection(&Camera->View.val[0][0], &Camera->Projection.val[0][0]);
    Assert(Memory->Renderer.SubmitForRender);
    
    matrix4 CubeTransform = GetIdentityMatrix();
    float InitialX = CubeTransform.val[3][0];
    matrix4 YOffsetMatrix = GetTranslationMatrix({0.0f, 1.25f, 0.0f});
    matrix4 XOffsetMatrix = GetTranslationMatrix({1.25f, 0.0f, 0.0f});
    for(uint32_t y = 0; y < 2; ++y)
    {
        for(uint32_t x = 0; x < 2; ++x)
        {
            Memory->Renderer.SubmitForRender({CubeMesh, CubeTransform});
            CubeTransform = MultMatrixMatrix(&CubeTransform, &XOffsetMatrix);
        }
        CubeTransform.val[3][0] = InitialX;
        CubeTransform = MultMatrixMatrix(&CubeTransform, &YOffsetMatrix);
    }
    
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
