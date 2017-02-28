#makefile for hw5
homework5: hw5.c
	gcc -o hw5 hw5.c

tar:
	tar -cf hw4.tar hw5.c makefile