all: pes test_objects test_tree

pes: object.o index.o tree.o commit.o pes.o
	gcc -o pes object.o index.o tree.o commit.o pes.o -lcrypto

test_objects: object.o test_objects.c
	gcc -Wall -O2 -o test_objects test_objects.c object.o -lcrypto

test_tree: object.o index.o tree.o test_tree.c
	gcc -Wall -O2 -o test_tree test_tree.c object.o index.o tree.o -lcrypto

object.o: object.c
	gcc -Wall -O2 -c object.c

index.o: index.c
	gcc -Wall -O2 -c index.c

tree.o: tree.c
	gcc -Wall -O2 -c tree.c

commit.o: commit.c
	gcc -Wall -O2 -c commit.c

pes.o: pes.c
	gcc -Wall -O2 -c pes.c

test-integration: pes
	bash test_sequence.sh

clean:
	rm -f *.o pes test_objects test_tree
