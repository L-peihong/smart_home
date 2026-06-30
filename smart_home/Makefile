CC = g++
CFLAGS = -Wall -O2 -g -I./inc
LDFLAGS = -lpthread

SRCS = src/i2c.cpp src/i2c_probe.cpp src/s1.cpp src/s2.cpp src/s7.cpp src/s8.cpp src/e1.cpp src/e2.cpp src/e3.cpp src/main.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = smart_hub

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean