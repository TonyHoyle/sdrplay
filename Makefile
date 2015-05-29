CFLAGS=-Wall
LDFLAGS=-lmir_sdr -lstdc++

test: test.c sdrplay.cpp

clean:
	rm test
