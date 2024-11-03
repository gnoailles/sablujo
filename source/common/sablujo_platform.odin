package sablujo_common 

Game_Update_And_Render :: #type proc (^Game_Memory, ^Game_Offscreen_Buffer)

Game_Memory :: struct
{
    permanent_storage_size: u64,
    transient_storage_size: u64,
    permanent_storage: rawptr,
    transient_storage: rawptr,
}

Game_Offscreen_Buffer :: struct
{
    memory: rawptr,
    width: i32,
    height: i32,
    pitch: i32,
}