all: server client

SOCKET_NAME ?= server.sock
SOCKET_PERM ?= 777
CFLAGS      += -O2 -DSOCKET_NAME=\"${SOCKET_NAME}\" -DSOCKET_PERM=0${SOCKET_PERM}

server.o: server.c shared.h
client.o: client.c shared.h

server: server.o
	$(CC) $^ -o $@

client: client.o
	$(CC) $^ -o $@

clean:
	rm -rf server client *.o server.sock
