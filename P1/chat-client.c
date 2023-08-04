#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>

#include <netinet/in.h> 
#include <netdb.h>  
#include <arpa/inet.h>

#define MAX 4096 


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

int main (int argc, char** argv) {

    int client_socket;
    int server_port_number;
    struct sockaddr_in server_addr;  /*structure for handling internet addresses*/
    char chat_buffer[MAX+23]; /*message buffer*/
    char* host;

    int reading_size, response, trade, max_socket_d;


    if(argc < 3) {
        fprintf(stderr,"Numero invalido de argumentos\n");
        exit(-1);
    }

    host = argv[1];
    server_port_number = parse_int(argv[2], "server_port_number");


    fd_set read_fds;  /*file descriptor set*/

    /* Create a TCP client socket
    (AF_INET is used for IPv4 protocols)
    (SOCK_STREAM is used for TCP)*/

    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) { 
        fprintf(stderr, "erro ao criar client_socket\n"); 
        exit(-1); 
    }

    bzero(&server_addr, sizeof(server_addr)); 

    server_addr.sin_family = AF_INET; 
    server_addr.sin_port = htons(server_port_number); 

    /*Convert IPv4 and IPv6 addresses from text to binary form*/ 
    if(inet_pton(AF_INET, host, &server_addr.sin_addr) == -1) { 
        fprintf(stderr, "Erro em converter host addr\n"); 
        exit(-1); 
    }

    
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) { 
        fprintf(stderr, "coneccao com o servidor falhou\n"); 
        exit(-1);  
    }
    
    fflush(stdout);
 
    while(1) {

        FD_ZERO(&read_fds);   
        FD_SET(client_socket, &read_fds);         /*adds client_socket to fd_set */
        FD_SET(STDIN_FILENO, &read_fds);          /*adds STDIN to fd_set */

        max_socket_d = client_socket;

        if ((trade = select(max_socket_d + 1 , &read_fds , NULL , NULL , NULL)) == -1) { /*select which file discriptor is ready */  
            fprintf(stderr, "error on select\n");  
            exit(-1);   
        }

        if(FD_ISSET(client_socket, &read_fds)) {    /*if the client socket file discriptor is ready */        
            if((response = read(client_socket, chat_buffer, sizeof(chat_buffer))) != 0) {
                printf("%s", chat_buffer);
                bzero(chat_buffer, sizeof(chat_buffer));
            }
            else {
                break;
            }     
        }

        if(FD_ISSET(STDIN_FILENO, &read_fds)) { /*if the stdin file discriptor is ready*/
        	
            if((reading_size = read(STDIN_FILENO, chat_buffer, MAX+23)) == 0) { /*detects ctrl-D*/
                break;
            }

            else {
                if(send(client_socket, chat_buffer, strlen(chat_buffer), 0) == -1) {
                    fprintf(stderr, "Error sending message to server\n"); 
                    exit(-1);
                }
            }
            bzero(chat_buffer, sizeof(chat_buffer));
        }
    } 
    close(client_socket);
    return 0;
}

