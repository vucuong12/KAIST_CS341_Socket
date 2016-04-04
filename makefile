all: client server server_select clientLoc

client: client.c
	gcc -o client client.c

server: server.c
	gcc -o server server.c

server_select: server_select.c
	gcc -o server_select server_select.c

clientLoc: clientLoc.c
	gcc -o clientLoc clientLoc.c