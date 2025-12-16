@echo off
REM GLSL to SPIRV Shader Compilation Script
REM Requires Vulkan SDK with glslangValidator in PATH

setlocal

REM Output directory
set OUTDIR=..\spirv
if not exist %OUTDIR% mkdir %OUTDIR%

echo Compiling GLSL shaders to SPIRV...

REM Use explicit Vulkan 1.0 target environment
set VKFLAGS=-V --target-env vulkan1.0

REM Single-texture shaders
echo Compiling genericst...
glslangValidator %VKFLAGS% genericst.vert -o %OUTDIR%\genericst_vs.spirv
if errorlevel 1 goto error
glslangValidator %VKFLAGS% genericst.frag -o %OUTDIR%\genericst_ps.spirv
if errorlevel 1 goto error

REM Multi-texture shaders
echo Compiling genericmt...
glslangValidator %VKFLAGS% genericmt.vert -o %OUTDIR%\genericmt_vs.spirv
if errorlevel 1 goto error
glslangValidator %VKFLAGS% genericmt.frag -o %OUTDIR%\genericmt_ps.spirv
if errorlevel 1 goto error

REM Skybox shaders
echo Compiling skybox...
glslangValidator %VKFLAGS% skybox.vert -o %OUTDIR%\skybox_vs.spirv
if errorlevel 1 goto error
glslangValidator %VKFLAGS% skybox.frag -o %OUTDIR%\skybox_ps.spirv
if errorlevel 1 goto error

REM 2D image shaders
echo Compiling image2d...
glslangValidator %VKFLAGS% image2d.vert -o %OUTDIR%\image2d_vs.spirv
if errorlevel 1 goto error
glslangValidator %VKFLAGS% image2d.frag -o %OUTDIR%\image2d_ps.spirv
if errorlevel 1 goto error

echo.
echo All shaders compiled successfully!
echo Output directory: %OUTDIR%

REM Copy to baseq3 for runtime
echo.
echo Copying to baseq3\spirv...
if not exist ..\..\baseq3\spirv mkdir ..\..\baseq3\spirv
copy /Y %OUTDIR%\*.spirv ..\..\baseq3\spirv\
echo Done!

goto end

:error
echo.
echo ERROR: Shader compilation failed!
exit /b 1

:end
endlocal
