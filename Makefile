
CXX=clang++
CXXFLAGS = -O2 -std=c++14 -Wall -Wextra -Wno-unused-parameter
LINKER=-lurcu-memb -pthread

default: all

all: counter_race counter_lock counter_atomic

counter_race: counter_race.cpp
	$(CXX) $(CXXFLAGS) -o counter_race.out counter_race.cpp $(LINKER)

counter_lock: counter_lock.cpp
	$(CXX) $(CXXFLAGS) -o counter_lock.out counter_lock.cpp $(LINKER)

counter_atomic: counter_atomic.cpp
	$(CXX) $(CXXFLAGS) -o counter_atomic.out counter_atomic.cpp $(LINKER)