CC=gcc
CFLAGS=-I. -Wall -O2
DEPS=
OBJ=utp_com.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

utp_com: $(OBJ)
	gcc -o $@ $^ $(CFLAGS)
