
all: ext2_ls ext2_cp

ext2_ls : helper.o ext2_ls.o
	gcc -Wall -g -o ext2_ls $^

ext2_cp : helper.o ext2_cp.o
	gcc -Wall -g -o ext2_cp $^

ext2_ls.o : ext2_ls.c ext2.h
	gcc -Wall -g -c $<

ext2_cp.o : ext2_cp.c ext2.h
	gcc -Wall -g -c $<

clean : 
	rm -f *.o
