CFLAGS=-Wall -g -DNDEBUG

all:
	make csvr

csvr: libcsv/libcsv.o