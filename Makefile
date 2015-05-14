all: build

build:
	mkdir -p bin
	g++ -o bin/hw3 UdpSocket.cpp udp.cpp Timer.cpp hw3.cpp
clean:

	rm -rf bin/
