#include "sablujo.h"

#include <stdio.h>
#include "win32_sablujo.h"
#include "dx12_renderer.h"

// TODO(Gouzi): Temporary global
global_variable bool IsRunning;
//global_variable win32_offscreen_buffer BackBuffer;

inline void
DEBUGWin32PrintLine(char* String)
{
    OutputDebugStringA(String);
}

inline FILETIME
Win32GetLastWriteTime(char *Filename)
{
    FILETIME LastWriteTime = {};
    
    WIN32_FIND_DATA FindData;
    HANDLE FindHandle = FindFirstFileA(Filename, &FindData);
    if(FindHandle != INVALID_HANDLE_VALUE)
    {
        LastWriteTime = FindData.ftLastWriteTime;
        FindClose(FindHandle);
    }
    
    return(LastWriteTime);
}

internal win32_game_code
Win32LoadGameCode(char *SourceDLLName, char *TempDLLName)
{
    win32_game_code Result = {};
    CopyFileA(SourceDLLName, TempDLLName, FALSE);
    Result.GameDLL = LoadLibraryA(TempDLLName);
    if(Result.GameDLL)
    {
        Result.DLLLastWriteTime = Win32GetLastWriteTime(SourceDLLName);
        Result.UpdateAndRender = (game_update_and_render*)GetProcAddress(Result.GameDLL, "GameUpdateAndRender");
    }
    return Result;
}

internal void
Win32UnloadGameCode(win32_game_code* GameCode)
{
    if(GameCode->GameDLL)
    {
        FreeLibrary(GameCode->GameDLL);
    }
    GameCode->UpdateAndRender = 0;
}

internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;
    return Result;
}

LRESULT CALLBACK 
MainWindowCallback(HWND Window, 
                   UINT Message, 
                   WPARAM WParam, 
                   LPARAM LParam)
{
    LRESULT Result = 0;
    switch (Message)
    {
        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;
        /*
        case WM_SIZE:
        {
            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            Win32ResizeDIBSection(&BackBuffer, Dimension.Width, Dimension.Height);
        } break;
        */
        case WM_CLOSE:
        {
            // TODO(Gouzi): Confirmation message to the user?
            IsRunning = false;
        } break;
        
        case WM_DESTROY:
        {
            // TODO(Gouzi): Handle as error - recreate window
            IsRunning = false;
        } break;
        
        default:
        {
            Result = DefWindowProcA(Window, Message, WParam, LParam);
        } break;
    }
    return Result;
}

internal void
CatStrings(size_t SourceACount, char *SourceA,
           size_t SourceBCount, char *SourceB,
           size_t DestCount, char *Dest)
{
    // TODO(Gouzi): Dest bounds checking!
    for(int32_t Index = 0;
        Index < SourceACount;
        ++Index)
    {
        *Dest++ = *SourceA++;
    }
    
    for(int32_t Index = 0;
        Index < SourceBCount;
        ++Index)
    {
        *Dest++ = *SourceB++;
    }
    
    *Dest++ = 0;
}

int32_t CALLBACK 
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CommandLine,
        int32_t ShowCode)
{
    // NOTE: Never use MAX_PATH in code that is user-facing, because it
    // can be dangerous and lead to bad results.
    char EXEFileName[MAX_PATH];
    DWORD SizeOfFilename = GetModuleFileNameA(0, EXEFileName, sizeof(EXEFileName));
    char *OnePastLastSlash = EXEFileName;
    for(char *Scan = EXEFileName;
        *Scan;
        ++Scan)
    {
        if(*Scan == '\\')
        {
            OnePastLastSlash = Scan + 1;
        }
    }
    
    char SourceGameCodeDLLFilename[] = "sablujo.dll";
    char SourceGameCodeDLLFullPath[MAX_PATH];
    CatStrings(OnePastLastSlash - EXEFileName, EXEFileName,
               sizeof(SourceGameCodeDLLFilename) - 1, SourceGameCodeDLLFilename,
               sizeof(SourceGameCodeDLLFullPath), SourceGameCodeDLLFullPath);
    
    char TempGameCodeDLLFilename[] = "sablujo_temp.dll";
    char TempGameCodeDLLFullPath[MAX_PATH];
    CatStrings(OnePastLastSlash - EXEFileName, EXEFileName,
               sizeof(TempGameCodeDLLFilename) - 1, TempGameCodeDLLFilename,
               sizeof(TempGameCodeDLLFullPath), TempGameCodeDLLFullPath);
    
    LARGE_INTEGER PerfCountFrequency;
    QueryPerformanceFrequency(&PerfCountFrequency);
    
    WNDCLASSA WindowClass = {};
    WindowClass.style = CS_HREDRAW|CS_VREDRAW;
    WindowClass.lpfnWndProc = MainWindowCallback;
    WindowClass.hInstance = Instance;
    WindowClass.lpszClassName = "SablujoWindowClass";
    
    if(RegisterClassA(&WindowClass))
    {
        int32_t DefaultWidth = 1280;
        int32_t DefaultHeight = 720;
        
        RECT WindowRect = {0, 0, DefaultWidth, DefaultHeight};
        AdjustWindowRect(&WindowRect, WS_OVERLAPPEDWINDOW|WS_VISIBLE, FALSE);
        
        HWND Window = 
            CreateWindowExA(0, 
                            WindowClass.lpszClassName, 
                            "Sablujo",
                            WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                            CW_USEDEFAULT,
                            CW_USEDEFAULT,
                            WindowRect.right - WindowRect.left,
                            WindowRect.bottom - WindowRect.top,
                            0,
                            0,
                            Instance,
                            0);
        if(Window)
        {
            
            // Init Renderer
            win32_window_dimension DefaultDimension = Win32GetWindowDimension(Window);
            viewport Viewport = {(uint32_t)DefaultDimension.Width, (uint32_t)DefaultDimension.Height};
            Assert(DefaultDimension.Width == DefaultWidth && DefaultDimension.Height == DefaultHeight);
            DX12InitRenderer(Window, DefaultDimension);
            
            // Init Memory
            game_memory GameMemory = {};
            GameMemory.PermanentStorageSize = Megabytes(64);
            GameMemory.TransientStorageSize = Gigabytes((uint64_t)1);
            uint64_t TotalSize = GameMemory.TransientStorageSize + GameMemory.PermanentStorageSize;
            
            GameMemory.Renderer.CreateVertexBuffer = &DX12CreateVertexBuffer;
#ifdef SABLUJO_INTERNAL
            LPVOID BaseAddress = (LPVOID)Terabytes((uint64_t)2);
            GameMemory.Platform.DEBUGFormatString = &sprintf_s;
            GameMemory.Platform.DEBUGPrintLine = &DEBUGWin32PrintLine;
#else
            LPVOID BaseAddress = 0;
#endif
            
            GameMemory.PermanentStorage = VirtualAlloc(BaseAddress, TotalSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            GameMemory.TransientStorage = (uint8_t*)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize;
            
            //Init Game
            win32_game_code Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);
            
            // Setup Game Loop
            IsRunning = true;
            LARGE_INTEGER LastCounter;
            QueryPerformanceCounter(&LastCounter);
            uint64_t LastCycleCount = __rdtsc();
            
            while(IsRunning)
            {
                FILETIME NewDLLWriteTime = Win32GetLastWriteTime(SourceGameCodeDLLFullPath);
                if(CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime) != 0)
                {
                    Win32UnloadGameCode(&Game);
                    Game = Win32LoadGameCode(SourceGameCodeDLLFullPath,
                                             TempGameCodeDLLFullPath);
                }
                
                MSG Message;
                while(PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if(Message.message == WM_QUIT)
                    {
                        IsRunning = false;
                    }
                    
                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                }
                
                if(Game.UpdateAndRender)
                {
                    Game.UpdateAndRender(&GameMemory, &Viewport);
                }
                
                DX12Render();
                DX12Present();
                
                uint64_t EndCycleCount = __rdtsc();
                LARGE_INTEGER EndCounter;
                QueryPerformanceCounter(&EndCounter);
                
                uint64_t CyclesElapsed = EndCycleCount - LastCycleCount;
                int64_t CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
                float MSPerFrame = (float)((1000.0f*(double)CounterElapsed) / (double)PerfCountFrequency.QuadPart);
                float FPS = PerfCountFrequency.QuadPart / (float)CounterElapsed;
                float MCPF = (CyclesElapsed / (1000.0f * 1000.0f));
                
                char PerformanceReportBuffer [256];
                sprintf_s(PerformanceReportBuffer, "%.02fms/f, %.02fFPS,  %.02fMc/f\n\n", MSPerFrame, FPS, MCPF);
                OutputDebugStringA(PerformanceReportBuffer);
                
                LastCycleCount = EndCycleCount;
                LastCounter = EndCounter;
            }
            DX12ShutdownRenderer();
        }
        else
        {
            // TODO(Gouzi): Logging
            DWORD Error = GetLastError();
            char ErrorMessage [128];
            sprintf_s(ErrorMessage, "Fatal: Error creating the Window, Error : %d\n", Error);
            OutputDebugStringA(ErrorMessage);
        }
        
    }
    else
    {
        // TODO(Gouzi): Logging
    }
    return 0;
}