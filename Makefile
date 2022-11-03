
CXX=clang++
CC=clang
CXXFLAGS = -O -std=c++14 -Wall -Wextra -Wno-unused-parameter
LINKER=-lurcu-memb -pthread

default: all

all: benchmark

benchmark: benchmark.cpp
	$(CXX) $(CXXFLAGS) -o benchmark.out benchmark.cpp $(LINKER)