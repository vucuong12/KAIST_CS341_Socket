#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>

#define MESS_LEN_LIMIT 10485760
#define BYTE_PER_SEND 1024

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

uint16_t calculateCheckSum(char* buffer) {
  /* Return the checksum of the char array buffer with size 6 */

  uint32_t sum = 0;
  int i;
  for (i = 0; i < 6; i += 2) {
    sum += (int) ((buffer[i] << 8) + (buffer[i+1] & 0xff));
  }

  while (sum >> 16 != 0) {
    sum = (sum >> 16) + (sum & 0xffff);
  }

  return (uint16_t) (~(sum & 0xffff));
}

void copyIntToBuffer(char* buffer, uint32_t num, int from, int len) {
  /* Copy an integer number into a char array 1 byte per slot.
     Starting at from.
     Ending at from+len.
   */

  int i;
  for (i = 0; i < len; i++) {
    int temp = (len - i - 1) * 8;
    buffer[from+i] = (num >> temp) & 0xff;
  }
}

int main(int argc, char *argv[]) {

  int cli_sock, i, mess_len, walk;
  uint16_t cksum;
  struct sockaddr_in ser_addr;
  char temp_buffer[6], phase1_buf[8], recv_mess[BYTE_PER_SEND];

  memset(recv_mess, 0, sizeof(recv_mess));

  if ((cli_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("Error retrieving socket descriptor.\n");
    exit(1);
  }

  if (argc < 7) {
    printf("Wrong input format: –h [server IP addr] –p [port] –m [protocol].\n");
    exit(1);
  }

  ser_addr.sin_family = AF_INET;
  ser_addr.sin_port = htons(atoi(argv[4]));   //31415
  ser_addr.sin_addr.s_addr = inet_addr(argv[2]);  //143.248.48.110

  if (connect(cli_sock, (struct sockaddr *) &ser_addr, sizeof(ser_addr)) < 0) {
    printf("Error connecting to server.\n");
    exit(1);
  }

  //=====================CHECKSUM CALCULATION=========================
  temp_buffer[0] = 0;       // client protocol proposal
  temp_buffer[1] = atoi(argv[6]); // client proposal choice
  uint32_t trans_id = random(); // unique id of current data transmission
  
  copyIntToBuffer(temp_buffer, trans_id, 2, (int) sizeof(trans_id));
  cksum = calculateCheckSum(temp_buffer);

  //==============MAKE AND SEND NEGOTIATION MESSAGE===================
  phase1_buf[0] = 0;
  phase1_buf[1] = atoi(argv[6]);
  copyIntToBuffer(phase1_buf, (uint32_t) cksum, 2, (int) sizeof(cksum));
  copyIntToBuffer(phase1_buf, trans_id, 4, (int) sizeof(trans_id));

  send(cli_sock, phase1_buf, sizeof(phase1_buf), 0);
  recv(cli_sock, recv_mess, sizeof(recv_mess), 0);


  //  char send_mess[BYTE_PER_SEND], phase2_buf[MESS_LEN_LIMIT], mess[MESS_LEN_LIMIT], processed_mess[MESS_LEN_LIMIT*2];
  char* send_mess = (char*) malloc (sizeof(char)* MESS_LEN_LIMIT * 2);
  char* phase2_buf = (char*) malloc (sizeof(char)* MESS_LEN_LIMIT * 2);
  char* mess = (char*) malloc (sizeof(char)* MESS_LEN_LIMIT * 2);
  char *processed_mess = (char*) malloc (sizeof(char)* MESS_LEN_LIMIT * 2);

  memset(send_mess, 0, sizeof(send_mess));
  memset(phase2_buf, 0, sizeof(phase2_buf));
  memset(mess, 0, sizeof(mess));
  memset(processed_mess, 0, sizeof(processed_mess));

  int prot = recv_mess[1];

  //printf("Success Negotiation.\n");

  //=======================GET MESSAGE========================
  while (fgets(phase2_buf, MESS_LEN_LIMIT, stdin)) {
    if (strlen(phase2_buf) + strlen(mess) > MESS_LEN_LIMIT) {
      printf("Maximum length of message is 10MB.\n");
      exit(1);
    }
    strcat(mess, phase2_buf);
    memset(phase2_buf, 0, sizeof(phase2_buf));
  }
  //int i = 0;
  for (i = 0; i < 3000000; i++){
    mess[i] = 'a';
    if ( i % 2 == 0) mess[i] = '\\';
  }
  fprintf(stderr, "Length of input file %d\n", (int) strlen(mess));
  //printf("message input complete.\n");

  //======================PROTOCOL PROCESS=======================
  if (recv_mess[1] == 1) {  // protocol 1
    walk = 0;
    for (i = 0; i < 3000000; i++) {
      processed_mess[walk] = mess[i];
      walk++;
      if (mess[i] == 92) {
        processed_mess[walk] = mess[i];
        walk++;
      }
    }
    processed_mess[walk] = '\\';
    processed_mess[walk+1] = '0';
    mess_len = walk + 2;
    //printf("processed message: %s\n", processed_mess);
  }
  else if (recv_mess[1] == 2) { //protocol 2
    mess_len = strlen(mess) + 4;
    copyIntToBuffer(processed_mess, (int) strlen(mess), 0, 4);
    strcat(&processed_mess[4], mess);
    //printf("processed message: %d%s\n", processed_mess[3], &processed_mess[4]);
  }
  else {
    printf("undefined protocol: %d\n", recv_mess[1]);
    exit(1);
  }
  //writeToFile(processed_mess, mess_len);
  fprintf(stderr, "DM TA: %d\n", mess_len);
  //====================SENDING MESSAGE========================
  int send_len, recv_len;
  int time_send = mess_len / BYTE_PER_SEND;
  if (mess_len % BYTE_PER_SEND != 0) time_send++;
  //fprintf(stderr, "asdasdsad\n" );
  for (i = 0; i < time_send; i++) {   
    
    if ((send_len = send(cli_sock, &processed_mess[i*BYTE_PER_SEND], BYTE_PER_SEND, 0)) < 0) {
      printf("Message Sending Fail.\n");
      exit(1);
    }
    //fprintf(stderr, "Send_len %d\n", send_len);
  }

  //====================RECEIVING MESSAGE=======================
  memset(processed_mess, 0, sizeof(processed_mess));  // store sv reply mess
  memset(mess, 0, sizeof(mess));  // store decoded message
  mess_len = 0; // store mess length of protocol 2

  if (prot == 2) {
    recv_len = recv(cli_sock, recv_mess, BYTE_PER_SEND, 0);
    mess_len = (recv_mess[0]<<24) + (recv_mess[1]<<16) + (recv_mess[2]<<8) + recv_mess[3];
    strcat(processed_mess, &recv_mess[4]);  
  }

  while ((recv_len = recv(cli_sock, recv_mess, BYTE_PER_SEND, 0)) > 0) {
    strcat(processed_mess, recv_mess);
    memset(recv_mess, 0, sizeof(recv_mess));

    if (prot == 2 && strlen(processed_mess) >= mess_len) {
      break;
    }

    if (recv_len == 0 || recv_len != BYTE_PER_SEND) //signaling end of file
      break;
  }

  if (prot == 2) {
    strncat(mess, processed_mess, mess_len);
  }

  if (prot == 1) {
    walk = 0;
    for (i = 0; i < strlen(processed_mess); i++)  {
      mess[walk] = processed_mess[i];
      if (processed_mess[i] == 92) {
        if (processed_mess[i+1] == 92) i++;
        else if (processed_mess[i+1] == '0') {
          mess[walk] = 0;
          break;
        }
      }
      walk++;
    }
  }
  close(cli_sock);
  //writeToFile(mess, strlen(mess));
  //printf("%s", mess);
}