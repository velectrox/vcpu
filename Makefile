CC=armv6k-linux-gnueabihf-gcc
CFLAGS=-O2 -static

SRCS = vcpu.c svc.c
OBJS = $(SRCS:.c=.o)
TARGET = vcpu

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
