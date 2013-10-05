#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
 
#define PORT 54321
#define THREAD_NUMBER 10
#define STR_LENGTH 20

typedef struct client_info_t
{
  struct sockaddr_in st_client_addr;
  int desc;
  pthread_mutex_t peer_lock;
}client_info_t;

void *thread_client(void *arg)
{
  client_info_t *client_info = (client_info_t*) arg;
  int count, i32_connect_FD = client_info->desc;
  uint32_t length;
  pthread_mutex_unlock(&client_info->peer_lock);
  for(;;)
  {
    char str_in[STR_LENGTH], str_out[STR_LENGTH];
    if (read (i32_connect_FD, &length, sizeof(uint32_t)) == -1)
    {
      printf("Error in read length\n");
      exit (-1);
    }
    printf("len %d\n", length);
    if (read (i32_connect_FD, &str_in, length) == -1)
    {
      printf("Error in read str\n");
      exit (-1);
    }
    sscanf("%d", str_in, &count);
    count++;
    sprintf(str_out, "%d", count);
    length = strlen(str_out);
    printf("out_len %d\n", length);
    printf("%s\n", str_out);
    if (write(i32_connect_FD, length, sizeof(uint32_t)) == -1)
    {
      printf("Error in write length\n");
      exit(-1);
    }
    printf("%s\n", str_in);
    if (write(i32_connect_FD, str_out, length) == -1)
    {
      printf("Error in read str\n");
      exit(-1);
    }
  }
}

int main() 
{
    struct sockaddr_in st_sock_addr, peer_addr;
    socklen_t peer_addr_size = sizeof(struct sockaddr_in);
    pthread_t ids[THREAD_NUMBER];
    int i32_socket_FD = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP ), thread_ind = 0;
    if ( i32_socket_FD == -1 ) 
    {
      printf("Error in creat desc");
      exit( EXIT_FAILURE );
    }
 
    memset( &st_sock_addr, 0, sizeof( st_sock_addr ) );
 
    st_sock_addr.sin_family = PF_INET;
    st_sock_addr.sin_port = htons (PORT);
    st_sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  
    if ( bind( i32_socket_FD, (struct sockaddr *)&st_sock_addr, sizeof( st_sock_addr ) ) == -1)
    {
      printf("Error in bind");
      close( i32_socket_FD );
      exit( EXIT_FAILURE );
    }
    
    if ( listen (i32_socket_FD, 10 ) == -1 ) 
    {
      printf("Error in listen");
      close (i32_socket_FD );
      exit (EXIT_FAILURE );
    }
   
    for(;;) 
      {
 	memset (&peer_addr, 0, sizeof(peer_addr));
        int i32_connect_FD = accept (i32_socket_FD, (struct sockaddr*)&peer_addr, &peer_addr_size);
   
        if ( i32_connect_FD < 0 ) 
	{
	  printf("Error in enter desc");
	  close( i32_socket_FD );
	  exit( EXIT_FAILURE );
        }
	client_info_t client_info;
	client_info.st_client_addr = peer_addr;
	client_info.desc = i32_connect_FD;
	pthread_mutex_init (&client_info.peer_lock, NULL);
	pthread_create (&ids[thread_ind], NULL, &thread_client, &client_info);
	pthread_mutex_lock(&client_info.peer_lock);
	thread_ind++;
	if (thread_ind == THREAD_NUMBER)
	  thread_ind = 0;
    }

    return 0;
}
