PROGRAMS = Init B D I O T W Master
CC = gcc
CFLAGS = -Wall
all:
	$(CC) $(CFLAGS) -o bin/Master/Master src/Master/Master.c

	$(CC) $(CFLAGS) -o bin/Solo/Init src/Solo/Init.c
	$(CC) $(CFLAGS) -o bin/Solo/B src/Solo/B.c -lncurses -lm
	$(CC) $(CFLAGS) -o bin/Solo/D src/Solo/D.c -lm
	$(CC) $(CFLAGS) -o bin/Solo/I src/Solo/I.c -lncurses
	$(CC) $(CFLAGS) -o bin/Solo/O src/Solo/O.c
	$(CC) $(CFLAGS) -o bin/Solo/T src/Solo/T.c
	$(CC) $(CFLAGS) -o bin/Solo/W src/Solo/W.c -lncurses

	$(CC) $(CFLAGS) -o bin/MultiDrone/Init src/MultiDrone/Init.c
	$(CC) $(CFLAGS) -o bin/MultiDrone/B src/MultiDrone/B.c -lncurses -lm
	$(CC) $(CFLAGS) -o bin/MultiDrone/D src/MultiDrone/D.c -lm
	$(CC) $(CFLAGS) -o bin/MultiDrone/I src/MultiDrone/I.c -lncurses
	$(CC) $(CFLAGS) -o bin/MultiDrone/S src/MultiDrone/S.c

	$(CC) $(CFLAGS) -o bin/MultiObst/Init src/MultiObst/Init.c
	$(CC) $(CFLAGS) -o bin/MultiObst/B src/MultiObst/B.c -lncurses -lm
	$(CC) $(CFLAGS) -o bin/MultiObst/D src/MultiObst/D.c -lm
	$(CC) $(CFLAGS) -o bin/MultiObst/I src/MultiObst/I.c -lncurses
	$(CC) $(CFLAGS) -o bin/MultiObst/C src/MultiObst/C.c
clean:
	rm -f bin/Master/*
	rm -f bin/Solo/*
	rm -f bin/MultiDrone/*
	rm -f bin/MultiObst/*