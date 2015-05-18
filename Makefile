all: build

build:
	mkdir -p bin
	g++ -o bin/hw3 UdpSocket.cpp udp.cpp Timer.cpp hw3.cpp
	g++ -o bin/hw3a UdpSocket.cpp udpa.cpp Timer.cpp hw3a.cpp
clean:

	rm -rf bin/
