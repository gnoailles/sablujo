@echo off

set OptimOrDebugFlags=-MT -O2 -fp:fast -Oi
set WarningsHandlingFlags=-WX -W4 -wd4201 -wd4100 -wd4189 -wd4505
set CommonCompilerFlags=-nologo -arch:AVX2 -EHa- -EHsc -FC -Gm- -GR- -Z7 %OptimOrDebugFlags% %WarningsHandlingFlags%
set CommonCompilerDefines=-DSABLUJO_INTERNAL -DSABLUJO_SLOW -DSABLUJO_WIN32
REM set CommonCompilerDefines=-DSABLUJO_WIN32
set CommonLinkerFlags=-incremental:no -opt:ref

set GameSourceFiles=..\source\sablujo.cpp ..\source\sablujo_maths.cpp ..\source\sablujo_geometry.cpp
set GameCompilerFlags=-LD -Fmsablujo.map %CommonCompilerFlags% %CommonCompilerDefines%
set GameLinkerFlags=-PDB:sablujo_%random%.pdb -EXPORT:GameUpdateAndRender %CommonLinkerFlags%

set PlatformSourceFiles=..\source\win32_sablujo.cpp ..\source\dx12_renderer.cpp
set PlatformCompilerFlags=-Fmwin32_sablujo.map %CommonCompilerFlags% %CommonCompilerDefines%
set PlatformLinkerFlags=user32.lib gdi32.lib d3d12.lib dxgi.lib D3DCompiler.lib %CommonLinkerFlags%


REM Setup cl environment
WHERE cl > nul 2> nul
IF %ERRORLEVEL% NEQ 0 call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build



REM 32-bit build
REM cl %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp /link -subsystem:windows,5.1 %CommonLinkerFlags%


REM 64-bit build
del *.pdb > NUL 2> NUL
cl %GameCompilerFlags% %GameSourceFiles% /link %GameLinkerFlags% 
cl %PlatformCompilerFlags% %PlatformSourceFiles% /link %PlatformLinkerFlags%
popd