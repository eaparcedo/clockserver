CC=g++
CFLAGS=-DUNIX -pthread -Wall -Wpedantic -O3 -std=c++11 -D NO_PRINT

INCLUDESTD=-I/usr/include
INCLUDESTL=-I/usr/include/c++/7

all:

	$(CC) $(INCLUDESTD) $(INCLUDESTL) $(CFLAGS) -v ./clock_client_glibc.cpp -o clock_client_glibc
	$(CC) $(INCLUDESTD) $(INCLUDESTL) $(CFLAGS) -v ./clock_server_glibc.cpp -o clock_server_glibc
