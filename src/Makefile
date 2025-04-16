# Makefile for building the smartodo application

# Compiler and flags
CC      = cc
CFLAGS  = -std=c17 -Wall -Wextra -pedantic -I/opt/homebrew/include
LDFLAGS = -L/opt/homebrew/lib -lcjson -lncurses -lcurl

# Sources and objects
SRCS   = main.c ui.c storage.c task.c ai_assist.c
OBJS   = $(SRCS:.c=.o)
TARGET = smartodo

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Compile .c to .o, update dependencies as needed
%.o: %.c ui.h storage.h task.h ai_assist.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -d $(DESTDIR)/usr/local/bin
	install $(TARGET) $(DESTDIR)/usr/local/bin/$(TARGET)
