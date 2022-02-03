SOURCES=$(wildcard *.c)
HEADERS=$(SOURCES:.c=.h)
FLAGS=-DDEBUG -g

all: main

main: $(SOURCES) $(HEADERS)
	mpicc $(SOURCES) $(FLAGS) -o main

clear: clean

clean:
	rm main

run: main
	mpirun -np 4 ./main 
	#mpirun -default-hostfile none --oversubscribe -np 4 ./main
#aby dzialało na debianie to podmienić, nie mam pojecia co to robi
