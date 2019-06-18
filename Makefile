
OBJECTS := varbit.o bstrlib.o archive.o dbsqlite.o

CFLAGS := -g -pg -Wall -Werror -DUSE_SQLITE
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

clean:
	rm varbit ; rm *.o

%.o: %.c
	$(MD) $(OBJDIR)/$(dir $@)
	$(CC) -c -o $(OBJDIR)/$@ $< $(CFLAGS)

