CXX=g++
CXXFLAGS=-Wall -Wextra -g -O1 -std=c++17 -pthread

TARGETS=torero-serve
PC_SRC = torero-serve.cpp BoundedBuffer.cpp

all: $(TARGETS)

torero-serve: $(PC_SRC)
	$(CXX) $^ -o $@ $(CXXFLAGS)
clean:
	rm -f $(TARGETS)
