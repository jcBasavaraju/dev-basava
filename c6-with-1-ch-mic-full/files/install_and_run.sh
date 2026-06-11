#!/bin/bash
echo ""
echo " ============================================="
echo "  1CH Mic Tester  --  Glyph C6 / ICS43434"
echo " ============================================="
echo ""

if ! command -v python3 &>/dev/null; then
    echo " [ERROR] python3 not found. Install it with:"
    echo "   sudo apt install python3 python3-pip   (Ubuntu/Debian)"
    echo "   brew install python3                   (macOS)"
    exit 1
fi

echo " [OK] $(python3 --version)"
echo ""

echo " [1/6] Updating pip..."
python3 -m pip install --upgrade pip --quiet

echo " [2/6] Installing pyserial..."
python3 -m pip install pyserial --quiet
echo " [3/6] Installing sounddevice..."
python3 -m pip install sounddevice --quiet
echo " [4/6] Installing numpy..."
python3 -m pip install numpy --quiet
echo " [5/6] Installing matplotlib..."
python3 -m pip install matplotlib --quiet
echo " [6/6] Installing mysql-connector-python..."
python3 -m pip install mysql-connector-python --quiet

echo ""
echo " [OK] All dependencies installed."
echo ""
echo " ============================================="
echo "  Launching Mic Tester App..."
echo " ============================================="
echo ""

cd "$(dirname "$0")"
python3 mic_tester_app.py
