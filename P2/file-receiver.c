#include "packet-format.h"

int main (int argc, char **argv) {
    
    int receiver_port_number, receiver_socket;
    int socket_option_value = 1;

    struct sockaddr_in receiver_addr;  /*structure for handling internet addresses*/
    struct sockaddr_in sender_addr;
    struct sockaddr_in sender_addr_aux;
    
    socklen_t sender_addrlen = sizeof(sender_addr);
    
    FILE* output_file = NULL;

    if(argc < 4) {
        fprintf(stderr,"Numero invalido de argumentos\n");
        exit(-1);
    }

    /*Assign receiver port number*/
    receiver_port_number = parse_int(argv[2], "receiver_port_number");

    /* Create a UDP receiver socket
    (AF_INET is used for IPv4 protocols)
    (SOCK_DGRAM is used for UDP)*/  
    if((receiver_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {  
      fprintf(stderr, "erro ao criar receiver_socket\n"); //canal de erro 
      exit(-1); 
    }

    /*Forcefully attaching the receiver_port to the receiver socket*/ 
    if(setsockopt(receiver_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &socket_option_value, sizeof(socket_option_value)) == -1) { 
        fprintf(stderr, "erro em set socket option\n");
        exit(-1); 
    }

    bzero(&receiver_addr, sizeof(receiver_addr));

    receiver_addr.sin_family = AF_INET; 
    receiver_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    receiver_addr.sin_port = htons(receiver_port_number); 

    /* Bind the receiver socket to receiver address and receiver port*/
    if((bind(receiver_socket, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr))) == -1) { 
        fprintf(stderr, "Error on bind socket\n"); 
        exit(-1); 
    }

    int window_size = parse_int(argv[3], "window_size");  

    if(window_size > MAX_WINDOW_SIZE) {
      fprintf(stderr, "Error setting window size, it must be less than %i.\n", MAX_WINDOW_SIZE);
      exit(-1);
    }

    int last_chunk = 0;
    
    data_pkt_t* recv_chunk = malloc(sizeof(data_pkt_t));
    
    ack_pkt_t* send_chunk = malloc(sizeof(ack_pkt_t));
    
    int recv_base = 1;
    int first_packet_received = 1;
    
    receiver_window receiver_window_space[window_size];
    
    receiver_window_space[0].seq_num = 1;       
    receiver_window_space[0].received_flag = 0;
    receiver_window_space[0].data_buffer = NULL;
    receiver_window_space[0].data_size = 0;

    // initialize receive space
    int i;
    for(i = 1; i < window_size; i++) {  

      receiver_window_space[i].seq_num = 0;
         
      receiver_window_space[i].received_flag = 0;
      receiver_window_space[i].data_buffer = NULL;
      receiver_window_space[i].data_size = 0;
    }

    char *data_buffer;
    
    int last_window = 0;
    int last_seq_num;
    
    int sdr_len = sdr_len;

    int recv_size, chunk_seq_number, offset, chunk_index, recv_seq_num;

    uint32_t selective_acks_aux;

    output_file = fopen(argv[1], "w");

    if(output_file == NULL) {
        fprintf(stderr, "Erro so abrir output_file\n");
        exit(-1);
    }

    // receive packets until last chunk
    while(!last_chunk) {

        bzero(&recv_chunk->data, MAX_DATA_SIZE);

        recv_size = recvfrom(receiver_socket, recv_chunk, BUFFERLEN, 0, (struct sockaddr *)&sender_addr, (socklen_t *)&sender_addrlen);

        if(first_packet_received) {
          sender_addr_aux = sender_addr;
          first_packet_received = 0;
        }

        if(sender_addr.sin_addr.s_addr != sender_addr_aux.sin_addr.s_addr && sender_addr_aux.sin_port != sender_addr.sin_port) {
          fprintf(stderr, "sender diferente\n");
          break;
        }

        chunk_seq_number = ntohl(recv_chunk->seq_num);
        
        if((chunk_seq_number > (recv_base + window_size -1))  || (chunk_seq_number < recv_base) ) {         
          if(sendto(receiver_socket, send_chunk, ACKLEN, 0, (struct sockaddr *)&sender_addr, sizeof(struct sockaddr_storage)) == -1) {
            fprintf(stderr, "Problem sending packet.\n");
          }
          continue;
        }

        if(recv_size < MAX_DATA_SIZE + 4) {
          last_window = 1;
          last_seq_num = chunk_seq_number;
        }

        data_buffer = malloc(recv_size - 4 + 1);

        memset(data_buffer, 0, recv_size - 4 + 1);
        memcpy(data_buffer, recv_chunk->data, (size_t)(recv_size - 4));

        if(chunk_seq_number == recv_base) {  // if received chunk equals recv_base write pending chunks
          receiver_window_space[0].received_flag = 1;
          receiver_window_space[0].data_buffer = data_buffer;
          receiver_window_space[0].data_size = recv_size - 4;
          receiver_window_space[0].seq_num = chunk_seq_number;
          
          while(1) { //packet received
            
            if(receiver_window_space[0].received_flag) {
              offset = (receiver_window_space[0].seq_num - 1)*1000;
              
              fseek(output_file, offset , SEEK_SET);
              fwrite(receiver_window_space[0].data_buffer, receiver_window_space[0].data_size, 1, output_file);
              
              recv_base++;

              if(window_size == 1) {
                receiver_window_space[0].seq_num = recv_base;
                receiver_window_space[0].received_flag = 1;
                break;
              }

              // adjust received space elements
              for(i = 1; i < window_size; i++) {
                
                if(receiver_window_space[i-1].data_buffer != NULL) {
                  free(receiver_window_space[i-1].data_buffer);
                }

                receiver_window_space[i-1].seq_num = receiver_window_space[i].seq_num;
                receiver_window_space[i-1].data_buffer = receiver_window_space[i].data_buffer;
                receiver_window_space[i-1].received_flag = receiver_window_space[i].received_flag;
                receiver_window_space[i-1].data_size = receiver_window_space[i].data_size;

                
                receiver_window_space[i].seq_num = 0;
                receiver_window_space[i].received_flag = 0;
                receiver_window_space[i].data_buffer = NULL;
                receiver_window_space[i].data_size = 0;

              }
            } 
            else {
                break;
            }
          }
        } 
        else {
          chunk_index = chunk_seq_number - recv_base;
          
          receiver_window_space[chunk_index].seq_num = chunk_seq_number;
          receiver_window_space[chunk_index].received_flag = 1;
          receiver_window_space[chunk_index].data_buffer =  data_buffer;
          receiver_window_space[chunk_index].data_size = recv_size - 4;
        }

        // build ack packet
        selective_acks_aux = 0;
        
        for(i = (window_size - 1); i > 0; i--) {
          if(receiver_window_space[i].received_flag) {
            selective_acks_aux = selective_acks_aux << 1;			
            selective_acks_aux = selective_acks_aux | 1;
          } 
          else {
            selective_acks_aux = selective_acks_aux << 1;
          }
        }

        send_chunk->seq_num = htonl(recv_base);
        send_chunk->selective_acks = htonl(selective_acks_aux);
        
        // Send ACK packet    
        if(sendto(receiver_socket, send_chunk, ACKLEN, 0, (struct sockaddr *)&sender_addr, sizeof(struct sockaddr_storage)) == -1) {
          fprintf(stderr, "Problem sending packet.\n");
        }

        // check if all chunks are received_flag
        if(last_window) {
          if(recv_base > last_seq_num) {
            break;
          } 
          else {
            last_chunk = 1;
            
            recv_seq_num = 0;
            while((receiver_window_space[recv_seq_num].seq_num) <= last_seq_num) {
                if(!receiver_window_space[recv_seq_num].received_flag) {
                  last_chunk = 0;
                  
                  break;
                }
                recv_seq_num++;
            }
          }
        }
    }

    free(recv_chunk);
    free(send_chunk);
    
    fclose(output_file);

    return 0;
}
