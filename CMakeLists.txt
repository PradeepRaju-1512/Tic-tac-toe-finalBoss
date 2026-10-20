# ============================================================================
# Course: CS204 - Object-Oriented Systems & Network Programming (B.Tech CSE)
# CMakeLists.txt for Linux C++ Online Multiplayer Tic-Tac-Toe
# ============================================================================

cmake_minimum_required(VERSION 3.10)
project(TicTacToeServer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for academic rigor and warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pthread -O2")

# Include directories
include_directories(include)

# Source files
set(SOURCES
    src/AuthManager.cpp
    src/Board.cpp
    src/Player.cpp
    src/AIPlayer.cpp
    src/WebSocketPlayer.cpp
    src/LocalPlayer.cpp
    src/GameSession.cpp
    src/ServerNetworkManager.cpp
    src/main.cpp
)

add_executable(tictactoe_server ${SOURCES})

# Threads requirement for POSIX pthreads / Asio
find_package(Threads REQUIRED)
target_link_libraries(tictactoe_server PRIVATE Threads::Threads)

# Output status
message(STATUS "Configured TicTacToeServer with C++17 and pthread support.")
