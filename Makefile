all: objetivo1 objetivo2 objetivo3

objetivo1: servidor1 cliente1
objetivo2: servidor2 cliente1
objetivo3: servidor3 cliente3

servidor1: servidor1.c
	gcc servidor1.c -o servidor1
	
cliente1: cliente1.c
	gcc cliente1.c -o cliente1
	
servidor2: servidor2.c
	gcc servidor2.c -o servidor2
	
servidor3: servidor3.c
	gcc servidor3.c -o servidor3
	
cliente3: cliente3.c
	gcc cliente3.c -o cliente3

clean:
	 
	rm -rf *.o servidor1 cliente1 servidor2 servidor3 cliente3
