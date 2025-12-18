CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -O2 -DUSE_LONG_DOUBLE
TARGET = proto
SRCS = proto.cpp bcd.cpp addsub.cpp testbench.cpp
OBJS = $(SRCS:.cpp=.o)

.PHONY: all clean double

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp bcd.h testbench.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Build with double precision (for Windows compatibility testing)
double: CXXFLAGS = -Wall -Wextra -std=c++17 -O2
double: clean $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
