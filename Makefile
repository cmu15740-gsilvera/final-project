
CXX=clang++
CXXFLAGS = -O2 -std=c++14 -Wall -Wextra -Wno-unused-parameter
LINKER=-lurcu-memb -pthread

default: all

all: counter_race counter_lock

counter_race: counter_race.cpp
	$(CXX) $(CXXFLAGS) -o counter_race.out counter_race.cpp $(LINKER)

counter_lock: counter_lock.cpp
	$(CXX) $(CXXFLAGS) -o counter_lock.out counter_lock.cpp $(LINKER)