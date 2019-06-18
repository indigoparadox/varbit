
CFLAGS := -g -pg -Wall -Werror
LIBS := $(shell pkg-config --libs sqlite3)
LDFLAGS := $(LIBS)

varbit: varbit.o storage.o bstrlib.o
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	rm varbit ; rm *.o

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

