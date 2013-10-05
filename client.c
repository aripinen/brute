#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
 
#define STR_SIZE 10
int main( void )  {
        struct sockaddr_in st_sock_addr;
        int i32_res, count = 0;
	uint32_t length;
        int i32_socket_FD = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP );
        if ( i32_socket_FD == -1 ) 
	{
	  printf("Error in socket\n");
	  return EXIT_FAILURE;
        }
 
        memset( &st_sock_addr, 0, sizeof( st_sock_addr ) );
	
        st_sock_addr.sin_family = PF_INET;
        st_sock_addr.sin_port = htons( 1100 );
        i32_res = inet_pton( PF_INET, "127.0.0.1", &st_sock_addr.sin_addr );
 
        if ( i32_res < 0 ) 
	{
	  close( i32_socket_FD );
	  printf("Error in addr\n");
	  return EXIT_FAILURE;
        } else if ( !i32_res ) 
	{
	  printf("Error in addr\n");
	  close( i32_socket_FD );
	  return EXIT_FAILURE;
        }
 
        if ( connect( i32_socket_FD, ( const void* )&st_sock_addr, sizeof(st_sock_addr)) == -1 )
	{
	  printf("Can't connect to server\n");
	  close( i32_socket_FD );
	  return EXIT_FAILURE;
        }
	for(;;)
	  {
	    char str_in[STR_SIZE], str_out[STR_SIZE];
	    sprintf (str_out, "%d", count);
	    printf ("%s\n", str_out);
	    length = strlen (str_out);
	    printf("len %d\n", length);

	    if (write (i32_socket_FD, length, sizeof(length)) == -1)
	      return EXIT_FAILURE;

	    if (write (i32_socket_FD, str_out, length) == -1)
	      return EXIT_FAILURE;

	    if (read(i32_socket_FD, &length, sizeof(length)) == -1)
	      return EXIT_FAILURE;

	    if (read(i32_socket_FD, &str_in, length) == -1)
	      return EXIT_FAILURE;
	    sscanf("%d", str_in, &count);
	    count++;
	  }
        return 0;
}
