@echo off
chcp 65001 >nul
echo ========================================
echo  37AC 控制器 - 构建脚本
echo ========================================
echo.

set BUILD_DIR=build

if "%1"=="clean" (
    echo [清理] 删除构建目录...
    if exist %BUILD_DIR% rmdir /s /q %BUILD_DIR%
    echo [清理] 完成
    exit /b 0
)

if not exist %BUILD_DIR% mkdir %BUILD_DIR%

set CMAKE_PATH="E:\apps\VS2022\app\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

echo [配置] 使用 CMake 配置项目...
cd %BUILD_DIR%
%CMAKE_PATH% .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE="E:/apps/vcpkg/scripts/buildsystems/vcpkg.cmake"
if %ERRORLEVEL% neq 0 (
    echo [错误] CMake 配置失败
    pause
    exit /b 1
)

echo [构建] 编译项目...
%CMAKE_PATH% --build . --config Release
if %ERRORLEVEL% neq 0 (
    echo [错误] 编译失败
    pause
    exit /b 1
)

echo.
echo [成功] 编译完成！
echo [输出] %BUILD_DIR%\Release\ACUI.exe
echo.
pause