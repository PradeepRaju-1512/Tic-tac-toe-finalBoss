#!/bin/bash
# ============================================================================
# Course: CS204 - Object-Oriented Systems & Network Programming (B.Tech CSE)
# One-Step Linux Build & Execution Script
# ============================================================================

set -e

echo "=== [Step 1/3] Checking Linux Toolchain ==="
if ! command -v g++ &> /dev/null; then
    echo "[ERROR] g++ compiler not found. Installing build-essential..."
    sudo apt-get update && sudo apt-get install -y build-essential libasio-dev
fi

echo "=== [Step 2/3] Compiling C++ Translation Units (C++17) ==="
g++ -std=c++17 -Wall -Wextra -O2 -pthread \
    -Iinclude \
    src/AuthManager.cpp \
    src/Board.cpp \
    src/Player.cpp \
    src/AIPlayer.cpp \
    src/WebSocketPlayer.cpp \
    src/LocalPlayer.cpp \
    src/GameSession.cpp \
    src/ServerNetworkManager.cpp \
    src/main.cpp \
    -o tictactoe_server

echo "=== [Step 3/3] Build Complete! ==="
echo "Executable created: ./tictactoe_server"
echo "To launch: ./tictactoe_server 8080"
