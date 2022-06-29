all: server subscriber

server:
	g++ server.cpp -Wall -g -std=c++11 -o server

subscriber:
	g++ subscriber.cpp -Wall -g -std=c++11 -o subscriber

clean:
	rm -rf server subscriber