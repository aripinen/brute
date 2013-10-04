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
        int i32_res, count = 0, length;
        int i32_socket_FD = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP );
	char str[STR_SIZE], str_length[STR_SIZE];
        if ( i32_socket_FD == -1 ) {
                return EXIT_FAILURE;
        }
 
        memset( &st_sock_addr, 0, sizeof( st_sock_addr ) );
 
        st_sock_addr.sin_family = PF_INET;
        st_sock_addr.sin_port = htons( 1100 );
        i32_res = inet_pton( PF_INET, "127.0.0.1", &st_sock_addr.sin_addr );
 
        if ( i32_res < 0 ) {
                close( i32_socket_FD );
                return EXIT_FAILURE;
        } else if ( !i32_res ) {
                close( i32_socket_FD );
                return EXIT_FAILURE;
        }
 
        if ( connect( i32_socket_FD, ( const void* )&st_sock_addr, sizeof(st_sock_addr)) == -1 ) {
                close( i32_socket_FD );
                return EXIT_FAILURE;
        }
	for(;;)
	  {
	    sprintf("%d\0", str, count);
	    printf("%s\n", str);
	    length = strlen(str);
	    sprintf("%d\0", str_length, length);
	    if (write (i32_socket_FD, str_length, 2) == -1)
	      return EXIT_FAILURE;
	    if (write (i32_socket_FD, str, length) == -1)
	      return EXIT_FAILURE;
	    if (read(i32_socket_FD, str, 2) == -1)
	      return EXIT_FAILURE;
	    sscanf("%d", str, &length);
	    if (read(i32_socket_FD, str, length) == -1)
	      return EXIT_FAILURE;
	    sscanf("%d", str, &count);
	    count++;
	  }
        return 0;
}
