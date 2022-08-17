build: libso_stdio.so

libso_stdio.so: so_stdio.c
	gcc -Wall -fPIC -c so_stdio.c -o so_stdio.o
	gcc -Wall -shared so_stdio.o -o libso_stdio.so

clean:
	rm *.o *.so