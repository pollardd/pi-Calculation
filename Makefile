CC = g++
CFLAGS = -std=c++17 -Wall -O3
LDFLAGS = -lgmp -lmpfr -lm -pthread -lstdc++fs

# Add chudnovsky.cpp here
SOURCES = calculate_pi.cpp chudnovsky.cpp
OBJECTS = $(SOURCES:.cpp=.o)

all: clean calculate_pi

calculate_pi: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o calculate_pi

install:
	cp calculate_pi /usr/local/bin/
