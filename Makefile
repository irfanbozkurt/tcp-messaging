COMPILER = g++
STDLIB = -std=c++0x

all: client server message

.SILENT:
client: client.cpp
	rm -f client;
	g++ $(STDLIB) client.cpp -o client;

.SILENT:
server: server.cpp
	rm -f server;
	g++ $(STDLIB) -pthread server.cpp -o server;

message:
	@echo "\t@@@@@@@@@@@ @@@@@@@@@@@@ @@@@@@@@@@@"
	@echo "\t@@@@@@@@@@@ INSTRUCTIONS @@@@@@@@@@@"
	@echo "\t (1) ./server (optional -p 1212)"
	@echo "\t\t-> default port is 1234\n"
	@echo "\t (2) ./client (optional -p 1212) (optional -ip 127.0.0.5)"
	@echo "\t\t-> default port is 1234, default ip (to connect) is 127.0.0.1\n"
	@echo "\t (3) Run the server program first\n"
	@echo "\t (4) You may run multiple instances of the client program\n"
	@echo "\t (5) Type exit and hit enter to exit client program"
	@echo "\t     You'll have to kill the process to exit server program\n"
	