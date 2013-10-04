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
 
#define PORT 1100
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
  char str[STR_LENGTH], str_length[STR_LENGTH];
  int count, length, i32_connect_FD = client_info->desc;
  pthread_mutex_unlock(&client_info->peer_lock);
  for(;;)
  {
    if (read (i32_connect_FD, &str, 2) == -1)
      exit (-1);
    sscanf("%d", str, &length);
    if (read (i32_connect_FD, &str, length) == -1)
      exit (-1);
    sscanf("%d", str, &count);
    count++;
    sprintf("%d\0", str,  count);
    length = strlen(str);
    sprintf("%d\0", str_length, length);
    if (write(i32_connect_FD, str_length, 2) == -1)
	exit(-1);
    printf("%s\n", str);
    if (write(i32_connect_FD, str, length) == -1)
      exit(-1);
  }
}

int main() 
{
    struct sockaddr_in st_sock_addr, peer_addr;
    socklen_t peer_addr_size = sizeof(struct sockaddr_in);
    pthread_t ids[THREAD_NUMBER];
    int i32_socket_FD = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP ), thread_ind = 0;
    if ( i32_socket_FD == -1 ) {
        exit( EXIT_FAILURE );
    }
 
    memset( &st_sock_addr, 0, sizeof( st_sock_addr ) );
 
    st_sock_addr.sin_family = PF_INET;
    st_sock_addr.sin_port = htons (PORT);
    st_sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
 
    if ( bind( i32_socket_FD, (struct sockaddr *)&st_sock_addr, sizeof( st_sock_addr ) ) == -1 )
    {
        close( i32_socket_FD );
        exit( EXIT_FAILURE );
    }
 
    if ( listen (i32_socket_FD, 10 ) == -1 ) 
    {
        close (i32_socket_FD );
        exit (EXIT_FAILURE );
    }
 
    for(;;) 
      {
 	memset (&peer_addr, 0, sizeof(peer_addr));
        int i32_connect_FD = accept( i32_socket_FD, (struct sockaddr*)&peer_addr, &peer_addr_size);
        if ( i32_connect_FD < 0 ) 
	{
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
