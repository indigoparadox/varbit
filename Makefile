

varbit: varbit.o storage.o
	gcc -l sqlite3 -l bstrlib -o $@ $^

clean:
	rm varbit ; rm *.o

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

