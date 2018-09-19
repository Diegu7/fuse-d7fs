
CFLAGS = -g -Wall -DFUSE_USE_VERSION=26 `pkg-config fuse --cflags`
LINKFLAGS = -Wall `pkg-config fuse --libs`

all: bin/d7fs

clean:
	rm -rf bin obj

bin: 
	mkdir -p bin

bin/d7fs: bin obj/d7fs.o obj/device.o obj/main.o
	g++ -g -o bin/d7fs obj/* $(LINKFLAGS)

obj:
	mkdir -p obj

obj/main.o: obj main.c d7fs.h
	gcc -g $(CFLAGS) -c main.c -o $@

obj/d7fs.o: obj d7fs.c d7fs.h 
	g++ -g $(CFLAGS) -c d7fs.c -o $@

obj/device.o: obj device.c device.h
	g++ -g $(CFLAGS) -c device.c -o $@