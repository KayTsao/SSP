CROSS = 
CC = $(CROSS)gcc
#CXX = $(CROSS)g++
DEBUG = -g -O2
CFLAGS = $(DEBUG) -Wall -c

SRCS = tuto1
OBJS = $(addsuffix .o, $(SRCS))

HEADER = $(wildcard include/*.h)

HEADER_PATH = ./include/
LIB_PATH = ./lib/
LIBS = -lavformat -lavdevice -lavcodec -lavutil -lswscale -lswresample  -lz -lm -lpthread#-lbz2 -llzma -ldl -lrt   
 
TARGET = test  #_$(VERSION)

all: $(OBJS) $(EXAMPLES)

$(OBJS):%.o : %.c
	$(CC) -o test $(CFLAGS) $<  -I $(HEADER_PATH) $(LIBS)

$(TARGET) : $(OBJS)
	$(CC) -o $@ $^ -L $(LIB_PATH) $(LIBS)

clean:
	rm -f *.o $(TARGET) 

#gcc -o test ./tuto1.c -lavformat -lavcodec -lavutil -lz -lm -lpthread
