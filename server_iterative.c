#define MAX_BUF 8192
#define MAX_FILE_LENGTH 11000000
#define LISTENQ  1024  /* second argument to listen() */

typedef unsigned short u16;
typedef unsigned long u32;

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>


int isValidInput(int argc, char *argv[]){
	if (argc >= 3 && strcmp("-p", argv[1]) == 0)
		return 1;
	else
		return 0;
}

int sendDataToClient(int sockfd, char* bufp, int bufLength){
	int nleft = bufLength;
	int nwritten;

	while (nleft > 0) {
		if ((nwritten = write(sockfd, bufp, nleft)) <= 0) {
	    if (errno == EINTR)  /* interrupted by sig handler return */
				nwritten = 0;    /* and call write() again */
	    else{
	    	printf("%d\n", errno);
	    	perror("ERROR writing to client");
    		return -1;     /* errorno set by write() */
	    }
				
		}
		nleft -= nwritten;
		bufp += nwritten;
  }
  return 1;
}

char* readDataFromClient(int sockfd, char* buf, int* fileLength, int phase, int protocol){
	int length = 0;
	char *bufp = buf;
  char *startBuf = bufp;
	int nread;
	int nwritten;
  int isFirstTime = 1;
	while (1) {
		if ((nread = read(sockfd, bufp, MAX_BUF)) < 0) {
			if (errno == EINTR) { /* interrupted by sig handler return */
				printf("interrupted: %d\n", length);
				nread = 0;    /* and call write() again */
			} else {
		  	perror("ERROR reading from client");
		 		return NULL;      /* errorno set by write() */
		  }
		} else if (nread == 0) {
			printf("Socket closed: %d\n", errno);
			return NULL; //EOF 
		}

		bufp += nread;
		length += nread;

    if (phase == 1){
      if (length == (*fileLength)) break;
    } else {
      if (protocol == 1){
        if (length >= 2 && *(bufp - 2) == '\\' && *(bufp - 1) == '0') {
          break;
        }
      } else {
        //find the length
        if (length >= 4 && isFirstTime == 1){
          char temp = buf[0];
          buf[0] = buf[3];buf[3] = temp;
          temp = buf[1];
          buf[1] = buf[2];buf[2] = temp;
          memcpy(fileLength, buf, 4);
          isFirstTime = 0;
        }
        //after get the length of the message
        if (isFirstTime == 0 && length == (*fileLength) + 4) {break;};
      }
    }
	}

  *fileLength = length;
  return startBuf;
}

int checkSum(char*buf, int length){
  int res = 0;
  while (length > 1){
    int temp = htons(*((unsigned short*)buf));

    res = res + temp;
    if ((res >> 16) & 1) res += 1;
    buf += 2;
    length -= 2;
  }

  if (length == 1){
    res += *((char*) buf);
  }
  if ((res >> 16) & 1) res += 1;
  res = ~res;
  return res;
}

void copyNumberToBuf(int number, char* buf, int length){
	memcpy(buf, &number, length);
}

void numberToBuf(int number, char *buf, int length){
  int i = 0;
  for (i = 0; i < length; i++){
    buf[length - i - 1] = (number >> (i * 8)) & 0xFF;
    if (length == 2){
    }
  }
}

char* processMessage(int protocol, char* buf, int* fileLength){
  char* newBuf = (char*) malloc (sizeof(char)*(MAX_FILE_LENGTH * 2));
  //.Protocol 1
  if (protocol == 1){
    int i = 0, j = 0;
    while (!(buf[i] == '\\' && buf[i + 1] == '0')){
      if (buf[i] != '\\'){
        if (i == 0 || buf[i] != buf[i - 1]) newBuf[j++] = buf[i];
        i++;
      } else {
        if (buf[i + 1] != '\\') {
          fprintf(stderr, "Protocol violated\n" );
          return NULL;
        }
        newBuf[j++] = buf[i++]; newBuf[j++] = buf[i++];
        while (buf[i] == '\\') {
          if (buf[i + 1] != '\\' && buf[i + 1] != '0') {
            fprintf(stderr, "Protocol violated\n" );
            return NULL;
          }
          if (buf[i + 1] == '\\') i += 2;
          if (buf[i + 1] == '0') break;
        }
      }
    }
    newBuf[j++] = '\\';newBuf[j++] = '0';
    *fileLength = j;
    
  } else {
  //.Protocol 2
    
    buf += 4;
    *fileLength -= 4;
    int i = 0, j = 0;newBuf = newBuf + 4;
    while (i < *fileLength){
      if (i == 0 || buf[i] != buf[i - 1]) newBuf[j++] = buf[i];
      i++;
    }
    *fileLength = j;
    newBuf -= 4;
    newBuf[3] = (unsigned char) (*fileLength & 0xFF);
    newBuf[2] = (unsigned char) (*fileLength >> 8 & 0xFF);
    newBuf[1] = (unsigned char) (*fileLength >> 16 & 0xFF);
    newBuf[0] = (unsigned char) (*fileLength >> 24 & 0xFF);
    *fileLength += 4;
    
  }
  return newBuf;
}


int main(int argc, char *argv[]){
	int sockfd, clientFd, clilen;
	struct sockaddr_in serverAdd, clientAdd;
	int port;
	int lengthToSend = 0;
	char *bufToSend;
	int resLength = 0;
	char *resBuf;
  int protocol, trans_id, checksum;
  char* mutualBuf;
  int i = 0;

	if (isValidInput(argc, argv) == 0) {
		fprintf(stderr,"Invalid input\n");
		return 1;
	}
  signal(SIGPIPE, SIG_IGN);  //ignore SIGPINE signal
  port = atoi(argv[2]);
  //Create a socket
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    perror("ERROR opening socket");
      return 1;
  }
  
  // Initialize serverAdd
  bzero((char *) &serverAdd, sizeof(serverAdd));
  serverAdd.sin_family = AF_INET;
  serverAdd.sin_addr.s_addr = htonl(INADDR_ANY);
  serverAdd.sin_port = htons(port);
  //Bind the host address using bind() call.
  if (bind(sockfd, (struct sockaddr *) &serverAdd, sizeof(serverAdd)) < 0) {
    perror("ERROR on binding");
    exit(1);
  }
  int enable = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    error("setsockopt(SO_REUSEADDR) failed");
  
  listen(sockfd,LISTENQ);
  clilen = sizeof(clientAdd);
    

  //CONNECT EACH CLIENT
  while (1) {
    
   // establish connection with each client
    clientFd = accept(sockfd, (struct sockaddr *)&clientAdd, &clilen);
    printf("New Connection\n");
    if (clientFd < 0) {
      perror("ERROR on accept");
      printf("%s\n", "Done with one client");
      continue;
    }

    //CREATE AN MUTUAL MEMORY CHUNK
    mutualBuf = (char*) malloc (sizeof(char)*(MAX_FILE_LENGTH * 2));
    //-------------PHASE 1--------------
    //.READ
    resLength = 8;
    resBuf = readDataFromClient(clientFd, mutualBuf, &resLength, 1, 0);
    if (!resBuf){
      free(mutualBuf);
      close(clientFd);
      printf("%s\n", "Done with one client");
      continue;
    }


    //.WRITE
    protocol = (int) resBuf[1];
    if (protocol == 0) protocol = 2;
    //check checkSum
    checksum = ntohs(*((unsigned short*)( resBuf + 2)));
    numberToBuf(checksum, resBuf + 2, 2);
    if ((unsigned short)checkSum(resBuf, 8) != 0) {
      close(clientFd);
      free(mutualBuf);
      printf("%s\n", "Done with one client");
      continue;
    }

    numberToBuf(1, resBuf, 1);
    numberToBuf(protocol, resBuf + 1, 1);
    int check = sendDataToClient(clientFd, resBuf, 8);
    if (check == -1){
      close(clientFd);
      free(mutualBuf);
      printf("%s\n", "Done with one client");
      continue;
    }
  
    //---------------PHASE 2----------------
    //.READ
    resLength = -1;
    resBuf = readDataFromClient(clientFd, mutualBuf, &resLength, 2, protocol);
    if (!resBuf){
      close(clientFd);
      free(mutualBuf);
      printf("%s\n", "Done with one client");
      continue;
    }
    
    
    //.PROCESS
    bufToSend = processMessage(protocol, resBuf, &resLength);
    if (bufToSend == NULL){
      close(clientFd);
      free(mutualBuf);
      printf("%s\n", "Done with one client");
      continue;
    }

    //.WRITE
    check = sendDataToClient(clientFd, bufToSend, resLength);
    if (check == -1){
      close(clientFd);
      free(mutualBuf);
      printf("%s\n", "Done with one client");
      continue;
    }

    printf("%s\n", "Done with one client");
    free(mutualBuf);
    free(bufToSend);
    close(clientFd);
  }
  
	close(sockfd);
	return 0;
}