@echo off
REM ============================================================
REM  Compila el Gestor de Asignaturas: version grafica y consola.
REM  Busca g++ en el PATH y, si no, en las carpetas habituales
REM  (incluido el MinGW que trae Code::Blocks).
REM  Uso:  compilar.bat        -> se para al final para que leas
REM        compilar.bat auto   -> sin pausa (lo usa el lanzador)
REM ============================================================

cd /d "%~dp0"
set "AUTO="
if /i "%~1"=="auto" set "AUTO=1"

echo ============================================
echo   Compilando el Gestor de Asignaturas
echo ============================================
echo.

set "COMUNES=src\Json.cpp src\Fecha.cpp src\Modelo.cpp src\Rutas.cpp src\Almacen.cpp src\Utilidades.cpp src\Calendario.cpp src\ExportadorHtml.cpp"
REM -static enlaza TODO (libstdc++, libgcc y libwinpthread) dentro del .exe:
REM sin esto Windows pide libwinpthread-1.dll al abrirlo fuera del IDE.
set "ESTATICO=-static -static-libgcc -static-libstdc++"

REM ---------- 1. buscar g++ en el PATH ----------
set "GPP="
set "GPPDIR="
set "FALLO=0"
where g++ >nul 2>nul && set "GPP=g++"

REM ---------- 2. buscar en las rutas habituales ----------
if not defined GPP (
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
        "%USERPROFILE%\Downloads\CodeBlocks\MinGW\bin"
    ) do (
        if not defined GPP if exist "%%~D\g++.exe" (
            set "GPP=%%~D\g++.exe"
            set "GPPDIR=%%~D"
        )
    )
)

if not defined GPP goto sin_gpp
if defined GPPDIR set "PATH=%GPPDIR%;%PATH%"

echo Compilador: %GPP%
echo.

REM ---------- 3. recursos (manifiesto de estilos visuales) ----------
set "RECURSO="
set "WINDRES="
where windres >nul 2>nul && set "WINDRES=windres"
if not defined WINDRES if defined GPPDIR if exist "%GPPDIR%\windres.exe" set "WINDRES=%GPPDIR%\windres.exe"
if defined WINDRES (
    "%WINDRES%" gestor.rc -O coff -o gestor_res.o >nul 2>&1
    if exist "gestor_res.o" set "RECURSO=gestor_res.o"
)

REM ---------- 4. version grafica ----------
echo [1/2] Compilando la version grafica (gestor_gui.exe)...
"%GPP%" -std=c++17 -O2 -Wall -Iinclude %COMUNES% src\GuiMain.cpp src\GuiProfesor.cpp src\GuiProfesorEstudio.cpp src\GuiProfesorHerramientas.cpp src\GuiAjustes.cpp src\GuiAgenda.cpp src\GuiProfesorEnfoque.cpp src\GuiProfesorComandos.cpp src\GuiProfesorTemario.cpp src\GuiProfesorAmbiente.cpp src\Musica.cpp src\ProfesorMotor.cpp src\GuiUtil.cpp src\GuiCalendario.cpp %RECURSO% -o gestor_gui.exe -mwindows %ESTATICO% -lgdiplus -lcomctl32 -lcomdlg32 -lshell32 -lole32 -lgdi32 -luser32 -lmsimg32 -ladvapi32 -lwinmm > compilacion_gui.log 2>&1

if not errorlevel 1 (
    echo       [OK] gestor_gui.exe
    del compilacion_gui.log >nul 2>nul
) else (
    set "FALLO=1"
    echo       [ERROR] no se pudo compilar la version grafica:
    echo    --------------------------------------------
    type compilacion_gui.log
    echo    --------------------------------------------
)

REM ---------- 5. version de consola ----------
echo [2/2] Compilando la version de consola (gestor_asignaturas.exe)...
"%GPP%" -std=c++17 -O2 -Wall -Wextra -Iinclude %COMUNES% src\main.cpp -o gestor_asignaturas.exe %ESTATICO% -lshell32 > compilacion.log 2>&1

if not errorlevel 1 (
    echo       [OK] gestor_asignaturas.exe
    del compilacion.log >nul 2>nul
) else (
    set "FALLO=1"
    echo       [ERROR] no se pudo compilar la version de consola:
    echo    --------------------------------------------
    type compilacion.log
    echo    --------------------------------------------
)

REM ---------- 6. pruebas de la musica (las ejecuta probar.bat) ----------
"%GPP%" -std=c++17 -O2 -Iinclude pruebas\test_musica.cpp src\Musica.cpp src\Rutas.cpp src\GuiUtil.cpp src\Modelo.cpp src\Fecha.cpp src\Json.cpp src\Utilidades.cpp -o pruebas\test_musica.exe %ESTATICO% -lwinmm -lgdi32 -luser32 -lcomctl32 -lshell32 -lcomdlg32 -ladvapi32 -lmsimg32 -lgdiplus >nul 2>&1

del gestor_res.o >nul 2>nul

echo.
if "%FALLO%"=="1" goto fallo_total
echo Compilacion terminada.
if not defined AUTO pause
exit /b 0

:fallo_total
echo La compilacion ha fallado. Un ejecutable anterior no confirma una compilacion nueva.
if not defined AUTO pause
exit /b 1

:sin_gpp
echo  [ERROR] No se ha encontrado ningun compilador de C++ (g++).
echo.
echo  Soluciones, de mas facil a mas laboriosa:
echo.
echo   1) Abre GestorAsignaturas.cbp en Code::Blocks y pulsa F9.
echo   2) Si Code::Blocks esta instalado en otra carpeta, anade su carpeta
echo      MinGW\bin al PATH (o a la lista de este script).
echo   3) Instala MSYS2 (https://www.msys2.org) y ejecuta:
echo         pacman -S mingw-w64-ucrt-x86_64-gcc
echo.
echo  Ejecuta diagnostico.bat para ver que hay instalado en tu equipo.
echo.
if not defined AUTO pause
exit /b 1
