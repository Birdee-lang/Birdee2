test2:test2.o birdeelib
	gcc -o ../bin/test2 test2.o ../bin/blib/birdee.o ../../bin/libBirdeeRuntime.a
test2.o:test2.txt
	../../bin/birdeec -i test2.txt -o test2.o -e

birdeelib:
	../../bin/birdeec --corelib -i birdee.txt -o ../bin/blib/birdee.o
