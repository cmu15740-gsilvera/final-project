
CXX=clang++
CXXFLAGS = -O2 -std=c++14 -Wall -Wextra -Wno-unused-parameter
RCU=-lurcu-memb

critical:
	$(CXX) $(CXXFLAGS) -o critical critical.cpp $(RCU)