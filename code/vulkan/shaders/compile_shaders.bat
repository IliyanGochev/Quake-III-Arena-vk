@echo off
REM ----------------------------------------------------------------------------
REM Compile GLSL shaders to SPIR-V using glslangValidator
REM Requires Vulkan SDK to be installed with glslangValidator in PATH
REM ----------------------------------------------------------------------------

echo Compiling Quake III Arena Vulkan shaders...
echo.

REM Create output directory if it doesn't exist
if not exist "compiled" mkdir compiled

REM Check if glslangValidator is available
where glslangValidator >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: glslangValidator not found in PATH
    echo Please install the Vulkan SDK or add glslangValidator to your PATH
    pause
    exit /b 1
)

REM Compile single-texture shaders
echo [1/8] Compiling genericst.vert...
glslangValidator -V genericst.vert -o compiled/genericst_vs.spv
if %ERRORLEVEL% NEQ 0 goto :error

echo [2/8] Compiling genericst.frag...
glslangValidator -V genericst.frag -o compiled/genericst_ps.spv
if %ERRORLEVEL% NEQ 0 goto :error

REM Compile multi-texture shaders
echo [3/8] Compiling genericmt.vert...
glslangValidator -V genericmt.vert -o compiled/genericmt_vs.spv
if %ERRORLEVEL% NEQ 0 goto :error

echo [4/8] Compiling genericmt.frag...
glslangValidator -V genericmt.frag -o compiled/genericmt_ps.spv
if %ERRORLEVEL% NEQ 0 goto :error

REM Compile skybox shaders
echo [5/8] Compiling skybox.vert...
glslangValidator -V skybox.vert -o compiled/skybox_vs.spv
if %ERRORLEVEL% NEQ 0 goto :error

echo [6/8] Compiling skybox.frag...
glslangValidator -V skybox.frag -o compiled/skybox_ps.spv
if %ERRORLEVEL% NEQ 0 goto :error

REM Compile fullscreen quad / 2D shaders
echo [7/8] Compiling fsq.vert...
glslangValidator -V fsq.vert -o compiled/fsq_vs.spv
if %ERRORLEVEL% NEQ 0 goto :error

echo [8/8] Compiling fsq.frag...
glslangValidator -V fsq.frag -o compiled/fsq_ps.spv
if %ERRORLEVEL% NEQ 0 goto :error

echo.
echo ========================================
echo All shaders compiled successfully!
echo ========================================
echo.
echo Output directory: compiled\
dir /B compiled\*.spv
echo.

exit /b 0

:error
echo.
echo ========================================
echo ERROR: Shader compilation failed!
echo ========================================
pause
exit /b 1
