all: client server demo

client: client.o input_parsing.o
	gcc -o client client.o input_parsing.o

server: main.o input_parsing.o exec.o 
	gcc -o server main.o input_parsing.o exec.o -lpthread -lrt

main.o: main.c
	gcc -c main.c -lpthread -lrt

input_parsing: input_parsing.c input_parsing.h
	gcc -c input_parsing.c

exec: exec.c exec.h
	gcc -c exec.c

demo: demo.o
	gcc -o demo demo.o -lpthread -lrt

demo.o: demo.c
	gcc -c demo.c -lrt -lpthread

clean:
	rm *.o client server demo 
