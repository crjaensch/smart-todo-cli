# Makefile for building the smartodo application

# Compiler and flags
CC      = cc
CFLAGS  = -std=c17 -Wall -Wextra -pedantic -I/opt/homebrew/include
LDFLAGS = -L/opt/homebrew/lib -lcjson -lncurses -lcurl

# Sources and objects
SRCS   = main.c ui.c storage.c task.c ai_assist.c ai_chat.c llm_api.c utils.c
OBJS   = main.o task.o storage.o ai_assist.o ai_chat.o llm_api.o ui.o utils.o
TARGET = smartodo

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Compile .c to .o, update dependencies as needed
%.o: %.c ui.h storage.h task.h ai_assist.h utils.h
	$(CC) $(CFLAGS) -c $< -o $@

ai_chat.o: ai_chat.c ai_chat.h
	$(CC) $(CFLAGS) -c $< -o $@

llm_api.o: llm_api.c llm_api.h
	$(CC) $(CFLAGS) -c $< -o $@

utils.o: utils.c utils.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -d $(DESTDIR)/usr/local/bin
	install $(TARGET) $(DESTDIR)/usr/local/bin/$(TARGET)
