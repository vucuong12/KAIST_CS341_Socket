#define MAX_BUF 8192
#define MAX_FILE_LENGTH 10111000
#define MAX_CHUNK 500000

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
#include <unistd.h>


int isValidInput(int argc, char *argv[]){
	if (argc < 7 || strcmp("-h", argv[1]) != 0 || strcmp("-p", argv[3]) != 0 || strcmp("-m", argv[5]) != 0 )
		return 0;
  if (strcmp("0", argv[6]) != 0 && strcmp("1", argv[6]) != 0 && strcmp("2", argv[6]) != 0)
    return 0;
  return 1;
}

int min(int a, int b){
  if (a < b) return a;
  return b;
}

int sendDataToServer1(int sockfd, char* bufp, int nleft){

  int maxLengthToSend = 50000;
  int nsent;
  int total = 0;
  fprintf(stderr, "TOTAL %d\n", total);
  while (nleft > 0){
    nsent = send(sockfd, bufp, min(maxLengthToSend, nleft), 0);
    if (nsent < 1) return nsent;
    nleft -= nsent;
    total += nsent;
    /*if (total > 500000)*/ sleep(0.2);
    fprintf(stderr, "TOTAL %d\n", total);
  }
  return 1;
}

int sendDataToServer(int sockfd, char* bufp, int bufLength){
  int maxLengthToSend = 500000;
  int nsent;
	int nleft = bufLength;
	int nwritten = 0;
  int dem = 0;
	while (nleft > 0) {
    nwritten =  send(sockfd, bufp, min(maxLengthToSend, nleft), 0);
    //fprintf(stderr, "Byte sent %d: %d\n", dem++, nwritten);
		if (nwritten <= 0) {
     
	    if (errno == EINTR)  /* interrupted by sig handler return */
				nwritten = 0;    /* and call write() again */
	    else{
	    	perror("ERROR writing to server");
    		return -1;   /* errorno set by write() */
	    }
		}

		nleft -= nwritten;
		bufp += nwritten;
  }
  return 1;
}

char* readDataFromServer(int sockfd, char* buf, int* fileLength, int phase, int protocol){
  int length = 0;
  char *bufp = buf;
  char *startBuf = bufp;
  int nread;
  int nwritten;
  int isFirstTime = 1;
  while (1) {
    if ((nread = read(sockfd, bufp, MAX_BUF)) < 0) {
      if (errno == EINTR) { /* interrupted by sig handler return */
        nread = 0;    /* and call write() again */
      } else {
        perror("ERROR reading from client");
        return NULL;      /* errorno set by write() */
      }
    } else if (nread == 0) {
      fprintf(stderr, "Socket closed\n");
      return NULL;
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
        if (isFirstTime == 0 && length == (*fileLength) + 4) break;
      }
    }
    
  }
  *fileLength = length;
  return startBuf;
}

int readInputFile(char* buf){
  int fileLength = 0;
  buf += 4;
  int nread = 0;

  while((nread = read(STDIN_FILENO, buf, MAX_BUF)) > 0)
  {
    fileLength += nread;
    buf += nread;
  }
  if (nread < 0) {
    perror("ERROR reading input !");
    return -1;
  }
  return fileLength;
}

int writeToFile(char* buf, int bufLength){
  int nwritten;
  while((nwritten = write(STDOUT_FILENO, buf, bufLength)) > 0)
  {
    bufLength -= nwritten;
    if (bufLength == 0) return 1;
    buf += nwritten;
  }
  if (nwritten < 0) {
    perror("ERROR writing output !");
    return -1;
  }
  return 1;
}

void numberToBuf(int number, char *buf, int length){
  int i = 0;
  for (i = 0; i < length; i++){
    buf[length - i - 1] = (number >> (i * 8)) & 0xFF;
    if (length == 2){
    }
  }
}

void copyNumberToBuf(int number, char* buf, int length){
	memcpy(buf, &number, length);
}


//http://www.netfor2.com/tcpsum.htm

//Calculate checksum of char array, starting from buf and having length length
int checkSum1(char* buf, int length){
  int sum = 0;
  while (length > 1) {
    sum += *((unsigned short*)buf);
    buf += 2;
    length -= 2;
  }
  if (length == 1){
    sum += *((char*)buf);
  }
  while (sum >> 16){
    sum = (sum >> 16) + (sum & 0xFFFF);
  }
  return ~sum;
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



int main(int argc, char *argv[]){
	int sockfd;
	struct sockaddr_in serverAdd;
	int port;
	int lengthToSend = 0;
	char *bufToSend;
	int inputLength = 0;
	char *inputBuf;
	int resLength = 0;
	char *resBuf;
  int i = 0; 
  char *mutualBuf;

	if (isValidInput(argc, argv) == 0) {
		fprintf(stderr, "Invalid command\n");
    return 1;
	}
  
	port = atoi(argv[4]);
	//Create a socket
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("ERROR opening socket");
    return 1;
	}
	signal(SIGPIPE, SIG_IGN);
	// Initialize serverAdd
	bzero((char *) &serverAdd, sizeof(serverAdd));
	serverAdd.sin_family = AF_INET;
	serverAdd.sin_addr.s_addr = inet_addr(argv[2]);
	serverAdd.sin_port = htons(port);

	//Connect to the server
	if (connect(sockfd, (struct sockaddr*)&serverAdd, sizeof(serverAdd)) < 0){
		perror("ERROR connecting");
    return 1;
	}

  mutualBuf = (char*) malloc (sizeof(char)* MAX_FILE_LENGTH * 2);

  //----------------------------------------------------------------------------
	//-------------GET DATA TO SEND (saved in inputBuf and inputLength)-----------
  //----------------------------------------------------------------------------

  inputBuf = (char*) malloc (sizeof(char)* MAX_FILE_LENGTH);
	inputLength = readInputFile(inputBuf);
/*  inputLength = 10000000;
  for (i = 0; i < inputLength; i++){
    if ( i < 3000000) inputBuf[i] = 'a';
    else if (i < 6000000) inputBuf[i] = 'b';
    else inputBuf[i] = 'c';
    //if ( i % 5 == 0) inputBuf[i] = '\\';
  }*/

  if (inputLength == -1){
    free(mutualBuf);
    free(inputBuf);
    close(sockfd);
    return 1;
  }

  //-----------------------------------
  //-------------PHASE 1---------------
  //-----------------------------------
  
  int protocol = atoi(argv[6]);
  
  int checksum = 0;
  //calculate trans_id
  unsigned int trans_id = 0;
  //calculate checksum
  //https://tools.ietf.org/html/rfc1071#section-3
  bufToSend = (char*) malloc (sizeof(char)*8);
  numberToBuf(0, bufToSend, 1);
  numberToBuf(protocol, bufToSend + 1, 1);
  numberToBuf(trans_id, bufToSend + 2, 4);

  checksum = checkSum(bufToSend, 6);
  numberToBuf(checksum, bufToSend + 2, 2);
  numberToBuf(trans_id, bufToSend + 4, 4);
  //SEND
  sendDataToServer(sockfd, bufToSend, 8);
  resLength = 8;
  //RECEIVE
  resBuf = readDataFromServer(sockfd, mutualBuf, &resLength, 1, 0);

  //server rejected the connection
  if (!resBuf || (int)resBuf[0] != 1 || *(unsigned int*) (resBuf + 4) != htonl(trans_id)){
    fprintf(stderr, "Phase 1 failed\n" );
    free(mutualBuf);
    free(inputBuf);
    close(sockfd);
    return 1;
  }
  //Confirmed protocol
  protocol = (int) resBuf[1];
  
  //-----------------------------------
  //-------------PHASE 2---------------
  //-----------------------------------

  //1. PROTOCOL 1
  if (protocol == 1){
    char* inputBufHead = inputBuf;
    inputBuf += 4;

    char* tempBuf = (char*) malloc (sizeof(char)*(MAX_FILE_LENGTH * 2 + 100));
    //SPLIT MESSAGE INTO DIFFERENT CHUNKS
    int totalInputLength = inputLength;
    int sentLength;
    while (totalInputLength > 0){
      inputLength = min(totalInputLength, MAX_CHUNK);
      sentLength = inputLength;

      //Add backslash
      int j = 0;
      for (i = 0 ; i < inputLength; i++){
        if (inputBuf[i] == '\\'){
          tempBuf[j++] = '\\';
        } 
        tempBuf[j++] = inputBuf[i];
      }
      tempBuf[j++] = '\\'; tempBuf[j++] = '0';
      inputLength = j;  

      //SEND
      int check = sendDataToServer(sockfd, tempBuf, inputLength);
      if (check == -1){
        free(mutualBuf);
        free(tempBuf);
        free(inputBufHead);
        close(sockfd);
        return 1;
      }

      //RECEIVE RESULT
      resLength = -1;
      resBuf = readDataFromServer(sockfd, mutualBuf, &resLength, 2, protocol);

      //UNWRAP RESULT
      if (resBuf != NULL) {
        //remove backslash
        i = 0; j = 0;
        while (!(resBuf[i] == '\\' && resBuf[i + 1] == '0')){
          if (resBuf[i] != '\\'){
            tempBuf[j++] = resBuf[i++];
          } else {
            if (resBuf[i + 1] != '\\') {
              fprintf(stderr, "Protocol violated !\n");
              free(mutualBuf);
              free(tempBuf);
              free(inputBufHead);
              close(sockfd);
              return 1;
            }
            tempBuf[j++] = resBuf[i++];i++;
          }
        }
      } else {
        fprintf(stderr, "Error reading from server\n");
        free(mutualBuf);
        free(tempBuf);
        free(inputBufHead);
        close(sockfd);
        return 1;
      }
      resLength = j;
      //Printout to file
      writeToFile(tempBuf, resLength);
  
      inputBuf = inputBuf +  sentLength;
      totalInputLength -= sentLength;
    }

    free(tempBuf);
    free (mutualBuf);
    free (inputBufHead);
  }
  //2. PROTOCOL 2
  else {
    char* inputBufHead = inputBuf;
    inputBuf += 4;
    int totalInputLength = inputLength;
    int sentLength;

    while (totalInputLength > 0){
      inputLength = min(totalInputLength, MAX_CHUNK);
      sentLength = inputLength;

      //add length to the beginning of the message to send
      inputBuf -= 4;
      inputBuf[3] = (unsigned char) (inputLength & 0xFF);
      inputBuf[2] = (unsigned char) (inputLength >> 8 & 0xFF);
      inputBuf[1] = (unsigned char) (inputLength >> 16 & 0xFF);
      inputBuf[0] = (unsigned char) (inputLength >> 24 & 0xFF);

      //SEND
      int check = sendDataToServer(sockfd, inputBuf, inputLength + 4);
      if (check == -1){
        free(mutualBuf);
        free(inputBuf);
        close(sockfd);
        return 1;
      }

      //RECEIVE RESULT
      resLength = -1;
      resBuf = readDataFromServer(sockfd, mutualBuf, &resLength, 2, protocol);

      //UNWRAP RESULT
      if (resBuf != NULL) {
        resLength -= 4;
        resBuf += 4;
      } else {
        free(mutualBuf);
        free(inputBuf);
        close(sockfd);
        return 1;
      }

      //Printout to file
      writeToFile(resBuf, resLength);

      totalInputLength -= sentLength;

      inputBuf += (sentLength + 4);
    }

    free (mutualBuf);
    free (inputBufHead);
  }
  free (bufToSend);
  close(sockfd);
	return 0;
}