
CXX=clang++
CXXFLAGS = -O2 -std=c++14 -Wall -Wextra -Wno-unused-parameter
LINKER=-lurcu-memb -pthread

critical: critical.cpp
	$(CXX) $(CXXFLAGS) -o critical critical.cpp $(LINKER)