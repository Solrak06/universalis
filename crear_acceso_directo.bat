@echo off
REM ============================================================
REM  Crea el icono "Gestor de Asignaturas" en el Escritorio.
REM  Ejecutalo una sola vez.
REM ============================================================

cd /d "%~dp0"

echo ============================================
echo   Creando el acceso directo en el Escritorio
echo ============================================

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0crear_acceso_directo.ps1" "%~dp0."

if errorlevel 1 (
    echo.
    echo  No se ha podido crear automaticamente.
    echo  Hazlo a mano, es igual de valido:
    echo    1. Clic derecho sobre "Gestor de Asignaturas.bat"
    echo    2. Mostrar mas opciones ^> Enviar a ^> Escritorio ^(crear acceso directo^)
)

echo.
pause
