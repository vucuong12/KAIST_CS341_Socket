all: client server server_fork server_select

client: client.c
	gcc -o client client.c

server: server.c
	gcc -o server server.c

server_fork: server_fork.c
	gcc -o server_fork server_fork.c

server_select: server_select.c
	gcc -o server_select server_select.c