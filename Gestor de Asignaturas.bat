@echo off
REM ============================================================
REM  Lanzador del Gestor de Asignaturas (version grafica).
REM  Doble clic: busca el .exe, lo compila si hace falta y lo abre.
REM  La ventana NO se cierra sola si algo va mal.
REM ============================================================

title Gestor de Asignaturas
cd /d "%~dp0"

if exist "gestor_gui.exe" goto ejecutar

echo.
echo  La version grafica no esta compilada todavia. Lo intento ahora...
echo.
call "%~dp0compilar.bat" auto

if exist "gestor_gui.exe" goto ejecutar

echo.
echo ============================================================
echo   No se ha podido compilar la version grafica.
echo   Lee los mensajes de arriba.
echo   Alternativas:
echo     - Abre GestorAsignaturas.cbp en Code::Blocks, elige el
echo       target "Interfaz grafica" y pulsa F9.
echo     - Ejecuta diagnostico.bat para ver que falta.
if exist "gestor_asignaturas.exe" (
    echo.
    echo   La version de consola SI esta disponible:
    echo     ejecuta "Gestor consola.bat"
)
echo ============================================================
echo.
pause
exit /b 1

:ejecutar
start "" "%~dp0gestor_gui.exe"
exit /b 0
