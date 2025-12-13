CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -O2
TARGET = proto
SRCS = proto.cpp bcd.cpp
OBJS = $(SRCS:.cpp=.o)

.PHONY: all clean long-double

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp bcd.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Build with long double precision (~18-19 digits on Linux)
long-double: CXXFLAGS += -DUSE_LONG_DOUBLE
long-double: clean $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
