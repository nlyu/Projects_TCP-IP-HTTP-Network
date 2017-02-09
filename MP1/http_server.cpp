/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#include <sys/sendfile.h>

#define BACKLOG 10	 // how many pending connections queue will hold

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{

	if (argc != 2) {
	    fprintf(stderr,"usage: server hostname\n");
	    exit(1);
	}

	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections on port %s...\n", argv[1]);

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener
			//sparce the file
			int ret, end;
			char * recv_buf = (char *)calloc(4096, sizeof(char));
			char * document = (char *)calloc(4096, sizeof(char));
			if (recv(new_fd, recv_buf, 4096, 0) == -1){
				perror("recv");
				if (send(new_fd, "HTTP/1.0 400 Bad Request\r\n\r\n", 28, 0) == -1){
					perror("send");
					free(recv_buf);
					free(document);
					close(new_fd);
					exit(0);
				}
			}

			ret = (int)(strstr(recv_buf, "/") - recv_buf + 1);
			end = (int)(strstr(recv_buf, " HTTP") - recv_buf);
			strncpy(document, recv_buf + ret, end - ret);
			printf("the document is: %s\n", document);

			FILE *fd = fopen(document, "r");
			if(fd == NULL){
				//if nor found 404 not found
				printf("The file is not found\n");
				if (send(new_fd, "HTTP/1.0 404 Not Found\r\n\r\n", 26, 0) == -1){
					perror("send");
				}
			}
			else{
				//if found send file
				printf("begin sending file...\n");
				fseek(fd, 0L, SEEK_END);
				long file_size = ftell(fd);
				rewind(fd);
				printf("The size of file is: %ld\n", file_size);
				recv_buf = (char *)realloc(recv_buf, file_size * sizeof(char));
				memset(recv_buf, 0, file_size * sizeof(char));
				fread(recv_buf, 1, file_size * sizeof(char), fd);
				printf("successfully create buffer\n");
				if (send(new_fd, "HTTP/1.0 200 OK\r\n\r\n", 19, 0) == -1){
					perror("send");
				}
				if (send(new_fd, recv_buf, file_size * sizeof(char), 0) == -1){
					perror("send");
				}
				fclose(fd);
			}
			printf("end of listening\n");
			close(new_fd);
			free(document);
			free(recv_buf);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}
