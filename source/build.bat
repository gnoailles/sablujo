@echo off

set CommonCompilerFlags=-MT -nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -FC -Z7 -arch:AVX2 -EHsc
set CommonLinkerFlags=-incremental:no -opt:ref user32.lib gdi32.lib

WHERE cl > nul 2> nul
IF %ERRORLEVEL% NEQ 0 call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

@REM cl %CommonCompilerFlags% ..\source\win32_handmade.cpp ..\source\handmade.cpp /link %CommonLinkerFlags%

REM 32-bit build
REM cl %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp /link -subsystem:windows,5.1 %CommonLinkerFlags%

REM 64-bit build
del *.pdb > NUL 2> NUL
cl %CommonCompilerFlags% ..\source\handmade.cpp ..\source\handmade_maths.cpp -Fmhandmade.map -LD /link -incremental:no -opt:ref -PDB:handmade_%random%.pdb -EXPORT:GameUpdateAndRender
cl %CommonCompilerFlags% ..\source\win32_handmade.cpp -Fmwin32_handmade.map /link %CommonLinkerFlags%
popd