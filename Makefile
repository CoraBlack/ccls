ccls: main.o
	mkdir -p bin
	gcc build/main.o -o bin/ccls

main.o: main.c
	mkdir -p build
	gcc -c main.c -o build/main.o

clean:
	rm -f ./build/*.o
