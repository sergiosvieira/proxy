/*
 * proxy_server.c
 *
 *  Created on: 03/05/2010
 *      Author: Sérgio Vieira
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <ctype.h>

#define PORT 1978
#define HTTPPORT 80

void error(char *msg) {
	perror(msg);
	exit(1);
}

int split(char *string, char *delimiter, char result[][256]) {
	char *ret = NULL;
	int i = 0;
	ret = strtok(string, delimiter);
	while (ret != NULL) {
		strcpy(result[i++], ret);
		ret = strtok(NULL, delimiter);
	}

	return i;
}

char* find(char* string, const char *substring) {
	char *result;
	char *found = strstr(string, substring);
	char *aux = found;
	int size;
	while (*aux != '\n' && *aux != '\0' && *aux != '\r') {
		aux++;
	}
	size = aux - found + 1;
	result = (char*) malloc(sizeof(char) * size);
	bzero(result, size);
	memcpy(result, found, size);
	result[size - 1] = '\0';
	return result;
}

struct sockaddr_in get_sockaddr_in(const char* hostname, int port) {
	struct hostent *hp;
	struct sockaddr_in pin;
	if ((hp = gethostbyname(hostname)) == NULL) {
		error("Error: unknown host\n");
	}
	memset(&pin, 0, sizeof(pin));
	pin.sin_family = AF_INET;
	pin.sin_addr.s_addr = ((struct in_addr *) (hp->h_addr))->s_addr;
	pin.sin_port = htons(port);
	return pin;
}

void proxy(const char* message, int fd) {
	const int just_say_no = 1;
	char buffer[8192];
	struct sockaddr_in pin;
	int sd, i, pid;
	char *text = find(message, "Host:");

	printf("text [%s]\n", text);

	char splited[3][256];
	if (text == NULL) {
		error("Error: NULL message.");
	}
	int n = split(text, " ", splited);
	free(text);
	if (splited[1] == NULL) {
		error("Error: invalid URL.");
	}
	for (i = 0; i < n; i++) {
		printf("%s\n", splited[i]);
	}
	pin = get_sockaddr_in(splited[1], HTTPPORT);

	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		error("Error on socket");
	}

	/* connect to PORT on HOST */
	if (connect(sd, (struct sockaddr *) &pin, sizeof(pin)) == -1) {
		error("Error on connect");
	}

	setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, (char *) &just_say_no,
			sizeof(just_say_no));

	/* send a message to the server PORT on machine HOST */
	if (send(sd, message, strlen(message), 0) == -1) {
		error("Error on send");
	}
	i = 0;
	ssize_t rc;
	ssize_t amt;
	ssize_t offset;

	while ((rc = recv(sd, buffer, sizeof(buffer), 0)) > 0) {
		if (rc <= 0) {
			if (rc == 0)
				break;
			if (errno == EINTR || errno == EAGAIN)
				continue;
			error("read error");
		}
		offset = 0;
		amt = rc;
		pid = fork();

		if (pid < 0) {
			error("Error on fork");
		}

		if (pid == 0) {
			while (amt) {
				rc = send(fd, buffer + offset, amt, 0);
				if (rc < 0) {
					if (errno == EINTR || errno == EAGAIN)
						continue;
					error("write error");
				}
				offset += rc;
				amt -= rc;
			}
		} else {
			close(sd);
		}

	}
}

char* host2ip(const char* hostname) {
	struct addrinfo hints, *res, *p;
	char *ipstr = (char*) malloc(sizeof(char) * INET6_ADDRSTRLEN);

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(hostname, NULL, &hints, &res) != 0) {
		error("Error on getaddrinfo");
	}

	p = res;
	while (p != NULL) {
		void *addr;
		struct sockaddr_in *ipv4 = (struct soackaddr_in*) p->ai_addr;
		addr = &(ipv4->sin_addr);
		inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
	}

	return ipstr;
}

int main() {
	char buffer[4096];
	int sd, sd_current, cc, fromlen, tolen;
	int addrlen;
	struct sockaddr_in sin;
	struct sockaddr_in pin;
	int pid;
	bzero(buffer, sizeof(buffer));
	/* get an internet domain socket */
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		error("Error on socket.");
	}

	/* complete the socket structure */
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(PORT);

	/* bind the socket to the port number */
	if (bind(sd, (struct sockaddr *) &sin, sizeof(sin)) == -1) {
		error("Error on bind");
	}

	/* show that we are willing to listen */
	if (listen(sd, 5) == -1) {
		error("Error on listen");
	} else {
		printf("Listening on port %d ...\n", PORT);
	}
	/* wait for a client to talk to us */
	addrlen = sizeof(pin);
	while (1) {

		if ((sd_current = accept(sd, (struct sockaddr *) &pin, &addrlen)) == -1) {
			error("Error on accept");
		}

		pid = fork();
		if (pid < 0) {
			error("Error on fork.");
		}

		printf("Client connection on %s:%d\n", inet_ntoa(pin.sin_addr), ntohs(
				pin.sin_port));

		if (pid == 0) {
			close(sd);
			/* get a message from the client */
			while (recv(sd_current, buffer, sizeof(buffer), 0) > 0) {
				proxy(buffer, sd_current);
			}
		} else {
			close(sd_current);
		}
	}

	/* give client a chance to properly shutdown */
	sleep(1);

	return 0;
}
