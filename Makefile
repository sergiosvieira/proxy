all:
	gcc proxy_server.c -o proxy_server

clean:
	rm -rf *.o proxy_server
