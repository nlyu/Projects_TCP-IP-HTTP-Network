//nlyu2 Nuochen Lyu ECE438 MP1
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// struct storing the http datagram
typedef struct info_http{
	char * port;
	char * host;
	char * msg;
}info_http;


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


//sparce the url, fill info into struct myhttp
void sparceurl(info_http& myhttp, char * argv){
	int ret;
	int temp_len;
	char * temp = (char*)calloc(strlen(argv), sizeof(char));

	//get msg
	strcpy(temp, argv + 7);
	temp_len = strlen(temp);
	if(strchr(temp, '/') == NULL){//if there is no document request, set '/'
		myhttp.msg = (char*)calloc(2, sizeof(char));
		strcpy(myhttp.msg, "/");
		printf("input url no msg\n");

		myhttp.host = (char*)calloc(temp_len + 3, sizeof(char));
		strcpy(myhttp.host, temp);
		printf("input url host is %s\n", myhttp.host);
	}
	else{//if there is a request, extract the document name
		ret = (int)(strchr(temp, '/')-temp); //get url
		myhttp.msg = (char*)calloc(temp_len - ret + 3, sizeof(char));
		memcpy(myhttp.msg, temp+ret, temp_len - ret);
		printf("input url msg is %s\n", myhttp.msg);

		myhttp.host = (char*)calloc(ret + 3, sizeof(char));
		memcpy(myhttp.host, temp, ret);
		printf("input url host is %s\n", myhttp.host);
	}

	//get port and host info
	if(strchr(myhttp.host, ':') == NULL){//if no port specified, set 80 as default
		myhttp.port = (char*)calloc(4, sizeof(char*));
		strcpy(myhttp.port, "80");
		printf("input url no port, port is %s\n", myhttp.port);
	}
	else{//extract the port number
		ret = (int)(strchr(myhttp.host, ':')-myhttp.host);
		myhttp.port = (char*)calloc(strlen(myhttp.host) - ret + 3, sizeof(char));
		strcpy(myhttp.port, myhttp.host + ret + 1);
		printf("input url has port, port is %s\n", myhttp.port);
		myhttp.host[strlen(myhttp.host) - strlen(myhttp.port) -1] = '\0';
		printf("input ur host is %s\n", myhttp.host);
	}
	free(temp);//return memory
}



void http_send(char* & sendbuf, info_http& myhttp, int& sockfd){
	int numbytes, ret;
	sendbuf = (char *)calloc(5, sizeof(char*));
	strcpy(sendbuf, "GET ");
	printf("send msg is %s\n", myhttp.msg);
	sendbuf = (char*)realloc(sendbuf, strlen(myhttp.msg)+4100);
	strcpy(sendbuf+4, myhttp.msg);
	strcpy(sendbuf + strlen(sendbuf), " HTTP/1.1\r\nHost: ");
	strcpy(sendbuf + strlen(sendbuf), myhttp.host);
	strcpy(sendbuf + strlen(sendbuf), "\r\n\r\n");
	printf("sendbuf is msg\n%s", sendbuf);
    if((numbytes = send(sockfd, sendbuf, strlen(sendbuf), 0)) != strlen(sendbuf)) {
        perror("send");
        exit(1);
    }
}



int http_recv(char* & buf, info_http& myhttp, int& sockfd, char* & url, int & numbytes){
	int ret, end;
	char temp[10];
	if(buf == NULL){
		buf = (char *)malloc(10000);
	}
	else{
		memset(buf, 0, 10000);
	}
  //receieve
  int buf_size = 10000;
  numbytes = 0;
  while(1){
	  	ret = recv(sockfd, buf + numbytes, 1000, 0);
	  	if(ret == 0){
	  		break;
	  	}
			if(ret == -1) {
			    perror("recv");
			    exit(1);
			}
			numbytes += ret;
			if(strstr(buf, "301")){
				break;
			}

			if(numbytes >= buf_size/2){
				buf = (char*)realloc(buf, buf_size*2);
				buf_size = buf_size*2;
			}
	}

	printf("number read is %d\n", numbytes);
	buf[numbytes] = '\0';
	printf("s: %s\n",buf);

	strncpy(temp, buf+9, 3);

  if(strcmp(temp, "301") != 0){
		return 0;
	}

  //strcpy(buf, "HTTP/1.1 301 Moved Permanently\nLocation: http://www.baidu.com/index.asp");
  memset(url, 0, 4096);
	ret = (int)(strstr(buf, "Location: ") - buf);
	strncpy(url, buf + ret + 10, (int)strlen(buf)-ret);
	end = (int)(strstr(url, "\r\n") - url);
	url[end] = '\0';

	return 1;
}



int tcp_setup(info_http& myhttp, int& sockfd){
	int ret, rv;
	struct addrinfo hints, *servinfo, *p;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(myhttp.host, myhttp.port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}
		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	freeaddrinfo(servinfo); // all done with this structure
}



int main(int argc, char *argv[])
{
	int sockfd, ret, numbytes;
	char * url;
    //check
	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

	url = (char *)calloc(4096, sizeof(char));
	strcpy(url, argv[1]);
	printf("input url is %s\n", url);
	if (strstr(url, "http://") == NULL || url[0] != 'h') {
		fprintf(stderr, "Invalid URL: http:// not exist.\n");
		exit(1);
	}

	char * buf = NULL;
	char * sendbuf = NULL;
  //set up url and tcp
	info_http myhttp;
	while(1){//handle redirect
	  sparceurl(myhttp, url);
	  tcp_setup(myhttp, sockfd);
		http_send(sendbuf, myhttp, sockfd);
	  printf("send successfully, message:\n%s", sendbuf);
	  ret = http_recv(buf, myhttp, sockfd, url, numbytes);
		if(ret != 1)	break;
	  strcat(url, myhttp.msg);
	  printf("The UUURL is %s\n", url);
	}

  //write file
	FILE *fp;
	fp = fopen("output", "w");

	ret = (int)(strstr(buf, "\r\n\r\n") - buf);
	printf("numbytes is %d, ret is %d\n", numbytes, ret);
	fwrite(buf + ret + 4, 1, numbytes - ret - 4, fp);
	fclose(fp);
	close(sockfd);
	free(sendbuf);
	free(buf);
	free(url);
	free(myhttp.msg);
	free(myhttp.host);
	free(myhttp.port);
	printf("end of client\n");
	return 0;
}
