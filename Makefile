# Define compiler and flags
CC = gcc

# common flags
CFLAGS = -Wall -Wextra

# build specific flags
CFLAGS_DEV = -O0 -Wpointer-arith -Wshadow -Wfloat-equal -Werror
CFLAGS_REL = -O2

# Define the source files, object files, and executable name
SOURCES   = main.c
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


# Rule to clean up generated files
clean:
	rm -f $(OBJECTS) $(TARGET)
