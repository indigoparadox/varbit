
OBJECTS := varbit.o bstrlib.o archive.o dbsqlite.o hash.o

CFLAGS := -g -pg -Wall -Werror -DUSE_SQLITE --std=gnu89
LDFLAGS := $(shell pkg-config --libs sqlite3)

MD=mkdir -v -p

varbit: OBJDIR := ../obj/linux

varbit-threads: CFLAGS += -DUSE_THREADPOOL
varbit-threads: LDFLAGS += -lpthread
varbit-threads: OBJDIR := ../obj/linux-threads

all: varbit varbit-threads

varbit: $(OBJECTS)
	$(CC) -o $@ $(addprefix $(OBJDIR)/,$^) $(LDFLAGS)

varbit-threads: $(OBJECTS) thpool.o
	$(CC) -o $@ $(addprefix $(OBJDIR)/,$^) $(LDFLAGS)

.PHONY: clean

clean:
	rm varbit ; rm varbit-threads ; rm -rf ../obj

%.o: %.c
	$(MD) $(OBJDIR)/$(dir $@)
	$(CC) -c -o $(OBJDIR)/$@ $< $(CFLAGS)

