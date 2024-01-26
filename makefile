# makefile
all: server client

server: server.c
	gcc -o server server.c -pthread

client: client.c
	gcc -o client client.c