
CXX=clang++
CC=clang
CXXFLAGS = -O -std=c++17 -Wall -Wextra -Wno-unused-parameter
LINKER=-lurcu-qsbr -pthread

default: all

all: benchmark

benchmark: benchmark.cpp
	$(CXX) $(CXXFLAGS) -o benchmark.out benchmark.cpp $(LINKER)

clean:
	rm -rf *.out
	rm -rf results/