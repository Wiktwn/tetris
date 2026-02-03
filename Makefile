# Define compiler and flags
CC = gcc

# common flags
CFLAGS = -Wall

# build specific flags
CFLAGS_DEV = -Wextra
CFLAGS_REL = -O3

# Define the source files, object files, and executable name
SOURCES = main.c
OBJECTS = $(SOURCES:.c=.o)
TARGET  = tetris
LIBRARIES    = ncurses

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
	$(CC) $(OBJECTS) -o $(TARGET) -l $(LIBRARIES)
	
# Generic compilation rule
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


# Rule to clean up generated files
clean:
	rm -f $(OBJECTS) $(TARGET)
