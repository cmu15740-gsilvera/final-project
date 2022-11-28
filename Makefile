
CXX=clang++
CC=clang
CXXFLAGS = -O -std=c++17 -Wall -Wextra -Wno-unused-parameter
LINKER=-lurcu-qsbr -pthread

default: all

all: bump-counter atomic-str struct-abc

bump-counter: benchmark.cpp bump_counter.h
	$(CXX) $(CXXFLAGS) -o bump-counter.out benchmark.cpp -DOP_BUMP_COUNTER $(LINKER)

atomic-str: benchmark.cpp atomic_string.h
	$(CXX) $(CXXFLAGS) -o bump-counter.out benchmark.cpp -DOP_ATOMIC_STR $(LINKER)

struct-abc: benchmark.cpp struct_abc.h
	$(CXX) $(CXXFLAGS) -o bump-counter.out benchmark.cpp -DOP_STRUCT_ABC $(LINKER)

clean:
	rm -rf *.out
	rm -rf results/