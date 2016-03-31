default: ftserver 

ftserver.o: ftserver.c
	gcc -Wall -std=c99 -c ftserver.c

ftserver: ftserver.o
	gcc -Wall -std=c99 -o ftserver ftserver.o
	
clean:
	rm ftserver *.o
