
CXX=clang++
CC=clang
CXXFLAGS = -O -std=c++17 -Wall -Wextra -Wno-unused-parameter
LINKER=-lurcu-qsbr -pthread

OUT=out

default: all

all: bump-counter atomic-str struct-abc

make-dir:
	mkdir -p ${OUT}

bump-counter: benchmark.cpp bump_counter.h make-dir
	$(CXX) $(CXXFLAGS) -o ${OUT}/bump-counter.out benchmark.cpp -DOP_BUMP_COUNTER $(LINKER)

atomic-str: benchmark.cpp atomic_string.h make-dir
	$(CXX) $(CXXFLAGS) -o ${OUT}/atomic-str.out benchmark.cpp -DOP_ATOMIC_STR $(LINKER)

struct-abc: benchmark.cpp struct_abc.h make-dir
	$(CXX) $(CXXFLAGS) -o ${OUT}/struct-abc.out benchmark.cpp -DOP_STRUCT_ABC $(LINKER)

clean:
	rm -rf ${OUT}
	rm -rf results/