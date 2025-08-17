@echo off
dir /b/s *.h *.hpp *.c *.cpp | sort > format-code-0.tmp
copy /y NUL format-code.tmp
for /f "delims=" %%f in ('findstr /v "_deps cmake-build managed miniz gtest CheckPowershell DotnetPlugins dotnet-plugin-api mongoose.c mongoose.h" format-code-0.tmp') do (
    echo %%f >> format-code.tmp
)
if %errorlevel% neq 0 (
    echo Error: Failed to create format-code.tmp.
    exit /b %errorlevel%
)
del format-code-0.tmp
echo Formatting code...

for /f "delims=" %%f in (format-code.tmp) do (
        echo %%f
        clang-format -i %%f
    )
if %errorlevel% neq 0 (
    echo Error: clang-format failed.
    exit /b %errorlevel%
)
del format-code.tmp

rem dir /b/s CMakeLists.txt | sort > format-cmake.tmp
rem dir /b/s *.cmake | sort >> format-cmake.tmp
rem echo Formatting CMake files...
rem for /f "delims=" %%f in (format-cmake.tmp) do (
rem         echo %%f
rem         gersemi -i %%f
rem     )
rem del format-cmake.tmp
