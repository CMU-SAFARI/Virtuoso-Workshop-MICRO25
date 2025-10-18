
CC= g++
CFLAGS= -Wall -Wextra -O3 -g
INCLUDE=-I../include
SRCS = microbench.cpp

OBJS = $(SRCS:.cpp=.o)

TARGET = microbench

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDE) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CC) $(CFLAGS) $(INCLUDE) -c $<

clean: 
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean