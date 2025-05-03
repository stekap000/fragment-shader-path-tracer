@echo off

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64

set DEFINES=/DATTACH_CONSOLE /DDEBUG_MODE /D_HAS_EXCEPTIONS=0 /D_CRT_SECURE_NO_WARNINGS
set DISABLE_SOME_WARNINGS=-wd4505 -wd4702 -wd4100

set COMPILER_OPTIONS=%DEFINES%               ^
                     -nologo                 ^
                     /GR-                    ^
					 -EHa-                   ^
					 -Oi                     ^
					 -W4                     ^
					 /std:c++20              ^
					 %DISABLE_SOME_WARNINGS%
					 
set INCLUDE_DIRS=include

rem audioses.lib
rem mmdevapi.lib
set LIBRARIES=gdi32.lib user32.lib opengl32.lib

rem model.cpp ^
set C_AND_CPP_FILES=main.cpp glad.c

cl %COMPILER_OPTIONS% %C_AND_CPP_FILES% /I %INCLUDE_DIRS% /link %LIBRARIES%

