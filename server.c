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
	if (argc == 3 && strcmp("-p", argv[1]) == 0)
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
		printf("nwritten for sending data: %d\n", nwritten);
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
		//printf("Reading data chunk from client ...\n");
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
        if (length >= 2 && *(bufp - 2) == '\'' && *(bufp - 1) == '0') {
          printf("(Protocol 1): Message Length From Client: %d\n", length);
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
          printf("(Protocol 2): Message Length From Client: %d\n", *fileLength);
          isFirstTime = 0;
        }
        //after get the length of the message
        if (isFirstTime == 0 && length == (*fileLength) + 4) {break;};
      }
    }
		//printf("fileLength for receiving data: %d\n", nread);
    
	}

  *fileLength = length;
  return startBuf;
}

void copyNumberToBuf(int number, char* buf, int length){
	memcpy(buf, &number, length);
}

void sig_handler(int signo)
{  
    printf("received SIGINT %d\n", signo);
}

char* processMessage(int protocol, char* buf, int* fileLength){
  char* newBuf = (char*) malloc (sizeof(char)*(MAX_FILE_LENGTH * 2));
  //.Protocol 1
  if (protocol == 1){
    int i = 0, j = 0;
    while (!(buf[i] == '\'' && buf[i + 1] == '0')){
      if (buf[i] != '\''){
        if (i == 0 || buf[i] != buf[i - 1]) newBuf[j++] = buf[i];
        i++;
      } else {
        newBuf[j++] = buf[i++]; newBuf[j++] = buf[i++];
        while (buf[i] == '\'' && buf[i + 1] == '\'') i += 2;
      }
    }
    newBuf[j++] = '\'';newBuf[j++] = '0';
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
    printf("Length of message to send back in protocol 2 is %d\n", *fileLength);
    newBuf -= 4;
    newBuf[3] = (unsigned char) (*fileLength & 0xFF);
    newBuf[2] = (unsigned char) (*fileLength >> 8 & 0xFF);
    newBuf[1] = (unsigned char) (*fileLength >> 16 & 0xFF);
    newBuf[0] = (unsigned char) (*fileLength >> 24 & 0xFF);
    *fileLength += 4;
    
  }
  return newBuf;
}

//http://www.netfor2.com/tcpsum.htm
/*u16 tcpCheckSum(u16 , u16 src_addr[],u16 dest_addr[], BOOL padding, u16 buff[]) {
	u16 prot_tcp=6;
	u16 padd=0;
	u16 word16;
	u32 sum;	
	
	// Find out if the length of data is even or odd number. If odd,
	// add a padding byte = 0 at the end of packet
	if (padding&1==1){
		padd=1;
		buff[len_tcp]=0;
	}
	
	//initialize sum to zero
	sum=0;
	
	// make 16 bit words out of every two adjacent 8 bit words and 
	// calculate the sum of all 16 vit words
	for (i=0;i<len_tcp+padd;i=i+2){
		word16 =((buff[i]<<8)&0xFF00)+(buff[i+1]&0xFF);
		sum = sum + (unsigned long)word16;
	}	
	// add the TCP pseudo header which contains:
	// the IP source and destinationn addresses,
	for (i=0;i<4;i=i+2){
		word16 =((src_addr[i]<<8)&0xFF00)+(src_addr[i+1]&0xFF);
		sum=sum+word16;	
	}
	for (i=0;i<4;i=i+2){
		word16 =((dest_addr[i]<<8)&0xFF00)+(dest_addr[i+1]&0xFF);
		sum=sum+word16; 	
	}
	// the protocol number and the length of the TCP packet
	sum = sum + prot_tcp + len_tcp;

	// keep only the last 16 bits of the 32 bit calculated sum and add the carries
    	while (sum>>16)
		sum = (sum & 0xFFFF)+(sum >> 16);
		
	// Take the one's complement of sum
	sum = ~sum;

return ((unsigned short) sum);
}*/


int main(int argc, char *argv[]){
	int sockfd, clientFd, clilen;
	struct sockaddr_in serverAdd, clientAdd;
	int port;
	int lengthToSend = 0;
	char *bufToSend;
	int resLength = 0;
	char *resBuf;
  int protocol, trans_id, tcpCheckSum;
  char* mutualBuf;
  int i = 0;

	if (isValidInput(argc, argv) == 0) {
		fprintf(stderr,"Invalid input\n");
		return 0;
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


  //CONNECT EACH CLIENT
  while (1) {
    printf("\n");
    listen(sockfd,LISTENQ);
    clilen = sizeof(clientAdd);
   
   // establish connection with each client
    clientFd = accept(sockfd, (struct sockaddr *)&clientAdd, &clilen);
    if (clientFd < 0) {
      perror("ERROR on accept");
      exit(1);
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
      continue;
    }
    /*printf("PHASE 1 Sent message\n");
    for (i = 0; i < resLength; i++){
      printf("%02x\n", (unsigned char) resBuf[i]);
    }*/

    //.WRITE
    protocol = (int) resBuf[1];
    if (protocol == 0) protocol = 1;
    //check checkSum
    if (0) {
      close(clientFd);
      free(mutualBuf);
      continue;
    }

    copyNumberToBuf(1, resBuf, 1);
    copyNumberToBuf(protocol, resBuf + 1, 1);
    int check = sendDataToClient(clientFd, resBuf, 8);
    if (check == -1){
      close(clientFd);
      free(mutualBuf);
      continue;
    }
  
    //---------------PHASE 2----------------
    //.READ
    resLength = -1;
    resBuf = readDataFromClient(clientFd, mutualBuf, &resLength, 2, protocol);
    if (!resBuf){
      close(clientFd);
      free(mutualBuf);
      continue;
    }
    
    
    //.PROCESS
    bufToSend = processMessage(protocol, resBuf, &resLength);
    if (bufToSend == NULL){
      close(clientFd);
      free(mutualBuf);
      continue;
    }
    /*for (i = 0; i < resLength; i++)
      printf("DM %02x\n", bufToSend[i]);
    printf("---------------\n");*/
    //.WRITE
    check = sendDataToClient(clientFd, bufToSend, resLength);
    if (check == -1){
      close(clientFd);
      free(mutualBuf);
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