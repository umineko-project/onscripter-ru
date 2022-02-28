@echo off&setlocal

rem
rem idebuild.cmd
rem ONScripter-RU
rem
rem A hack script to allow building from an external IDE.
rem
rem Consult LICENSE file for licensing terms and copyright holders.
rem

rem Usage:
rem idebuild.cmd action build_dir bin_dir
rem where actions include build, clean, debug

if not "%4"=="" del %4\ONScripter.exe

set B_DEBUG=
set B_ACTION=build

if "%1"=="debug" set B_DEBUG=DEBUG=1
if "%1"=="clean" set B_ACTION=clean

echo.%4 | findstr /C:"Debug">nul && set B_DEBUG=DEBUG=1
echo.%4 | findstr /C:"debug">nul && set B_DEBUG=DEBUG=1

if "%2"=="" (
	set B_PROJECT_PATH=%~dp0\..\
) else (
	set B_PROJECT_PATH=%2\
)

if "%3"=="" (
	set B_MSYS_PATH=C:\msys64\
) else (
	set B_MSYS_PATH=%3\
)

set PATH=%PATH%;%B_MSYS_PATH%usr\bin;%B_MSYS_PATH%mingw32\bin

rem Fixing CLion "handy" -j 4 and other presents
set MAKEFLAGS=""

if "%B_ACTION%"=="clean" (
	make -C %B_PROJECT_PATH% clean
) else (
	make -C %B_PROJECT_PATH% %B_DEBUG% -j 4
)

if not "%4"=="" cp %B_PROJECT_PATH%onscripter-ru.exe %4\ONScripter.exe


