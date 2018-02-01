CC:=gcc
CFLAGS:=-std=c99 -g -o0 -I./

OBJS:=cuckoo.o hashutil.o filter_test.o

all: $(OBJS)

clean:
	rm *.o
	rm filter_test

test:
	$(CC) $(CFLAGS) -o filter_test $(OBJS)
	./filter_test

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@  $<

