#+build windows
package sablujo_platform

import "core:sys/windows"
import "core:strings"
import "core:fmt"
import "core:dynlib"
import "core:mem"
import "core:log"
import "base:intrinsics"

import "../common"

Renderer_Type :: enum u8 {Dx12 = 0, Win32 = 1}
RENDERING_API :: #config(RENDERING_API, Renderer_Type.Win32)

when RENDERING_API == .Win32 {
    Win32_Offscreen_Buffer :: struct {
        info: windows.BITMAPINFO,
        memory: rawptr,
        width: i32,
        height: i32,
        pitch: i32,
        bytes_per_pixel: i32,
    }
} else {
    Win32_Offscreen_Buffer :: struct {
        memory: rawptr,
        width: i32,
        height: i32,
        pitch: i32,
        bytes_per_pixel: i32,
    }
}


Win32_Window_Dimension :: struct {
    width: i32,
    height: i32,
}

Game_Symbols :: struct {
    game_update_and_render: common.Game_Update_And_Render,
    game_dll: dynlib.Library,

    dll_name: string,
    dll_last_write_time: windows.FILETIME,
    version: int,
}

is_running := true


when RENDERING_API == .Win32 { 
win32_resize_dib_section :: proc (buffer: ^Win32_Offscreen_Buffer, width: i32, height: i32) {

    if buffer.memory != nil {
        windows.VirtualFree(buffer.memory, 0, windows.MEM_RELEASE)
    }
    
    buffer.width = width
    buffer.height = height
    buffer.bytes_per_pixel = 4
    buffer.pitch =  buffer.width * buffer.bytes_per_pixel
    
    // TODO(Gouzi): Maybe don't free first, free after and first if that fails.
    buffer.info.bmiHeader.biSize = size_of(buffer.info.bmiHeader)
    buffer.info.bmiHeader.biWidth = width
    buffer.info.bmiHeader.biHeight = -height
    buffer.info.bmiHeader.biPlanes = 1
    buffer.info.bmiHeader.biBitCount = 32
    buffer.info.bmiHeader.biCompression = windows.BI_RGB
    
    back_buffer_memory_size : uint = uint(buffer.bytes_per_pixel * width * height)
    buffer.memory = windows.VirtualAlloc(rawptr(uintptr(0)), back_buffer_memory_size, windows.MEM_RESERVE | windows.MEM_COMMIT, windows.PAGE_READWRITE)
}

Win32UpdateWindow :: proc (device_context: windows.HDC, size: Win32_Window_Dimension, buffer: ^Win32_Offscreen_Buffer) {
when false {

    padding_x := (size.width - buffer.width) / 2
    padding_y := (size.height - buffer.height) / 2
    windows.PatBlt(device_context, 0,                        0,                         padding_x,              size.height, windows.BLACKNESS)
    windows.PatBlt(device_context, padding_x + buffer.width, 0,                         padding_x,              size.height, windows.BLACKNESS)
    windows.PatBlt(device_context, padding_x,                0,                         size.width - padding_x, padding_y,   windows.BLACKNESS)
    windows.PatBlt(device_context, padding_x,                padding_y + buffer.height, size.width - padding_x, padding_y,   windows.BLACKNESS)
    StretchDIBits(device_context,
                    padding_x, padding_y, buffer.width, buffer.height,
                    0, 0, buffer.width, buffer.height,
                    buffer.memory,
                    &buffer.info,
                    windows.DIB_RGB_COLORS, windows.SRCCOPY)
} else {
    windows.StretchDIBits(device_context,
                            0, 0, buffer.width, buffer.height,
                            0, 0, buffer.width, buffer.height,
                            buffer.memory,
                            &buffer.info,
                            windows.DIB_RGB_COLORS, windows.SRCCOPY)
}
}
}

win32_get_last_write_time :: proc (filename: string) -> (windows.FILETIME, bool) {
    last_write_time : windows.FILETIME
    
    find_data : windows.WIN32_FIND_DATAW
    find_handle := windows.FindFirstFileW(windows.utf8_to_wstring(filename), &find_data)
    if find_handle != windows.INVALID_HANDLE_VALUE {
        last_write_time = find_data.ftLastWriteTime
        windows.FindClose(find_handle)
        return last_write_time, true
    }
    return last_write_time, false
    
}

win32_load_game_code :: proc (source_dll_name: string, api_version: int) -> Game_Symbols {

    dll_time, dll_time_ok := win32_get_last_write_time(source_dll_name)
    if !dll_time_ok {
        fmt.eprintln("Could not fetch last write date of ", source_dll_name)
        return {}
    }

    extension_pos := len(source_dll_name) - 4
    assert(source_dll_name[extension_pos:] == ".dll")
    dll_name := fmt.tprintf("{0}_{1}.dll", source_dll_name[:extension_pos], api_version)

    if !windows.CopyFileW(windows.utf8_to_wstring(source_dll_name), windows.utf8_to_wstring(dll_name), false) {
        fmt.eprintln("Failed to copy ", source_dll_name, "to", dll_name)
        return {}
    }
    
    dll, dll_ok := dynlib.load_library(dll_name)
    if !dll_ok {
        fmt.eprintln("Failed to load ", dll_name)
        return {}
    }
    
    result := Game_Symbols {
        game_update_and_render = cast(common.Game_Update_And_Render)(dynlib.symbol_address(dll, "game_update_and_render") or_else nil),
        game_dll = dll,
        dll_name = dll_name,
        dll_last_write_time = dll_time,
        version = api_version,

    }

    if result.game_update_and_render == nil {
        dynlib.unload_library(result.game_dll)
        fmt.eprintln("Game DLL missing required procedure")
        return {}
    }

    return result
}

win32_unload_game_code :: proc (game_code: Game_Symbols) {
    if game_code.game_dll != nil {
        dynlib.unload_library(game_code.game_dll)
    }

    
    if !windows.DeleteFileW(windows.utf8_to_wstring(game_code.dll_name)) {
        fmt.eprintln("Failed to delete", game_code.dll_name)
    }
}

win32_get_window_dimension :: proc (window: windows.HWND) -> Win32_Window_Dimension {
    result: Win32_Window_Dimension
    client_rect: windows.RECT
    windows.GetClientRect(window, &client_rect)
    result.width = client_rect.right - client_rect.left
    result.height = client_rect.bottom - client_rect.top
    return result
}

main_window_callback :: proc "stdcall" (window: windows.HWND, message: windows.UINT, w_param: windows.WPARAM, l_param: windows.LPARAM) -> windows.LRESULT {
    result : windows.LRESULT = 0
    switch (message) {
    case windows.WM_ACTIVATEAPP:
        windows.OutputDebugStringA("WM_ACTIVATEAPP\n")
    /*
    case WM_SIZE:
    {
        win32_window_dimension Dimension = Win32GetWindowDimension(Window);
        Win32ResizeDIBSection(&BackBuffer, Dimension.Width, Dimension.Height);
    } break;
    */
    case windows.WM_CLOSE:
        // TODO(Gouzi): Confirmation message to the user?
        is_running = false
    
    case windows.WM_DESTROY:
        // TODO(Gouzi): Handle as error - recreate window
        is_running = false
    
    case:
        result = windows.DefWindowProcA(window, message,w_param, l_param)
    }
    return result
}

main :: proc() {
    // NOTE: Never use MAX_PATH in code that is user-facing, because it
    // can be dangerous and lead to bad results.

when common.SABLUJO_INTERNAL {
    context.logger = log.create_console_logger()
    default_allocator := context.allocator
	tracking_allocator: mem.Tracking_Allocator
	mem.tracking_allocator_init(&tracking_allocator, default_allocator)
	context.allocator = mem.tracking_allocator(&tracking_allocator)

    reset_tracking_allocator :: proc(a: ^mem.Tracking_Allocator) -> bool {
		err := false
		for _, value in a.allocation_map {
			fmt.printf("%v: Leaked %v bytes\n", value.location, value.size)
			err = true
		}
		mem.tracking_allocator_clear(a)
		return err
	}

}
    
    exe_filename_w: [windows.MAX_PATH]windows.wchar_t
    size_of_filename := windows.GetModuleFileNameW(nil, raw_data(exe_filename_w[:]), len(exe_filename_w))
    exe_filename, err := windows.wstring_to_utf8(raw_data(exe_filename_w[:]), -1)
    assert(size_of_filename > 0 && err == .None)

    dir := exe_filename[:strings.last_index(exe_filename, "\\") + 1]
    source_game_code_dll_fullpath := strings.concatenate({dir, "sablujo.dll"})
    // temp_game_code_dll_fullpath := strings.concatenate({dir, "sablujo_temp.dll"})
    
    perf_count_frequency: windows.LARGE_INTEGER
    windows.QueryPerformanceFrequency(&perf_count_frequency)
    
    window_class : windows.WNDCLASSW
    window_class.style = windows.CS_HREDRAW | windows.CS_VREDRAW
    window_class.lpfnWndProc = main_window_callback
    window_class.hInstance = nil // TODO(Gouzi) : Use a proper instance
    window_class.lpszClassName = windows.utf8_to_wstring("SablujoWindowClass")
    
    if windows.RegisterClassW(&window_class) != 0 {
        default_width : i32 = 1920
        default_height : i32 = 1080

        window_rect: windows.RECT = {0, 0, default_width, default_height}
        windows.AdjustWindowRect(&window_rect, windows.WS_OVERLAPPEDWINDOW|windows.WS_VISIBLE, windows.FALSE)
        
        window := windows.CreateWindowExW(0, 
                                            window_class.lpszClassName, 
                                            windows.utf8_to_wstring("Sablujo"),
                                            windows.WS_OVERLAPPEDWINDOW | windows.WS_VISIBLE,
                                            windows.CW_USEDEFAULT,
                                            windows.CW_USEDEFAULT,
                                            window_rect.right - window_rect.left,
                                            window_rect.bottom - window_rect.top,
                                            nil,
                                            nil,
                                            nil, // TODO(Gouzi) : Use a proper instance
                                            nil)
        if window != nil {
            // Init Renderer
            default_dimension := win32_get_window_dimension(window)
            assert(default_dimension.width == default_width && default_dimension.height == default_height)
when RENDERING_API == .Win32 {
            back_buffer: Win32_Offscreen_Buffer
            win32_resize_dib_section(&back_buffer, default_width, default_height)
} else when RENDERING_API == .Dx12 {
            // back_buffer := DX12InitRenderer(Window, default_dimension)
}

            
            // Init Memory
            game_memory : common.Game_Memory
            game_memory.permanent_storage_size = common.Megabytes(u64(64))
            game_memory.transient_storage_size = common.Gigabytes(u64(1))
            total_size := game_memory.permanent_storage_size + game_memory.transient_storage_size
            
when RENDERING_API == .Dx12 {
            // GameMemory.Renderer.CreateVertexBuffer = &DX12CreateVertexBuffer
}
when common.SABLUJO_INTERNAL {
            base_address : windows.LPVOID = rawptr(uintptr(common.Terabytes(u64(2))))
            // game_memory.Platform.DEBUGFormatString = &sprintf_s
            // game_memory.Platform.DEBUGPrintLine = &DEBUGWin32PrintLine
} else {
            base_address : windows.LPVOID = nil
}
            
            game_memory.permanent_storage = windows.VirtualAlloc(base_address, uint(total_size), windows.MEM_RESERVE | windows.MEM_COMMIT, windows.PAGE_READWRITE)
            game_memory.transient_storage = rawptr(uintptr(game_memory.permanent_storage) + uintptr(game_memory.permanent_storage_size))
            
            //Init Game
            game_api_version := 0
            game := win32_load_game_code(source_game_code_dll_fullpath, game_api_version)
            defer dynlib.unload_library(game.game_dll)
            game_api_version += 1
            
            // Setup Game Loop
            is_running = true
            last_counter: windows.LARGE_INTEGER
            windows.QueryPerformanceCounter(&last_counter)
            last_cycle_count := intrinsics.read_cycle_counter()
            ms_per_frame : f32 = 1000/60
            for is_running {

                dll_time, dll_time_ok := win32_get_last_write_time(source_game_code_dll_fullpath)
                reload := dll_time_ok && (windows.CompareFileTime(&dll_time, &game.dll_last_write_time) != 0)

                if reload {
                    new_game := win32_load_game_code(source_game_code_dll_fullpath, game_api_version)
                    if new_game.game_dll != nil && new_game.game_update_and_render != nil {
                        win32_unload_game_code(game)
                        game = new_game
                        game_api_version += 1
                    }
                }
                
                message: windows.MSG
                for windows.PeekMessageA(&message, nil, 0, 0, windows.PM_REMOVE) {
                    if(message.message == windows.WM_QUIT) {
                        is_running = false
                    }
                    
                    windows.TranslateMessage(&message)
                    windows.DispatchMessageW(&message)
                }
                
                game_buffer : common.Game_Offscreen_Buffer
                game_buffer.memory = back_buffer.memory
                game_buffer.width = back_buffer.width
                game_buffer.height = back_buffer.height
                game_buffer.pitch = back_buffer.pitch
                if game.game_update_and_render != nil {
                    game.game_update_and_render(&game_memory, &game_buffer)
                }
                
when RENDERING_API == .Win32 {
                device_context := windows.GetDC(window)
                dimension := win32_get_window_dimension(window)
                Win32UpdateWindow(device_context, dimension, &back_buffer)
                windows.ReleaseDC(window, device_context)
} else when RENDERING_API == .Dx12 {
                DX12Render(&BackBuffer)
                DX12Present()
}
                
                
                end_cycle_count := intrinsics.read_cycle_counter()
                end_counter : windows.LARGE_INTEGER
                windows.QueryPerformanceCounter(&end_counter)
                
                cycles_elapsed := end_cycle_count - last_cycle_count
                counter_elapsed := end_counter - last_counter
                ms_per_frame = f32((1000 * f64(counter_elapsed)) / f64(perf_count_frequency))
                fps : f32 = f32(perf_count_frequency) / f32(counter_elapsed)
                mcpf : f32 = f32(cycles_elapsed) / (1000 * 1000)
                
                fmt.printfln("%.02fms/f, %.02fFPS,  %.02fMc/f\n", ms_per_frame, fps, mcpf)
                
                last_cycle_count = end_cycle_count
                last_counter = end_counter
            }
when RENDERING_API == .Dx12 {
            DX12ShutdownRenderer()
}
when common.SABLUJO_INTERNAL {
            reset_tracking_allocator(&tracking_allocator)
}
        } else {
            // TODO(Gouzi): Logging
            error := windows.GetLastError()
            fmt.println("Fatal: Error creating the Window, Error : %d", error)
        }
        
    } else {
        // TODO(Gouzi): Logging
    }
    return
}