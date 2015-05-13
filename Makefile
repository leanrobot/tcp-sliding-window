all: build

build:
	mkdir -p bin
	g++ -pthread -o bin/hw3 hw3.cpp
clean:

	rm -rf bin/
