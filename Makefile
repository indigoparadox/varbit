
OBJECTS := varbit.o bstrlib.o archive.o dbsqlite.o hash.o

CFLAGS := -g -pg -Wall -Werror -DUSE_SQLITE $(shell pkg-config --cflags sqlite3)
LDFLAGS := $(shell pkg-config --libs sqlite3)

MD=mkdir -v -p

OBJDIR := obj

all: varbit varbit-threads

varbit: $(addprefix $(OBJDIR)/,$(OBJECTS))
	$(CC) -o $@ $^ $(LDFLAGS)

varbit-threads: $(addprefix $(OBJDIR)/threads/,$(OBJECTS) thpool.o)
	$(CC) -o $@ $^ $(LDFLAGS) -lpthread

.PHONY: clean

clean:
	rm -rf varbit varbit-threads obj

$(OBJDIR)/threads/%.o: src/%.c
	$(MD) $(OBJDIR)/threads
	$(CC) -DUSE_THREADPOOL -c -o $@ $< $(CFLAGS)

$(OBJDIR)/%.o: src/%.c
	$(MD) $(OBJDIR)
	$(CC) -c -o $@ $< $(CFLAGS)

