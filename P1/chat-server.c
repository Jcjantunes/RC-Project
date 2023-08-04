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

/*--------------------------------------------------------------------
|initialize_client_sockets:inicializa o array de client sockets a 0
---------------------------------------------------------------------*/
void initialize_client_sockets(int* client_sockets_array) {
  int i = 0;
  for(i=0; i < 1000; i++) {
    client_sockets_array[i] = 0;
  }
}

int main(int argc, char** argv) {

  fd_set read_fds;   /*file descriptor set*/

  int server_socket,connection_socket; /*sockets used*/

  int server_port_number;
  int socket_option_value = 1;
	
  struct sockaddr_in server_addr;  /*structure for handling internet addresses*/
  char chat_buffer[MAX+1];         /*message buffer*/  
  char response_ip[MAX+23];        /*message buffer*/
  char port_str[6];

  int client_sockets_array[1000];         /*array of client sockets*/

  int i,j,trade,max_socket_d, response;
  
  initialize_client_sockets(client_sockets_array);


  socklen_t addr_len;

  if(argc < 2) {
    fprintf(stderr,"Numero invalido de argumentos\n");
    exit(-1);
  }

	/*Assign server port number*/
  server_port_number = parse_int(argv[1], "server_port_number");

	/* Create a TCP server socket
	(AF_INET is used for IPv4 protocols)
	(SOCK_STREAM is used for TCP)*/	
  if((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {  
    fprintf(stderr, "erro ao criar server_socket\n"); //canal de erro 
    exit(-1); 
  }

  /*Forcefully attaching the server_port to the server socket*/ 
  if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &socket_option_value, sizeof(socket_option_value)) == -1) { 
      fprintf(stderr, "erro em set socket option\n");
      exit(-1); 
  }

  bzero(&server_addr, sizeof(server_addr));

  server_addr.sin_family = AF_INET; 
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
  server_addr.sin_port = htons(server_port_number); 

  /* Bind the server socket to server address and server port*/
  if((bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr))) == -1) { 
      fprintf(stderr, "Error on bind socket\n"); 
      exit(-1); 
  } 

  /* Now server is ready to listen and verification */
  if((listen(server_socket, 1000)) == -1) { 
      fprintf(stderr, "Listen\n"); 
      exit(-1); 
  }

  fflush(stdout);
  printf("Listening on port: %d\n",server_port_number);

  while(1) {
  	
    FD_ZERO(&read_fds);                       /*initializes all file discriptores to 0*/
    FD_SET(server_socket, &read_fds);         /*adds server_socket to fd_set */
    
    max_socket_d = server_socket;
   
    for(i = 0; i < 1000; i++) {       /*adds client_sockets_array to set*/             
        if(client_sockets_array[i] > 0) {    /*if socket exists adds it to the set*/
            FD_SET(client_sockets_array[i], &read_fds);
        }   
        if(client_sockets_array[i] > max_socket_d){   
            max_socket_d = client_sockets_array[i];  /*the max socket value will be used by the select*/
        }   
    }    
   
    if((trade = select( max_socket_d + 1 , &read_fds , NULL , NULL , NULL)) == -1) {  /*selecting the file discriptor to use*/
        fprintf(stderr, "Error on select\n"); 
        exit(-1);   
    }

    if(FD_ISSET(server_socket, &read_fds)) { /*if the server file descriptor is ready */
      addr_len = sizeof(server_addr);
	  
	    /* Accept the data packet from client and verification*/
      /* Set up a new connection from the client*/
	    if((connection_socket = accept(server_socket, (struct sockaddr *)&server_addr, &addr_len)) == -1) { 
	  	   fprintf(stderr, "Error creating connection_socket\n"); 
	       exit(-1); 
	    }

	    getpeername(connection_socket, (struct sockaddr*)&server_addr ,(socklen_t*)&addr_len);
      printf("%s:%d joined.\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));

      strcat(chat_buffer,inet_ntoa(server_addr.sin_addr));
      strcat(chat_buffer,":");
      sprintf(port_str, "%d", ntohs(server_addr.sin_port));
      strcat(chat_buffer,port_str);
      strcat(chat_buffer, " joined.\n");
    
      for(i = 0; i < 1000; i++) {    
          if(client_sockets_array[i] == 0) {   
            client_sockets_array[i] = connection_socket;   /*if a socket is empty it will be the connection socket*/   
            break;   
          }   
      } 

      for(i = 0; i < 1000; i++) {        
        if(client_sockets_array[i] > 0) { 
          if(send(client_sockets_array[i], chat_buffer, strlen(chat_buffer), 0) == -1) { /*if a socket is connected to the server it will send the message*/
          	fprintf(stderr, "Error sending message to client\n"); 
	       	  exit(-1);
          }
        }
      }
      bzero(chat_buffer, sizeof(chat_buffer));
      bzero(port_str, sizeof(port_str));
    }

    for(i = 0; i < 1000; i++) { 
      if(FD_ISSET(client_sockets_array[i], &read_fds)) {  /*if the client file discriptor is ready*/
  	    /*reads the message from client and stores a copy of the message content into the chat_buffer*/ 
        if((response = read(client_sockets_array[i], chat_buffer, sizeof(chat_buffer))) != 0) { /*if the client response is bigger than 0*/
          getpeername(client_sockets_array[i], (struct sockaddr*)&server_addr ,(socklen_t*)&addr_len);
          printf("%s:%d %s", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port) ,chat_buffer);

          strcat(response_ip,inet_ntoa(server_addr.sin_addr));
          strcat(response_ip,":");
          sprintf(port_str, "%d", ntohs(server_addr.sin_port));
          strcat(response_ip, port_str);
          strcat(response_ip, " ");
          strcat(response_ip, chat_buffer);

          for(j = 0; j < 1000; j++) {  
            if ((client_sockets_array[j] > 0) && (client_sockets_array[i] != client_sockets_array[j])) { /*if the socket is active and is different than the current client socket*/
              if(send(client_sockets_array[j], response_ip, strlen(response_ip), 0) == -1) {
              	fprintf(stderr, "Error sending message to client\n"); 
	        	    exit(-1); 
              }
            }
          }
          bzero(chat_buffer, sizeof(chat_buffer));
          bzero(response_ip, sizeof(response_ip));
          bzero(port_str, sizeof(port_str));
        }

        else { /*if the message size is zero it means the client disconected*/
          getpeername(client_sockets_array[i], (struct sockaddr*)&server_addr ,(socklen_t*)&addr_len);
          printf("%s:%d left.\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));

          strcat(chat_buffer,inet_ntoa(server_addr.sin_addr));
          strcat(chat_buffer,":");
          sprintf(port_str, "%d", ntohs(server_addr.sin_port));
          strcat(chat_buffer,port_str);
          strcat(chat_buffer, " left.\n");

          for(j = 0; j < 1000; j++) {   
            if ((client_sockets_array[j] > 0) && (client_sockets_array[i] != client_sockets_array[j])) { /*if the socket is active and is different than the disconected client socket*/
              if(send(client_sockets_array[j], chat_buffer, strlen(chat_buffer), 0) == -1) {
              	fprintf(stderr, "Error sending message to client\n"); 
	        	    exit(-1);
              }
            }
          }
          bzero(chat_buffer, sizeof(chat_buffer));
          bzero(port_str, sizeof(port_str));

          if(close(client_sockets_array[i]) == -1) {
          	fprintf(stderr, "Error closing client socket\n"); 
	       	  exit(-1);
          }   
          client_sockets_array[i] = 0;  
        }
      }
    }
  }
  
  return 0;
}