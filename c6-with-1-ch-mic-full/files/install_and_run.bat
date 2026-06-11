@echo off
title 1CH Mic Tester - Setup and Launch
color 0A

echo.
echo  =============================================
echo   1CH Mic Tester  --  Glyph C6 / ICS43434
echo  =============================================
echo.

REM Check if Python is installed
python --version >nul 2>&1
if errorlevel 1 (
    echo  [ERROR] Python is not installed or not in PATH.
    echo.
    echo  Please install Python 3.8 or newer from:
    echo  https://www.python.org/downloads/
    echo.
    echo  IMPORTANT: During install, check the box:
    echo  "Add Python to PATH"
    echo.
    pause
    start https://www.python.org/downloads/
    exit /b 1
)

echo  [OK] Python found:
python --version
echo.

echo  [1/6] Updating pip...
python -m pip install --upgrade pip --quiet

echo  [2/6] Installing pyserial...
python -m pip install pyserial --quiet
echo  [3/6] Installing sounddevice...
python -m pip install sounddevice --quiet
echo  [4/6] Installing numpy...
python -m pip install numpy --quiet
echo  [5/6] Installing matplotlib...
python -m pip install matplotlib --quiet
echo  [6/6] Installing mysql-connector-python...
python -m pip install mysql-connector-python --quiet

echo.
echo  [OK] All dependencies installed.
echo.
echo  =============================================
echo   Launching Mic Tester App...
echo  =============================================
echo.

cd /d "%~dp0"
python mic_tester_app.py

if errorlevel 1 (
    echo.
    echo  [ERROR] App exited with an error. See message above.
    pause
)
