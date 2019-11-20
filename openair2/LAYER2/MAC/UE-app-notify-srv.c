#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#define IP_PROTOCOL 0
#define TXRXFLAG 0
#define BIND_PORT 31671
#define BIND_IP "10.0.1.2"
#define BUF_SIZE 1024
#define REQUEST_STR "GET_APP_TYPE"
#define RESPONSE_STR "1"


void clearBuf(char* b) {
	memset(b, 0, BUF_SIZE);
}


int appTypeBuf(char* b) {
    strncpy(b, RESPONSE_STR, BUF_SIZE);
	return strlen(b);
}


int main(int argc, char *argv[]) {
    int sockfd, cfd, nBytes, sentBytes, len, rc;
    struct sockaddr_in my_addr, peer_addr;
    socklen_t my_addr_size, peer_addr_size;
    char buf[BUF_SIZE];


    sockfd = socket(AF_INET, SOCK_STREAM, IP_PROTOCOL);
    if (sockfd < 0) {
        perror("socket:");
		return 1;
	}

    my_addr_size = sizeof(struct sockaddr_in);
    memset(&my_addr, 0, my_addr_size);
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(BIND_PORT);
    my_addr.sin_addr.s_addr = inet_addr(BIND_IP);

    if (bind(sockfd, (struct sockaddr *) &my_addr, my_addr_size) == -1) {
        perror("bind:");
		return 1;
	}

    if (listen(sockfd, 1) == -1) {
        perror("listen:");
		return 1;
	}

    while (1) {
        printf("\nWaiting for a TCP request...\n");

        // accept a TCP peer connection
        peer_addr_size = sizeof(struct sockaddr_in);
        memset(&peer_addr, 0, peer_addr_size);
        cfd = accept(sockfd, (struct sockaddr *) &peer_addr, &peer_addr_size);
        if (cfd == -1) {
          perror("accept:");
            continue;
        }

        printf("\nConnected to a client...\n");

        // receive & send
		while (1) {
			printf("\nWaiting to receive data\n");
			clearBuf(buf);
			nBytes = recv(cfd, buf, BUF_SIZE, TXRXFLAG);
			if (nBytes == -1) {
				perror("recv:");
				break;
			}
			else if (nBytes == 0) {
				printf("\n0 bytes are read from the socket...\n");
				break;
			}
			else {
				// valid request
				if (strncmp(buf, REQUEST_STR, nBytes) == 0) {
					clearBuf(buf);
					len = appTypeBuf(buf);
					sentBytes = 0;
					while (sentBytes < len) {
						rc = send(cfd, buf+sentBytes, len, TXRXFLAG);
						if (rc == -1) {
							perror("sent:");
							break;
						}
						sentBytes += rc;
					}
					if (sentBytes == len) {
						printf("\nApplication type is successfully sent to peer...\n");
						continue;
					}
					else {
						printf("\nProblem on sending!\n");
						break;
					}
				}
				// invalid request
				else {
					printf("\nInvalid request is received!\n");
					break;
				}
			}
		}

		close(cfd);
	}

	return 0;
}


