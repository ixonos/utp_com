CC ?= gcc
CFLAGS ?= -I. -Wall -O2
LDFLAGS ?= 
DEPS ?=

OBJ=utp_com.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

utp_com: $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)
