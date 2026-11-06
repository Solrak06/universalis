@echo off
REM Lanzador de la version de consola (menus de texto).
title Gestor de Asignaturas - consola
cd /d "%~dp0"

if exist "gestor_asignaturas.exe" goto ejecutar

echo.
echo  La version de consola no esta compilada. Lo intento ahora...
echo.
call "%~dp0compilar.bat" auto

if exist "gestor_asignaturas.exe" goto ejecutar

echo.
echo  No se ha podido compilar. Ejecuta diagnostico.bat y revisa el error.
echo.
pause
exit /b 1

:ejecutar
cls
gestor_asignaturas.exe
set "SALIDA=%ERRORLEVEL%"
if not "%SALIDA%"=="0" (
    echo.
    echo  El programa termino con el codigo de error %SALIDA%.
    pause
)
exit /b %SALIDA%
