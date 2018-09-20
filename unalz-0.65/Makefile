CPP = g++
CC  = gcc
OBJ = main.o UnAlz.o UnAlzUtils.o UnAlzBz2decompress.o UnAlzBzip2.o UnAlzbzlib.o 
BIN = unalz
LDFLAGS = 
CFLAGS = -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 


all: unalz
	$(CPP) -c UnAlz.cpp -c UnAlzUtils.cpp -c main.cpp -D_UNALZ_ICONV -D_UNALZ_UTF8 $(CFLAGS)
	$(CPP) $(OBJ) $(LDFLAGS) -liconv -lz -lbz2 -o $(BIN)

install:
	cp unalz /usr/local/bin/

clean:
	rm -f $(OBJ) $(BIN)

$(BIN): $(OBJ)


