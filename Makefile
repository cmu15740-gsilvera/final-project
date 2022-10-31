
CXX=clang++
CXXFLAGS = -O2 -std=c++14 -Wall -Wextra -Wno-unused-parameter
LINKER=-lurcu-memb -pthread

counter: counter_race.cpp
	$(CXX) $(CXXFLAGS) -o counter_race.out counter_race.cpp $(LINKER)