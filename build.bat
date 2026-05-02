@echo off

mkdir build 2>nul

set SOURCES=^
  "..\src\doins.c" ^
  "..\src\intelhex.c" ^
  "..\src\io.c" ^
  "..\src\kc85rom.c" ^
  "..\src\m100rom.c" ^
  "..\src\m10rom.c" ^
  "..\src\m200rom.c" ^
  "..\src\memory.c" ^
  "..\src\n8201rom.c" ^
  "..\src\n8300rom.c" ^
  "..\src\serial.c" ^
  "..\src\sound.c" ^
  "..\src\main_sdl.c"

set INCLUDE_PATHS=^
	/I"..\src" ^
	/I"..\external\SDL3-3.4.4\include"

set CFLAGS=^
	/D_WIN32 ^
	/DWIN32 ^
	/D_CRT_SECURE_NO_WARNINGS ^
	/nologo ^
	/W3 ^
	/Zi ^
	/Od ^
	/MDd ^
	/Fe:"enu.exe"

set LDFLAGS=^
	/LIBPATH:"..\external\SDL3-3.4.4\lib\x64" ^
	wsock32.lib ^
	comctl32.lib ^
	winmm.lib ^
	sdl3.lib ^
	/SUBSYSTEM:CONSOLE ^
	/MACHINE:X64

pushd build

cl %CFLAGS% %INCLUDE_PATHS% %SOURCES% /link %LDFLAGS%
if errorlevel 1 (
echo Build failed.
  popd
  exit /b 1
)

cp ..\external\SDL3-3.4.4\lib\x64\SDL3.dll .
echo Build succeeded: %cd%\enu.exe
popd
exit /b 0
