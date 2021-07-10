#if !defined(WIN32_HANDMADE_H)

struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void* Memory;
    int32_t Width;
    int32_t Height;
    int32_t Pitch;
    int32_t BytesPerPixel;
};

struct win32_window_dimension
{
    int32_t Width;
    int32_t Height;
};

struct win32_game_code
{
    HMODULE GameDLL;
    FILETIME DLLLastWriteTime;
    game_update_and_render* UpdateAndRender;
};


#define WIN32_HANDMADE_H
#endif