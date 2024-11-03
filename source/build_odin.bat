@echo off

set GAME_RUNNING=false
set EXE=sablujo_platform.exe

:: Check if game is running
FOR /F %%x IN ('tasklist /NH /FI "IMAGENAME eq %EXE%"') DO IF %%x == %EXE% set GAME_RUNNING=true

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

:: If game isn't running then:
:: - delete all game_XXX.dll files
:: - delete all PDBs in pdbs subdir
:: - optionally create the pdbs subdir
:: - write 0 into pdbs\pdb_number so game.dll PDBs start counting from zero
::
:: This makes sure we start over "fresh" at PDB number 0 when starting up the
:: game and it also makes sure we don't have so many PDBs laying around.
if %GAME_RUNNING% == false (
	del /q sablujo*.dll 2> nul
	
	if exist "pdbs" (
		del /q pdbs\*.pdb
	) else (
		mkdir pdbs
	)

	echo 0 > pdbs\pdb_number
)

:: Load PDB number from file, increment and store back. For as long as the game
:: is running the pdb_number file won't be reset to 0, so we'll get a PDB of a
:: unique name on each hot reload.
set /p PDB_NUMBER=<pdbs\pdb_number
set /a PDB_NUMBER=%PDB_NUMBER%+1
echo %PDB_NUMBER% > pdbs\pdb_number

:: Build game dll, use pdbs\game_%PDB_NUMBER%.pdb as PDB name so each dll gets
:: its own PDB. This PDB stuff is done in order to make debugging work.
:: Debuggers tend to lock PDBs or just misbehave if you reuse the same PDB while
:: the debugger is attached. So each time we compile `game.dll` we give the
:: PDB a unique PDB.
:: 
:: Note that we could not just rename the PDB after creation; the DLL contains a
:: reference to where the PDB is.
::
:: Also note that we always write game.dll to the same file. game_hot_reload.exe
:: monitors this file and does the hot reload when it changes.

echo Building sablujo.dll
odin build ..\source\game\. -strict-style -vet -debug -define:INTERNAL=true -define:SLOW=true -build-mode:dll -out:sablujo.dll -pdb-name:pdbs\sablujo_%PDB_NUMBER%.pdb > nul
IF %ERRORLEVEL% NEQ 0 (
    echo Compilation failed
    popd
    exit /b 1
)

:: If game.exe already running: Then only compile game.dll and exit cleanly
if %GAME_RUNNING% == true (
	echo Game running, hot reloading... 
    popd
    exit /b 1
)

:: Build game.exe, which starts the program and loads game.dll och does the logic for hot reloading.
echo Building %EXE%
odin build ..\source\platform\. -strict-style -vet -debug -define:INTERNAL=true -define:SLOW=true -out:%EXE%
IF %ERRORLEVEL% NEQ 0 (
    echo Compilation failed
    popd
    exit /b 1
)

popd