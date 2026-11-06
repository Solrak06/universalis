@echo off
REM ============================================================
REM  Ejecuta las pruebas automaticas del profesor y de sus herramientas de estudio (no necesitan
REM  Ollama: la IA se simula). Uso: doble clic o "probar.bat".
REM ============================================================
cd /d "%~dp0"
set "PY=%UNIVERSALIS_PYTHON%"
if not defined PY if exist "%USERPROFILE%\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe" set "PY=%USERPROFILE%\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
if not defined PY set "PY=python"
set PYTHONIOENCODING=utf-8
set PYTHONDONTWRITEBYTECODE=1
"%PY%" profesor\pruebas\test_profesor.py
if errorlevel 1 (echo. & echo  Hay pruebas que fallan. & pause & exit /b 1)
"%PY%" profesor\pruebas\test_estudio.py
if errorlevel 1 (echo. & echo  Hay pruebas que fallan. & pause & exit /b 1)
"%PY%" profesor\pruebas\test_herramientas.py
if errorlevel 1 (echo. & echo  Hay pruebas que fallan. & pause & exit /b 1)
"%PY%" profesor\pruebas\test_temas.py
if errorlevel 1 (echo. & echo  Hay pruebas que fallan. & pause & exit /b 1)
"%PY%" profesor\pruebas\test_conexion_ia.py
if errorlevel 1 (echo. & echo  Hay pruebas que fallan. & pause & exit /b 1)
REM Pruebas de la musica (C++): las compila compilar.bat
if exist "pruebas\test_musica.exe" (
    "pruebas\test_musica.exe"
    if errorlevel 1 (echo. & echo  Hay pruebas de la musica que fallan. & pause & exit /b 1)
)
echo. & echo  Todas las pruebas pasan.
if /i not "%~1"=="auto" pause
