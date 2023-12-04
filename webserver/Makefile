# Daniel Mendez
# ECEN 5713
# Final Project

CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Wextra -g
INCLUDES ?= -I./include
LDFLAGS ?= -pthread -lrt

OUTPUT = webserver

SOURCES = webserver.c

OBJS = $(SOURCES:.c=.o)

all: $(OUTPUT)

$(OUTPUT): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(OUTPUT) $(OBJS) $(LDFLAGS)

clean:
	rm -f $(OUTPUT) $(OBJS)