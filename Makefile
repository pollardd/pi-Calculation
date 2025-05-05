# Compiler and flags
CC = g++
CFLAGS = -std=c++17 -Wall
DEBUG_FLAGS = -g           # Enable debug symbols
OPT_FLAGS = -O3            # Optimization level
LDFLAGS = -lgmp -lmpfr -lm -pthread -lstdc++fs

# Add chudnovsky.cpp here
SOURCES = calculate_pi.cpp chudnovsky.cpp
OBJECTS = $(SOURCES:.cpp=.o)

# Default target is optimized build
all: calculate_pi

# Optimized build
calculate_pi: CFLAGS += $(OPT_FLAGS)
calculate_pi: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Debug build
debug: CFLAGS += $(DEBUG_FLAGS)
debug: $(OBJECTS)
	$(CC) $(CFLAGS) -o calculate_pi_debug $^ $(LDFLAGS)

# Object file rule
%.o: %.cpp
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o calculate_pi calculate_pi_debug

install:
	cp calculate_pi /usr/local/bin/
