#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>

#define MAX_WINDOW_SIZE 32
#define TIMEOUT 1000 // ms
#define MAX_RETRIES 3
#define MAX_DATA_SIZE 1000		// Number of bytes in the data field of a packet
#define TOTAL_RETRIES 3
#define BUFFERLEN 1004
#define ACKLEN 8

typedef struct receiver_window {
  uint32_t seq_num ;
  char received_flag;
  char *data_buffer;
  size_t data_size;
} receiver_window ;

typedef struct sender_window {
  uint32_t seq_num ;
  char ack;
  char *data_buffer;
  int data_len;
} sender_window ;

typedef struct __attribute__((__packed__)) data_pkt_t {
  uint32_t seq_num;
  char data[1000];
} data_pkt_t;

typedef struct __attribute__((__packed__)) ack_pkt_t {
  uint32_t seq_num;
  uint32_t selective_acks;
} ack_pkt_t;

/*--------------------------------------------------------------------
|parse_int:converte o conteudo em str para int
---------------------------------------------------------------------*/

int parse_int(char const *str, char const *name) {
  int parsed_value;

  if(sscanf(str, "%d", &parsed_value) == -1) {
    fprintf(stderr, "Error on argument: \"%s\".\n\n", name); //canal de erro
    exit(-1);
  }
  return parsed_value;
}

int create_chunk(FILE *fd, int chunk_num, data_pkt_t *send_chunk) {
  char buffer[MAX_DATA_SIZE+1];
  int chunk_size = MAX_DATA_SIZE + 4;

  send_chunk->seq_num = htonl(chunk_num);
  memset(buffer,0, MAX_DATA_SIZE+1);

  int file_offset = (chunk_num -1 ) * MAX_DATA_SIZE;
  fseek(fd, file_offset, SEEK_SET);

  size_t chunk_read = fread(buffer, 1, MAX_DATA_SIZE, fd);
  // for debug
  if(chunk_read < MAX_DATA_SIZE) {
    chunk_size = chunk_read + 4;
  }

  memcpy(send_chunk->data, buffer, (size_t)(chunk_size - 4));
  
  return chunk_size;
}