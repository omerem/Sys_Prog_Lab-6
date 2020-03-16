all: shell
shell: LineParser.o main.o
	gcc -g -Wall -m32 -o shell LineParser.o main.o
LineParser.o: LineParser.c LineParser.h
	gcc -g -Wall -m32 -c LineParser.c -o LineParser.o
main.o: main.c 
	gcc -g -Wall -m32 -c main.c -o main.o
.PHONY: clean
clean: 
	rm -f *.o shell
