TARGET ?= custom_cpc_host
CC = gcc
LD = ld
C_SRC = custom_cpc_host.c
CFLAGS=-g -Wall -Wextra -lcpc -lpthread
EXEDIR = exe

$(EXEDIR)/$(TARGET): $(C_SRC)
	mkdir -p $(EXEDIR)
	$(CC) $(DEBUG) -o $@ $^ $(CFLAGS)

debug: DEBUG = -DDEBUG

debug: $(EXEDIR)/$(TARGET)

clean:
	rm -f $(EXEDIR)/$(TARGET)