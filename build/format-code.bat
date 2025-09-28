@echo off
setlocal enabledelayedexpansion

:: =============================================================================
:: Script to format C/C++ and CMake files.
::
:: USAGE:
::   format.bat          - Formats all relevant files in the project.
::   format.bat /changed - Formats only files that are changed (modified or
::                         untracked) according to Git.
::
:: REQUIREMENTS:
::   - clang-format: For C/C++ files.
::   - gersemi: For CMake files.
::   - git: Required for the /changed option.
:: =============================================================================

set CODE_EXCLUDE_FILTER=_deps cmake-build managed miniz gtest CheckPowershell DotnetPlugins dotnet-plugin-api mongoose.c mongoose.h
set CMAKE_EXCLUDE_FILTER=gtest cmake-build miniz vagrant

:: Check if Git is installed and available in PATH
where git >nul 2>nul
if %errorlevel% neq 0 (
    echo Error: Git is not found.
    exit /b 1
)

rem Check for the /changed argument
if /I "%1" == "/changed" goto changed
goto all

:changed
echo Formatting changed files based on Git status...

:: Get a list of all modified and untracked files from Git
git ls-files -m -o --exclude-standard > changed-files.tmp

echo Looking for changed C/C++ and CMake files...
findstr /I "\.h \.hpp \.c \.cpp" changed-files.tmp | findstr /v /I "%CODE_EXCLUDE_FILTER%" > format-code.tmp

echo Looking for changed CMake files...
findstr /I "\.cmake CMakeLists.txt" changed-files.tmp | findstr /v /I "%CMAKE_EXCLUDE_FILTER%" > format-cmake.tmp

del changed-files.tmp
goto format

:all
echo Formatting all project files...

:: Original logic to find all C/C++ files
(
    dir /b/s *.h
    dir /b/s *.hpp
    dir /b/s *.c
    dir /b/s *.cpp
) | sort | findstr /v /I "%CODE_EXCLUDE_FILTER%" > format-code.tmp

:: Original logic to find all CMake files
(
    dir /b/s CMakeLists.txt
    dir /b/s *.cmake
) | sort | findstr /v /I "%CMAKE_EXCLUDE_FILTER%" > format-cmake.tmp

if %errorlevel% neq 0 (
    echo Error: Failed to create file lists.
    exit /b %errorlevel%
)

goto format

:format

if not exist format-code.tmp (
    echo No C/C++ files to format.
    goto format_cmake
    exit /b 0
)

:format_c
echo.
echo Formatting C/C++ code...
for /f "delims=" %%f in (format-code.tmp) do (
    echo %%f
    clang-format -i "%%f"
)
if %errorlevel% neq 0 (
    echo Error: clang-format failed.
    del *.tmp
    exit /b !errorlevel!
)

:format_cmake
if not exist format-cmake.tmp (
    echo No CMake files to format.
    goto cleanup
    exit /b 0
)
echo.
echo Formatting CMake files...
for /f "delims=" %%f in (format-cmake.tmp) do (
    echo %%f
    gersemi -i "%%f"
)
if %errorlevel% neq 0 (
    echo Error: gersemi failed.
    del *.tmp
    exit /b !errorlevel!
)

:cleanup
:: --- Cleanup ---
echo.
echo Formatting complete.
del format-code.tmp 2>nul
del format-cmake.tmp 2>nul
endlocal