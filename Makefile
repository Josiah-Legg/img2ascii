SRC = src/main.c
BIN = img2ascii
CC = gcc
CFLAGS ?= -Wall -Wextra -std=c11
LIBS = -lm

ifeq ($(OS),Windows_NT)
EXEEXT = .exe
LDFLAGS = -static
RMCMD = del
else
EXEEXT =
LDFLAGS =
RMCMD = rm -f
endif

all:
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(BIN)$(EXEEXT) $(SRC) $(LIBS)

clean:
	-$(RMCMD) $(BIN) $(BIN).exe