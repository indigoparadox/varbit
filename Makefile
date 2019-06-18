
CFLAGS := -g -pg -Wall -Werror -DUSE_SQLITE
LIBS := $(shell pkg-config --libs sqlite3)
LDFLAGS := $(LIBS)

varbit: varbit.o storage.o bstrlib.o archive.o dbsqlite.o
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	rm varbit ; rm *.o

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

