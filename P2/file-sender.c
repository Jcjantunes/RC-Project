#include "packet-format.h"

int main (int argc, char **argv) {
    
    struct sockaddr_in receiver_addr;  /*structure for handling internet addresses*/
    struct sockaddr_in receiver_addr_aux;

    socklen_t receiver_addr_length = sizeof(receiver_addr);
 
    int sender_socket;
    
    char* host;
    int receiver_port_number;

    host = argv[2];
    receiver_port_number = parse_int(argv[3], "receiver_port_number");

    FILE* input_file = NULL;

    if(argc < 5) {
      fprintf(stderr,"Numero invalido de argumentos\n");
      exit(-1);
    }

    int window_size = parse_int(argv[4], "window_size"); 
    
    if(window_size > MAX_WINDOW_SIZE) {
      fprintf(stderr, "Error setting window size, it must be less than %i.\n", MAX_WINDOW_SIZE);
      exit(-1);
    }

    struct timeval timeout;
    bzero(&timeout, sizeof(timeout));

    timeout.tv_sec = TIMEOUT/1000;  /* 1 Sec Timeout */
    timeout.tv_usec = 0;

    /* Create a UDP sender socket
    (AF_INET is used for IPv4 protocols)
    (SOCK_DGRAM is used for UDP)*/  
    if((sender_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {  
      fprintf(stderr, "erro ao criar receiver_socket\n"); //canal de erro 
      exit(-1); 
    }

    if(setsockopt(sender_socket, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&timeout, sizeof(struct timeval)) == -1) { 
        fprintf(stderr, "erro em set socket option\n");
        exit(-1); 
    }

    bzero(&receiver_addr, sizeof(receiver_addr));

    receiver_addr.sin_family = AF_INET;  
    receiver_addr.sin_port = htons(receiver_port_number); 

     /*Convert IPv4 and IPv6 addresses from text to binary form*/ 
    if(inet_pton(AF_INET, host, &receiver_addr.sin_addr) == -1) { 
        fprintf(stderr, "Erro em converter host addr\n"); 
        exit(-1); 
    }
  
    input_file = fopen(argv[1], "r");
    
    if(input_file == NULL) {
        fprintf(stderr, "Erro so abrir input_file\n");
        exit(-1);
    }

    struct stat input_file_info;
    
    // Get filesize to calculate total number of chunks
    stat(argv[1], &input_file_info);

    int total_chunks = ((input_file_info.st_size) / MAX_DATA_SIZE) + 1;
    int current_chunk = 1;
    
    char* data_buffer;
    
    int first_packet_received = 1;
    
    int send_base = 1;
    int send_next_chunk = 1;
    
    int chunck_index;
    
    data_pkt_t* send_chunk = malloc (sizeof(data_pkt_t));
    
    ack_pkt_t* recv_chunk = malloc(sizeof(ack_pkt_t));
    
    int recv_base;
    uint32_t selective_ack;

    sender_window sender_window_space[MAX_WINDOW_SIZE];

    sender_window_space[0].seq_num = 1;
    sender_window_space[0].ack = 0;
    sender_window_space[0].data_buffer = NULL;
    sender_window_space[0].data_len = 0;

    int i;
    // sequence space initialization
    for(i = 1; i < window_size; i++) {
      sender_window_space[i].seq_num = 0;
      sender_window_space[i].ack = 0;
      sender_window_space[i].data_buffer = NULL;
      sender_window_space[i].data_len = 0;
    }

    int all_chunks_ack = 0;
    
    int count_retries, chunk_recv, recv_size, n_chunk_ack, j;

    uint32_t chunk_ack, selective_acks_aux;

    int chunk_size, sel_chunk_seq_num;

    while(!all_chunks_ack) {
      while(send_next_chunk < (send_base + window_size) && send_next_chunk <= total_chunks) {
        
        data_buffer = malloc(MAX_DATA_SIZE + 1);
        
        memset (data_buffer, 0, MAX_DATA_SIZE + 1);
        memset(send_chunk, 0, sizeof(data_pkt_t));
        
        chunk_size = create_chunk(input_file, current_chunk, send_chunk);

        //Transmit the packet that was just created
        if(sendto(sender_socket, send_chunk, chunk_size, 0, (struct sockaddr*)&receiver_addr, receiver_addr_length) == -1) {
          fprintf(stderr, "Problem sending packet.\n");
        }

        if(current_chunk < total_chunks) {
          current_chunk++;
        }

        chunck_index = send_next_chunk - send_base;
        sender_window_space[chunck_index].seq_num = send_next_chunk;
        sender_window_space[chunck_index].ack = 0;
        
        memcpy(data_buffer, send_chunk->data, (size_t)MAX_DATA_SIZE);
        
        sender_window_space[chunck_index].data_buffer = data_buffer;
        sender_window_space[chunck_index].data_len = chunk_size;
        
        send_next_chunk++;
      }

      count_retries = 0;
      chunk_recv = 0;
      
      while(!chunk_recv) {

        // Wait 1 second for ACK packet from server
        recv_size = recvfrom(sender_socket, recv_chunk, sizeof(ack_pkt_t), 0, (struct sockaddr*)&receiver_addr, (socklen_t *)&receiver_addr_length);

        if(first_packet_received) {
          receiver_addr_aux = receiver_addr;
          first_packet_received = 0;
        }

        if(receiver_addr.sin_addr.s_addr != receiver_addr_aux.sin_addr.s_addr && receiver_addr.sin_port != receiver_addr_aux.sin_port) {
          fprintf(stderr, "receiver diferente\n");
          break;
        }


        if(recv_size != -1) {
            chunk_recv = 1;
            recv_base = ntohl(recv_chunk->seq_num);
            selective_ack = ntohl(recv_chunk->selective_acks);

            if(recv_base > total_chunks) {
              all_chunks_ack = 1;
              continue;
            }

            // if send_base < recv_base adjust position of elements in sequence space
            if(send_base < recv_base) {
              n_chunk_ack = recv_base - send_base;
              send_base = recv_base;

              for(i = 0; i < window_size; i++) {
                if(i + n_chunk_ack < window_size) {
                  sender_window_space[i].seq_num = sender_window_space[i+n_chunk_ack].seq_num;
                  sender_window_space[i].ack = sender_window_space[i+n_chunk_ack].ack;
                  
                  if(sender_window_space[i].data_buffer != NULL) {
                    free(sender_window_space[i].data_buffer);
                  }

                  sender_window_space[i].data_buffer = sender_window_space[i+n_chunk_ack].data_buffer;
                  sender_window_space[i].data_len = sender_window_space[i+n_chunk_ack].data_len;
                  
                  sender_window_space[i+n_chunk_ack].seq_num = 0;
                  sender_window_space[i+n_chunk_ack].ack = 0;
                  sender_window_space[i+n_chunk_ack].data_buffer = NULL;
                  sender_window_space[i+n_chunk_ack].data_len = 0;
                } 
                else {
                  sender_window_space[i].seq_num = 0;
                  sender_window_space[i].ack = 0;
                  sender_window_space[i].data_buffer = NULL;
                  sender_window_space[i].data_len = 0;
                }
              }
            }
            // use selective_acks to change ack information in sequence space
            selective_acks_aux = selective_ack;
            
            if(selective_acks_aux) {
              for(i = 0; i < window_size; i++) {
                
                chunk_ack = selective_acks_aux & 1;
                if(chunk_ack == 1) {
                  j = recv_base + i + 1 - send_base;
                  sender_window_space[j].ack = 1;
                  selective_acks_aux = selective_acks_aux >> 1;
                }
              }
            }
        }
        else {

          if(count_retries == MAX_RETRIES) {
            fprintf(stderr, "Server does not respond.\n");
            
            free(send_chunk);
            free(recv_chunk);
            fclose(input_file);
            
            exit(-1);
          }

          // retransmits data with ack = 0
          for(i = 0; i < window_size; i++) {
            sel_chunk_seq_num = sender_window_space[i].seq_num;
            
            // for debug
            if(!sender_window_space[i].ack && sel_chunk_seq_num) {
              
              memset(send_chunk, 0, sizeof(data_pkt_t));
              send_chunk->seq_num = htonl(sel_chunk_seq_num);
              
              memcpy(send_chunk->data, sender_window_space[i].data_buffer, (size_t) MAX_DATA_SIZE);
              
              chunk_size = sender_window_space[i].data_len;
              
              //Transmit the packet that was just created
              if(sendto(sender_socket, send_chunk, chunk_size, 0, (struct sockaddr*)&receiver_addr, receiver_addr_length) == -1) {
                fprintf(stderr, "Problem sending packet.\n");
              }
            }
          }
          count_retries++;
        }
      }
    }
   
    free(send_chunk);
    free(recv_chunk);

    fclose(input_file);

    return 0;
}
