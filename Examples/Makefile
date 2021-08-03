.PHONY = all clean

all: example_findloop example_dsf

example_findloop: example_findloop.o utils/malloc.o utils/findloop.o
	gcc example_findloop.o utils/malloc.o utils/findloop.o -o example_findloop

example_dsf: example_dsf.o utils/malloc.o utils/dsf.o
	gcc example_dsf.o utils/malloc.o utils/dsf.o -o example_dsf

example_%.o: example_%.c
	LC_ALL=C $(CC) -c $< -o $@ -I./include $(CFLAGS)

utils/%.o: utils/%.c
	LC_ALL=C $(CC) -c $< -o $@ -I./include $(CFLAGS)

clean:
	rm example_findloop example_dsf *.o utils/*.o

