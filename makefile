# Roll no: MT25087
# Makefile

CC=gcc
CFLAGS=-O0 -g -pthread

all: a1 a2 a3

a1:
	$(CC) $(CFLAGS) MT25087_Part_A_A1_server.c -o a1_server
	$(CC) $(CFLAGS) MT25087_Part_A_A1_client.c -o a1_client
a2:
	$(CC) $(CFLAGS) MT25087_Part_A_A2_server.c -o a2_server
	$(CC) $(CFLAGS) MT25087_Part_A_A2_client.c -o a2_client

a3:
	$(CC) $(CFLAGS) MT25087_Part_A_A3_server.c -o a3_server
	$(CC) $(CFLAGS) MT25087_Part_A_A3_client.c -o a3_client

run-a1-server:
	./a1_server 64 32

run-a1-client:
	./a1_client 64 8 10

run-a2-server:
	./a2_server 64 32

run-a2-client:
	./a2_client 64 8 10

run-a3-server:
	./a3_server 64 32

run-a3-client:
	./a3_client 64 8 10

partC:
	chmod +x MT25087_Part_C_main.sh
	./MT25087_Part_C_main.sh

clean:
	rm -f a1_server a1_client a2_server a2_client a3_server a3_client
	rm -f *.png *.csv