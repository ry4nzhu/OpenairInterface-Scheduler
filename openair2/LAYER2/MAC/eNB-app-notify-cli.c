#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <limits.h>

#define IP_PROTOCOL 0
#define IP_ADDRESS "10.0.1.2"
#define PORT_NO 31671
#define BUF_SIZE 1024
#define sendrecvflag 0
#define REQUEST_STR "GET_APP_TYPE"
#define SHM_BUF_SIZE 1024
#define SHM_KEY 0x1234


struct shmseg {
	int ue_app_type;
};

int _UE_app_type = -1;

int req2buf(char *buf, int size) {
	strncpy(buf, REQUEST_STR, size);
	return strlen(buf);
}

int main(int argc, char *argv[]) {
  char buf[BUF_SIZE] = {0};
	int len;
	/* socket programming */
  int sockfd, nBytes;
  struct sockaddr_in addr_con;
  int addrlen = sizeof(addr_con);
  addr_con.sin_family = AF_INET;
  addr_con.sin_port = htons(PORT_NO);
  addr_con.sin_addr.s_addr = inet_addr(IP_ADDRESS);
	/* shared memory */
	int shmid;
  struct shmseg *shmp;
	/* strtol */
	int base = 10;
	char *endptr = NULL;
	long val;

	sockfd = socket(AF_INET, SOCK_STREAM, IP_PROTOCOL);
	if (sockfd < 0) {
		perror("socket:");
		return 1;
	}

	if (-1 == connect(sockfd, (struct sockaddr *) &addr_con, addrlen)) {
		perror("connect:");
		return 1;
	}
	printf("Connected to the server.\n");

  shmid = shmget(SHM_KEY, sizeof(struct shmseg), 0644|IPC_CREAT);
  if (shmid == -1) {
     perror("Shared memory");
     return 1;
  }
   	
  // Attach to the segment to get a pointer to it.
  shmp = shmat(shmid, NULL, 0);
  if (shmp == (void *) -1) {
     perror("Shared memory attach");
     return 1;
  }

	while (1) {
		// send
		len = req2buf(buf, sizeof(buf));
		send(sockfd, buf, len, sendrecvflag);
		// TODO: handle error

		// receive
		memset(buf, 0, sizeof(buf));
		nBytes = recv(sockfd, buf, sizeof(buf), sendrecvflag);
		// TODO: handle error
		if (nBytes == -1) {
		  perror("recv error");
		  exit(1);
		}

		// process
		if (nBytes > 0) {
			printf("Received: %s\n", buf);

			errno = 0;    /* To distinguish success/failure after call */
			val = strtol(buf, &endptr, base);
			/* Check for various possible errors */
			if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
				   || (errno != 0 && val == 0)) {
				perror("strtol");
				exit(EXIT_FAILURE);
			}

			if (endptr == buf) {
				fprintf(stderr, "No digits were found\n");
				exit(EXIT_FAILURE);
			}
			/* If we got here, strtol() successfully parsed a number */
			_UE_app_type = (int) val;
			
			/* Transfer blocks of data from buffer to shared memory */
			// TODO: mutex
			shmp->ue_app_type = _UE_app_type;

			sleep(1);
		}
	}

	close(sockfd);

   	if (shmdt(shmp) == -1) {
   	   perror("shmdt");
   	   return 1;
   	}

   	if (shmctl(shmid, IPC_RMID, 0) == -1) {
   	   perror("shmctl");
   	   return 1;
   	}

	return 0;
}


