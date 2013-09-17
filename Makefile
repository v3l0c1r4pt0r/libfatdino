GCC = gcc
GPP = g++ -std=c++0x
all: build build/demo
debug: GCC += -g
debug:
	@echo "Debug mode enabled"
lib: build/libfatdino.o
libfatdino.o: build/libfatdino.o
libfatdino.so: build/libfatdino.so
build/libfatdino.so: build/libfatdino.o
build/libfatdino.so:
	gcc -shared -o build/libfatdino.so build/libfatdino.o -lc
build:
	mkdir build
build/libfatdino.o:build src/fat32.c src/fat32.h
	$(GCC) -o build/libfatdino.o -c -fPIC src/fat32.c
build/demo: build demo/fat32_test.cpp build/libfatdino.o
	$(GPP) -o build/demo build/libfatdino.o demo/fat32_test.cpp
build/demo-shared: build demo/fat32_test.cpp build/libfatdino.so
	$(GPP) -L `pwd`/build/ -o build/demo-shared demo/fat32_test.cpp -lfatdino
clean:
	rm -r build