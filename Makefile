# ============================================================================
# Course: CS204 - Object-Oriented Systems & Network Programming (B.Tech CSE)
# Makefile for Linux C++ Online Multiplayer Tic-Tac-Toe
# ============================================================================

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread -Iinclude
TARGET = tictactoe_server

SRCS = src/AuthManager.cpp \
       src/Board.cpp \
       src/Player.cpp \
       src/AIPlayer.cpp \
       src/WebSocketPlayer.cpp \
       src/LocalPlayer.cpp \
       src/GameSession.cpp \
       src/ServerNetworkManager.cpp \
       src/main.cpp

OBJS = $(SRCS:.cpp=.o)

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)
	@echo "Build successful: ./$(TARGET) is ready."

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) 8080

clean:
	rm -f $(OBJS) $(TARGET)
	@echo "Cleaned build artifacts."
