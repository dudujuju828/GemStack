@echo off
setlocal enabledelayedexpansion

echo ========================================
echo GemStack Build Script
echo ========================================
echo.

:: Check for required tools
where git >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] Git is not installed or not in PATH
    exit /b 1
)

where node >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] Node.js is not installed or not in PATH
    exit /b 1
)

where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] CMake is not installed or not in PATH
    exit /b 1
)

:: Step 1: Initialize/update submodules
echo [1/4] Initializing submodules...
git submodule update --init --recursive
if %errorlevel% neq 0 (
    echo [ERROR] Failed to initialize submodules
    exit /b 1
)
echo [OK] Submodules initialized
echo.

:: Step 2: Install Gemini CLI dependencies
echo [2/4] Installing Gemini CLI dependencies...
cd gemini-cli
call npm install
if %errorlevel% neq 0 (
    echo [ERROR] Failed to install Gemini CLI dependencies
    cd ..
    exit /b 1
)
echo [OK] Dependencies installed
echo.

:: Step 3: Build Gemini CLI
echo [3/4] Building Gemini CLI...
call npm run build
if %errorlevel% neq 0 (
    echo [ERROR] Failed to build Gemini CLI
    cd ..
    exit /b 1
)
cd ..
echo [OK] Gemini CLI built
echo.

:: Step 4: Build GemStack
echo [4/4] Building GemStack...
cmake -B build -S .
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed
    exit /b 1
)

cmake --build build
if %errorlevel% neq 0 (
    echo [ERROR] GemStack build failed
    exit /b 1
)
echo [OK] GemStack built
echo.

echo ========================================
echo Build completed successfully!
echo.
echo Run GemStack with:
echo   .\build\Debug\GemStack.exe
echo ========================================
