/* Reflect - version 0.1
*/

#define DOS_SPEC
#define DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef DOS_SPEC
#include <winsock.h>
#include <io.h>
#include <fcntl.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif DOS_SPEC

#define LISTENQUEUESIZE 5

// Proxy-IP address: 192.138.158.131 = 3263864451UL
// ----------------: 010.000.000.001 = 167772161UL
#define PROXY_HOST_ADDR_ULONG 167772161UL
#define PROXY_TCP_PORT 8080
#define PROXY_FTP_PORT 21	/*not used here*/

#define MAXBUFFER 32768

unsigned long int allowedClient[]=
{
   3029217081UL, // me
   3029217032UL, // sie-8
   3029249041UL, // nb
   3029217082UL, // pn
   3029225009UL, // pg
   3029225308UL, // ph
   3029225250UL, // hm
   3029225244UL, // pm
   0
};

char* errorMsg="<BODY><H2>You are not allowed to use this service...</H2></BODY>\n\n";

int serverSocketId;

int checkClient (unsigned char b1, unsigned char b2, unsigned char b3, unsigned char b4)
{
return b1!=0;
}

int writebuffer (int sock, char* buffer, int count)
{
   register const char *ptr = (const char *)buffer;
   register int bytesleft = count;

   do
   {
      register int rc;

      do
      {
	 rc = write(sock, ptr, bytesleft);
      } while (rc < 0 && errno == EINTR);

      if (rc < 0) return -1;

      bytesleft -= rc;
      ptr += rc;
   } while (bytesleft);
   return count;
}

void manageConnection (int clientSocketId, unsigned long proxyAddressUlong)
{
#ifdef DEBUG
   printf("new connectionManager for socket %d\n", clientSocketId);
#endif
   {
      int proxySocketId;
      int readByteCount;
      char buffer[MAXBUFFER+1];
      struct sockaddr_in proxyAddressData;
      fd_set readfds;
      fd_set writefds;
      int selectResult;

      memset((char*)&proxyAddressData, 0, sizeof(proxyAddressData));
      proxyAddressData.sin_family = AF_INET;
      proxyAddressData.sin_addr.s_addr = proxyAddressUlong;
      proxyAddressData.sin_port = htons(PROXY_TCP_PORT);

#ifdef DEBUG
	printf("Proxy port is %u, [%u]\n",PROXY_TCP_PORT,proxyAddressData.sin_port);
#endif

      if( (proxySocketId = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
      {
	 perror("thread: can't open stream socket to proxy");
	 close(clientSocketId);
	 return;
      }
#ifdef DEBUG
      printf("proxy socket OK.\n");
#endif
      if ( connect(proxySocketId,(struct sockaddr*)&proxyAddressData,sizeof(proxyAddressData)) < 0 )
      {
	 perror("thread: cannot connect to proxy");
	 close(clientSocketId);
	 close(proxySocketId);
	 return;
      }

#ifdef DEBUG
      printf("thread: connected to proxy\n");
#endif

      while(1)
      {
	 FD_ZERO(&readfds);
	 FD_ZERO(&writefds);
	 FD_SET(clientSocketId, &readfds);
	 FD_SET(proxySocketId, &readfds);
	 FD_SET(clientSocketId, &writefds);
	 FD_SET(proxySocketId, &writefds);

	 selectResult = select(FD_SETSIZE, &readfds, &writefds, 0, 0);

	 if (selectResult < 0) {
#ifdef DEBUG
	printf("'select' failed\n");
#endif
		break;
	 }

	 if ( FD_ISSET(clientSocketId, &readfds) )
	 {
	    int nBytesWritten;
#ifdef DEBUG
	printf("reading from client, writing to proxy\n");
#endif
	    readByteCount = read(clientSocketId,buffer,MAXBUFFER);
#ifdef DEBUG
	printf("reading from client %d\n",readByteCount);
#endif
	    if (readByteCount <= 0) break;

	    nBytesWritten = writebuffer(proxySocketId, buffer, readByteCount);
#ifdef DEBUG
	printf("wrote to proxy %d\n",nBytesWritten);
#endif
	    if ( nBytesWritten != readByteCount )
	    {
	       perror("thread: write error to proxy");
	       break;
	    }
	 }

	 if ( FD_ISSET(proxySocketId,&readfds) )
	 {
#ifdef DEBUG
	    printf("reading from proxy, writing to client\n");
#endif
	    readByteCount = read(proxySocketId, buffer, MAXBUFFER);
	    if (readByteCount <= 0) break;

	    if ( writebuffer(clientSocketId, buffer, readByteCount) != readByteCount )
	    {
	       perror("thread: write error to client");
	       break;
	    }
	 }
      }
      close(proxySocketId);
      close(clientSocketId);
   }

#ifdef DEBUG
   printf("connectionManager for socket %d ended\n", clientSocketId);
#endif
}

int main (int argc, char *argv[])
{
   struct sockaddr_in serverAddressData;
   char* portNrStr;
   int serverPort;

#ifdef DOS_SPEC
// INIT-STUFF (WIN32 specific)
// Startup DLL
WORD wVersionRequested;
WSADATA wsaData;
int err;
wVersionRequested = MAKEWORD( 2, 0 );
err = WSAStartup( wVersionRequested, &wsaData );
if ( err != 0 ) {
	 /* Tell the user that we couldn't find a usable */
	 /* WinSock DLL.                                  */
	 return 1;
}
/* Confirm that the WinSock DLL supports 2.0.*/
/* Note that if the DLL supports versions greater    */
/* than 2.0 in addition to 2.0, it will still return */
/* 2.0 in wVersion since that is the version we      */
/* requested.                                        */
if ( LOBYTE( wsaData.wVersion ) != 2 ||
		  HIBYTE( wsaData.wVersion ) != 0 ) {
	 /* Tell the user that we couldn't find a usable */
	 /* WinSock DLL.                                  */
	 WSACleanup( );
	 return 2;
}
// END-STUFF
#endif DOS_SPEC

   if ( argc < 2 )
	portNrStr = strdup("123");
   else
	portNrStr = argv[1];

   serverPort = atoi(portNrStr);

   if ( (serverSocketId = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
   {
      perror("server: can't open stream socket");
      return 1;
   }

   memset((char*)&serverAddressData,0,sizeof(serverAddressData));
   serverAddressData.sin_family = AF_INET;
   serverAddressData.sin_addr.s_addr = htonl(INADDR_ANY);
   serverAddressData.sin_port = htons(serverPort);
#ifdef DEBUG
	printf("Using server port %u [%u]\n",serverPort,serverAddressData.sin_port);
#endif

   {
      int serverSocketOptional = 1;
      setsockopt(serverSocketId, SOL_SOCKET, SO_REUSEADDR,(char*)&serverSocketOptional, sizeof(int));
   }

   if ( bind(serverSocketId,(struct sockaddr*)&serverAddressData,sizeof(serverAddressData)) < 0 )
   {
      perror("server: can't bind local address");
      close(serverSocketId);
      return 1;
   }

#ifdef DEBUG
   printf("bind OK.\n");
#endif

   if ( listen(serverSocketId,LISTENQUEUESIZE) )
   {
      close(serverSocketId);
      return 1;
   }

#ifdef DEBUG
	printf("Will do infinite loop\n");
#endif

   while (1)
   {
      struct sockaddr_in clientAddressData;
      int clientAddressDataLength = sizeof(clientAddressData);
      int clientSocketId;

#ifdef DEBUG
	printf("Waiting for 'accept'\n");
#endif
      clientSocketId = accept(serverSocketId,(struct sockaddr*)&clientAddressData,&clientAddressDataLength);

#ifdef DEBUG
      printf("accept OK.(%d.%d.%d.%d)\n",
	       clientAddressData.sin_addr.S_un.S_un_b.s_b1,
	       clientAddressData.sin_addr.S_un.S_un_b.s_b2,
	       clientAddressData.sin_addr.S_un.S_un_b.s_b3,
	       clientAddressData.sin_addr.S_un.S_un_b.s_b4);
#endif

      if ( checkClient(clientAddressData.sin_addr.S_un.S_un_b.s_b1,
		       clientAddressData.sin_addr.S_un.S_un_b.s_b2,
		       clientAddressData.sin_addr.S_un.S_un_b.s_b3,
		       clientAddressData.sin_addr.S_un.S_un_b.s_b4)==0 )
      {
	 write(clientSocketId,errorMsg,strlen(errorMsg));
	 close(clientSocketId);
	 continue;
      }

      if (clientSocketId < 0) continue;

#ifdef DEBUG
	printf("Creating child for socket %d\n", clientSocketId);
#endif
      {
	 int childPid;
#ifdef DOS_SPEC
	 childPid = 0;
#else
	 childPid = fork();
#endif DOS_SPEC

#ifdef DEBUG
	printf("Creating child PID=%d\n",childPid);
#endif
	 if ( childPid < 0 ) continue;

	 if (childPid == 0) {
	    close(serverSocketId);
	    manageConnection(clientSocketId,PROXY_HOST_ADDR_ULONG);
#ifdef DOS_SPEC
#ifdef DEBUG
	printf("manageConnection -> DONE\n");
#endif
#else
	    return 0;
#endif DOS_SPEC
	 }
	 // parent
	 close(clientSocketId);
      }
   }
}

