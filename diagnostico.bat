@echo off
REM ============================================================
REM  Diagnostico: muestra que hay instalado y que falta.
REM  Ejecutalo y pasame lo que salga si algo no funciona.
REM ============================================================

cd /d "%~dp0"

echo ============================================================
echo   DIAGNOSTICO - Gestor de Asignaturas
echo ============================================================
echo.
echo Carpeta del proyecto:
echo   %~dp0
echo.

echo --- Compiladores en el PATH ---
where g++ 2>nul || echo   g++    : NO encontrado en el PATH
where gcc 2>nul || echo   gcc    : NO encontrado en el PATH
where cmake 2>nul || echo   cmake  : NO encontrado en el PATH
where cl 2>nul || echo   cl.exe : NO encontrado en el PATH (Visual Studio)
echo.

echo --- Buscando MinGW en carpetas habituales ---
for %%D in (
    "C:\Program Files\CodeBlocks\MinGW\bin"
    "C:\Program Files (x86)\CodeBlocks\MinGW\bin"
    "C:\CodeBlocks\MinGW\bin"
    "C:\Program Files\CodeBlocks\MinGW64\bin"
    "C:\msys64\ucrt64\bin"
    "C:\msys64\mingw64\bin"
    "C:\mingw64\bin"
    "C:\MinGW\bin"
    "C:\TDM-GCC-64\bin"
    "C:\Strawberry\c\bin"
) do (
    if exist "%%~D\g++.exe" (echo   [SI] %%~D) else (echo   [no] %%~D)
)
echo.

echo --- Instalaciones de Code::Blocks ---
if exist "C:\Program Files\CodeBlocks\codeblocks.exe" (echo   [SI] C:\Program Files\CodeBlocks) else (echo   [no] C:\Program Files\CodeBlocks)
if exist "C:\Program Files (x86)\CodeBlocks\codeblocks.exe" (echo   [SI] C:\Program Files ^(x86^)\CodeBlocks) else (echo   [no] C:\Program Files ^(x86^)\CodeBlocks)
echo.

echo --- Ficheros del proyecto ---
if exist "src\main.cpp" (echo   [SI] src\main.cpp) else (echo   [NO] src\main.cpp)
if exist "include\Modelo.hpp" (echo   [SI] include\Modelo.hpp) else (echo   [NO] include\Modelo.hpp)
if exist "GestorAsignaturas.cbp" (echo   [SI] GestorAsignaturas.cbp) else (echo   [NO] GestorAsignaturas.cbp)
if exist "gestor_asignaturas.exe" (echo   [SI] gestor_asignaturas.exe COMPILADO) else (echo   [no] gestor_asignaturas.exe todavia no compilado)
if exist "bin\Release\gestor_asignaturas.exe" echo   [SI] bin\Release\gestor_asignaturas.exe
if exist "bin\Debug\gestor_asignaturas.exe" echo   [SI] bin\Debug\gestor_asignaturas.exe
if exist "compilacion.log" (
    echo.
    echo --- Ultimo error de compilacion ---
    type compilacion.log
)
echo.

echo ============================================================
echo   Fin del diagnostico
echo ============================================================
echo.
pause
