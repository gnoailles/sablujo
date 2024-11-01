@echo off

set OptimOrDebugFlags=-MTd -Od -fp:fast -Oi
set WarningsHandlingFlags=-WX -W4 -wd4201 -wd4100 -wd4189 -wd4505
set CommonCompilerFlags=-nologo -arch:AVX2 -EHa- -EHsc -FC -Gm- -GR- -Z7 %OptimOrDebugFlags% %WarningsHandlingFlags%
set CommonCompilerDefines=-DSABLUJO_INTERNAL -DSABLUJO_SLOW -DSABLUJO_WIN32
REM set CommonCompilerDefines=-DSABLUJO_WIN32
set CommonLinkerFlags=-incremental:no -opt:ref

set GameSourceFiles=..\source\sablujo.cpp ..\source\sablujo_maths.cpp ..\source\sablujo_geometry.cpp
set GameCompilerFlags=-LD -Fmsablujo.map %CommonCompilerFlags% %CommonCompilerDefines%
set GameLinkerFlags=-PDB:sablujo_%random%.pdb -EXPORT:GameUpdateAndRender %CommonLinkerFlags%

set VendorSourceFiles=..\vendor\imgui\imgui.cpp ..\vendor\imgui\imgui_demo.cpp ..\vendor\imgui\imgui_draw.cpp ..\vendor\imgui\imgui_tables.cpp ..\vendor\imgui\imgui_widgets.cpp ..\vendor\imgui\backends\imgui_impl_win32.cpp ..\vendor\imgui\backends\imgui_impl_dx12.cpp

set DXRHelperSourceFiles=..\source\nv_helpers_dx12\BottomLevelASGenerator.cpp ..\source\nv_helpers_dx12\RaytracingPipelineGenerator.cpp ..\source\nv_helpers_dx12\RootSignatureGenerator.cpp ..\source\nv_helpers_dx12\ShaderBindingTableGenerator.cpp ..\source\nv_helpers_dx12\TopLevelASGenerator.cpp -I..\vendor\imgui

set PlatformSourceFiles=..\source\win32_sablujo.cpp ..\source\dx12_renderer.cpp ..\source\dx12_raytracing.cpp %VendorSourceFiles% %DXRHelperSourceFiles%

set PlatformCompilerFlags=-Fmwin32_sablujo.map %CommonCompilerFlags% %CommonCompilerDefines%

set PlatformVendorLibraries=..\vendor\dxcompiler\lib\dxcompiler.lib ..\vendor\dxcompiler\lib\dxilconv.lib ..\vendor\dxcompiler\lib\DxbcConverter.lib ..\vendor\dxcompiler\lib\DxilConvPasses.lib ..\vendor\dxcompiler\lib\ShaderBinary.lib
set PlatformLinkerFlags=user32.lib gdi32.lib d3d12.lib dxgi.lib dxguid.lib D3DCompiler.lib %CommonLinkerFlags%


REM Setup cl environment
WHERE cl > nul 2> nul
IF %ERRORLEVEL% NEQ 0 call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd" -no_logo -arch=x64 -host_arch=x64

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

REM 64-bit build
del *.pdb > NUL 2> NUL
cl %GameCompilerFlags% %GameSourceFiles% /link %GameLinkerFlags% 
cl %PlatformCompilerFlags% %PlatformSourceFiles% /link %PlatformLinkerFlags%
popd