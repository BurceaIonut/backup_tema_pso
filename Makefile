.PHONY: clean
build: so_stdio.o
	gcc -shared so_stdio.o -o libso_stdio.so
	gcc main.c -o main -lso_stdio -L .
so_stdio.o: so_stdio.c
	gcc -c so_stdio.c -o so_stdio.o
clean: rm -f *.o main libso_stdio.so 
	export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:.
