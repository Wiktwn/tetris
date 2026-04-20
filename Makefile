# Define compiler and flags
CC = gcc

# common flags
CFLAGS = -Wall -Wextra

# build specific flags
CFLAGS_DEV = -O0 -Wpointer-arith -Wshadow -Wfloat-equal -Werror
CFLAGS_REL = -O2

# Define the source files, object files, and executable name
SOURCES   = main.c term.c
OBJECTS   = $(SOURCES:.c=.o)
TARGET    = tetris
LIBRARIES =

.PHONY: all clean dev release
 
all: dev release

# debug rules
dev: CFLAGS += $(CFLAGS_DEV)
dev: $(TARGET)

# release rules
release: CFLAGS += $(CFLAGS_REL)
release: $(TARGET)
	

# Rule to link the executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBRARIES)
	
# Generic compilation rule
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

REMOVE_CMD = 

# Rule to clean up generated files
ifeq ($(OS),Windows_NT)
	REMOVE_CMD := del
else
	REMOVE_CMD := rm -f
endif

clean:
	$(REMOVE_CMD) $(OBJECTS) $(TARGET)
