# Use all available cores for parallel compilation
MAKEFLAGS += -j$(shell nproc)

CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -O2 -DUSE_LONG_DOUBLE
TARGET = proto
SRCS = proto.cpp bcd.cpp addsub.cpp testbench.cpp exponent.cpp mantissa.cpp register.cpp mult.cpp div.cpp log.cpp tan.cpp tan10.cpp sqrt.cpp sin.cpp cos.cpp asin.cpp acos.cpp
OBJS = $(SRCS:.cpp=.o)

.PHONY: all clean double

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp bcd.h proto.h testbench.h testbench.inl exponent.h mantissa.h register.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Build with double precision (for Windows compatibility testing)
double: CXXFLAGS = -Wall -Wextra -std=c++17 -O2
double: clean $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
