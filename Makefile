# Define compiler and flags
CC = gcc

# common flags
CFLAGS = -Wall -Wextra

# build specific flags
CFLAGS_DEV   = -O0 -Wpointer-arith -Wshadow -Wfloat-equal -Werror
CFLAGS_REL   = -O2
CFLAGS_DEBUG = -g 

# Define the source files, object files, and executable name
SOURCES   = main.c term.c
OBJECTS   = $(SOURCES:.c=.o)
TARGET    = tetris
LIBRARIES = -lm

# HOME_PATH =
REMOVE_CMD = 
ifeq ($(OS),Windows_NT)
	REMOVE_CMD := del
	# HOME_PATH  := -Force $(USERPROFILE)/Desktop
else
	REMOVE_CMD := rm -f
	# HOME_PATH  := -p ~
endif

# HOME_PATH_NORM = $(subst \,/,$(HOME_PATH))

.PHONY: all clean dev release install install-dep
 
all: dev release debug

# debug rules
dev: CFLAGS += $(CFLAGS_DEV)
dev: $(TARGET)

# release rules
release: CFLAGS += $(CFLAGS_REL)
release: $(TARGET)

debug: CFLAGS += $(CFLAGS_DEBUG)
debug: CFLAGS += $(CFLAGS_DEV)
debug: $(TARGET)

install:
	cp -f tetris /usr/bin/

# Rule to link the executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBRARIES)
	
# Generic compilation rule
# mkdir $(HOME_PATH_NORM)/.tetris/
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(REMOVE_CMD) $(OBJECTS) $(TARGET)
