#define MAX_BUF 8192
#define MAX_FILE_LENGTH 10001000

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
  if (argc == 7) return 1;
  if (argc != 9 && argc != 11) return 0;
  if (argc == 9){
    if (strcmp("<", argv[7]) != 0 && strcmp(">", argv[7]) != 0) return 0;
  } else {
    if (strcmp("<", argv[7]) != 0 && strcmp(">", argv[7]) != 0) return 0;
    if (strcmp("<", argv[9]) != 0 && strcmp(">", argv[9]) != 0) return 0;
  }
  return 1;
}

int sendDataToServer(int sockfd, char* bufp, int bufLength){
	int nleft = bufLength;
	int nwritten;

	while (nleft > 0) {
		if ((nwritten = write(sockfd, bufp, nleft)) <= 0) {
	    if (errno == EINTR)  /* interrupted by sig handler return */
				nwritten = 0;    /* and call write() again */
	    else{
	    	perror("ERROR writing to server");
    		return -1;   /* errorno set by write() */
	    }
				
		}
		////printf("nwritten for sending data: %d\n", nwritten);
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
        ////printf("interrupted: %d\n", length);
        nread = 0;    /* and call write() again */
      } else {
        perror("ERROR reading from client");
        return NULL;      /* errorno set by write() */
      }
    } else if (nread == 0) {
      perror("Socket closed: \n");
      return NULL;
    }

    bufp += nread;
    length += nread;

    if (phase == 1){
      if (length == (*fileLength)) break;
    } else {
      if (protocol == 1){
        if (length >= 2 && *(bufp - 2) == '\'' && *(bufp - 1) == '0') {
          ////printf("(PHASE 2 - Protocol 1): Message Length From Server: %d\n", length);
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
          ////printf("(PHASE 2 - Protocol 2): Message Length From Server: %d\n", *fileLength);
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
  ////printf("Da read %d bytes\n", nread);
  while((nread = read(STDIN_FILENO, buf, MAX_BUF)) > 0)
  {
    //printf("1111\n");
    fileLength += nread;
    buf += nread;
  }
  //printf("ket thuc\n");
  if (nread < 0) {
    perror("ERROR reading input !");
    return -1;
  }
  return fileLength;

/*
  FILE * pFile;
  long lSize;
  size_t result;

  pFile = fopen (inputFileName, "rb" );
  if (pFile==NULL) {
  	perror("ERROR opening input file");
    exit(1);
  }

  // obtain file size:
  fseek(pFile , 0 , SEEK_END);
  lSize = ftell(pFile);
  rewind(pFile);
  if (lSize > MAX_FILE_LENGTH) {
  	////printf("ERROR exceeded max file length");
  	exit(1);
  }

  // copy the file into the buffer:
  result = fread ((*buf) + 4,1,lSize,pFile);
  if (result != lSize) {
  	perror("ERROR copying file into buffer");
    exit(1);
  }
  fclose (pFile);
  return (int) result;*/
}

int writeToFile(char* buf, int bufLength){
  int nwritten;
  while((nwritten = write(STDOUT_FILENO, buf, bufLength)) > 0)
  {
    //printf("Da write %d bytes\n", nwritten);
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

void copyNumberToBuf(int number, char* buf, int length){
	memcpy(buf, &number, length);
}


//http://www.netfor2.com/tcpsum.htm

//Calculate checksum of char array, starting from buf and having length length
int checkSum(char* buf, int length){
  int sum = 0;
  while (length > 1) {
    sum += *(buf++);
    length -= 2;
  }
  if (length == 1){
    sum += *((char*)buf);
  }
  while (sum >> 16){
    sum = (sum >> 16) + (sum & 0xFFFF);
  }
  return  ~sum;
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

	////printf("Number of arguments is: %d\n", argc);
	if (isValidInput(argc, argv) == 0) {
		////printf(stderr,"Invalid input\n");
		return 0;
	}
  
	port = atoi(argv[4]);
	//Create a socket
  ////printf("asdfsdfsfadad\n");
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("ERROR opening socket");
    return 1;
	}
  ////printf("asdfsdfsfadad\n");
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
  ////printf("asdfsdfsfadad\n");

  mutualBuf = (char*) malloc (sizeof(char)* MAX_FILE_LENGTH * 2);

  //----------------------------------------------------------------------------
	//-------------GET DATA TO SEND (saved in inputBuf and inputLength)-----------
  //----------------------------------------------------------------------------
  inputBuf = (char*) malloc (sizeof(char)* MAX_FILE_LENGTH);
	inputLength = readInputFile(inputBuf);
  ////printf("Length of input is: %d\n", inputLength);

  if (inputLength == -1){
    free(mutualBuf);
    free(inputBuf);
    close(sockfd);
    return 1;
  }


  //-----------------------------------
  //-------------PHASE 1---------------
  //-----------------------------------\
  
  int protocol = atoi(argv[6]);
  //calculate checksum
  //https://tools.ietf.org/html/rfc1071#section-3
  int checksum = 0xddf2;
  //calculate trans_id
  int trans_id = 0;
  
  bufToSend = (char*) malloc (sizeof(char)*8);
  copyNumberToBuf(0, bufToSend, 1);
  copyNumberToBuf(protocol, bufToSend + 1, 1);
  //copyNumberToBuf(checksum, bufToSend + 2, 2);
  copyNumberToBuf(trans_id, bufToSend + 2, 4);
  for (i = 0; i < 8; i++){
    ////printf("%02x\n", (unsigned char) bufToSend[i]);
  }
  ////printf("--------------\n");
  checksum = checkSum(bufToSend, 6);
  ////printf("Check sum is : %d\n", checksum);
  ////printf("%04x\n", checksum);
  copyNumberToBuf(checksum, bufToSend + 2, 2);
  copyNumberToBuf(trans_id, bufToSend + 4, 4);
  ////printf("PHASE 1 Sent message\n");
  for (i = 0; i < 8; i++){
    printf("%02x\n", (unsigned char) bufToSend[i]);
  }
  sendDataToServer(sockfd, bufToSend, 8);
  resLength = 8;
  resBuf = readDataFromServer(sockfd, mutualBuf, &resLength, 1, 0);

  //server rejected the connection
  if (!resBuf){
    free(mutualBuf);
    free(inputBuf);
    close(sockfd);
    return 1;
  }
  protocol = (int) resBuf[1];


  //-----------------------------------
  //-------------PHASE 2---------------
  //-----------------------------------

  //1. PROTOCOL 1
  if (protocol == 1){
    //Add backslash
    char* tempBuf = (char*) malloc (sizeof(char)*(inputLength * 2 + 100));
    inputBuf += 4;
    int j = 0;
    for (i = 0 ; i < inputLength; i++){
      if (inputBuf[i] == '\''){
        tempBuf[j++] = '\'';
      }
      tempBuf[j++] = inputBuf[i];
    }
    tempBuf[j++] = '\''; tempBuf[j++] = '0';
    inputLength = j;  
     
    //SEND
    int check = sendDataToServer(sockfd, tempBuf, inputLength);
    if (check == -1){
      free(mutualBuf);
      free(tempBuf);
      free(inputBuf - 4);
      close(sockfd);
      return 1;
    }

    //RECEIVE RESULT
    resLength = -1;
    resBuf = readDataFromServer(sockfd, mutualBuf, &resLength, 2, protocol);
    
    if (resBuf != NULL) {
      //remove backslash
      i = 0; j = 0;
      while (!(resBuf[i] == '\'' && resBuf[i + 1] == '0')){
        if (resBuf[i] != '\''){
          tempBuf[j++] = resBuf[i++];
        } else {
          tempBuf[j++] = resBuf[i++];i++;
        }
      }
    }

    resLength = j;
    //printf("Data receive from server\n");
    for (i = 0; i < resLength; i++){
      //printf("%d: %c\n",i, (unsigned char) tempBuf[i]);
    }
    ////printf("(Protocol 1) Final result's length %d\n", resLength);
    //Printout to file
    if (resBuf != NULL)
      writeToFile(tempBuf, resLength);
    else 
      ////printf("Protocol violated !\n");

    free(tempBuf);
    free (mutualBuf);
    free (inputBuf - 4);
  }
  //2. PROTOCOL 2
  else {
    //add length to the beginning of the message to send
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
    if (resBuf != NULL) {
      resLength -= 4;
      resBuf += 4;
    }
    ////printf("(Protocol 2) Final result's length %d\n", resLength);

    //printf("Data receive from server %d\n", resLength);
    for (i = 0; i < resLength; i++){
      //printf("%d: %c\n",i, (unsigned char) resBuf[i]);
    }
    //Printout to file
    if (resBuf != NULL)
      writeToFile(resBuf, resLength);
    else 
      ////printf("Protocol violated !\n");

    free (mutualBuf);
    free (inputBuf);
  }
  free (bufToSend);
  close(sockfd);
	return 0;
}