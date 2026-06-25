@echo off

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
set LIBRARY_DIRS=lib

set LIBRARIES=gdi32.lib user32.lib opengl32.lib shell32.lib glfw3_mt_windows.lib

set C_AND_CPP_FILES=main.cpp glad.c

cl %COMPILER_OPTIONS% %C_AND_CPP_FILES% /I %INCLUDE_DIRS% /link /LIBPATH:%LIBRARY_DIRS% %LIBRARIES%
