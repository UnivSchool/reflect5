/* Reflect-Client (for test) - version 0.4
*/

#undef DOS_SPEC
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

#define PROXY_HOST_ADDR_LOCAL_ULONG 2130706433UL /*not-used*/
#define PROXY_HOST_ADDR_ULONG 167772161UL
#define PROXY_TCP_PORT 80
#define PROXY_TCP_PORT2 1234

#define LISTENQUEUESIZE 1

#define MAXBUFFER 32768

int serverSocketId;

int writebuffer (int sock, char* buffer, int count)
{
   register const char *ptr = (const char *)buffer;
   register int bytesleft = count;

#ifdef DEBUG
   printf("<%s> (%d) (socket=%d)\n",buffer,count,sock);
#endif
   if ( sock==-1 ) return count;

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

int manageConnection (int clientSocketId0, char* hostNameStr, unsigned short myPort)
{
  int proxySocketId;
  int readByteCount;
  char buffer[MAXBUFFER+1];
  struct sockaddr_in proxyAddressData;
  fd_set readfds;
  fd_set writefds;
  int selectResult;
  int error;

  unsigned char chr;

  static struct hostent *hp;

  hp = gethostbyname( hostNameStr );

  memset((char*)&proxyAddressData, 0, sizeof(proxyAddressData));
  proxyAddressData.sin_family = AF_INET;
  memcpy( &proxyAddressData.sin_addr, hp->h_addr, hp->h_length );
  //proxyAddressData.sin_addr.s_addr = INADDR_ANY; // -> proxyAddressUlong
  proxyAddressData.sin_port = htons(myPort);

  if ( (proxySocketId = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    perror("thread: can't open stream socket to proxy");
    //	 close(clientSocketId);
    return -1;
  }

#ifdef DEBUG
  printf("proxy socket OK: %d, port %u\n",proxySocketId,myPort);
  printf("b1=%u\n",(unsigned)(proxyAddressData.sin_addr.s_addr >> 24));
  printf("b2=%u\n",(unsigned)(proxyAddressData.sin_addr.s_addr >> 16) & 0xFF);
  printf("b3=%u\n",(unsigned)(proxyAddressData.sin_addr.s_addr >>  8) & 0xFF);
  printf("b4=%u\n",(unsigned)(proxyAddressData.sin_addr.s_addr) & 0xFF);
#endif

  error = connect(proxySocketId,(struct sockaddr*)&proxyAddressData,sizeof(proxyAddressData));
  if ( error < 0 ) {
    perror("thread: cannot connect to proxy");
    // close(clientSocketId);
    close(proxySocketId);
    return -2;
  }

#ifdef DEBUG
  printf("thread: connected to proxy\n");
#endif

  while ( (error = fscanf(stdin,"%s",buffer))==1 ) {
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    //	 FD_SET(clientSocketId, &readfds);
    FD_SET(proxySocketId, &readfds);
    //	 FD_SET(clientSocketId, &writefds);
    FD_SET(proxySocketId, &writefds);

    selectResult = select(FD_SETSIZE, &readfds, &writefds, 0, 0);

    if (selectResult < 0) {
#ifdef DEBUG
      printf("'select' failed\n");
#endif
      break;
    }

    if ( clientSocketId0!=-1 && FD_ISSET(clientSocketId0, &readfds) ) {
      int nBytesWritten;
#ifdef DEBUG
      printf("reading from client, writing to proxy\n");
#endif
      readByteCount = read(clientSocketId0,buffer,MAXBUFFER);
#ifdef DEBUG
      printf("reading from client %d\n",readByteCount);
#endif
      if (readByteCount <= 0) break;
      
      nBytesWritten = writebuffer(proxySocketId, buffer, readByteCount);
#ifdef DEBUG
      printf("wrote to proxy %d\n",nBytesWritten);
#endif
      if ( nBytesWritten != readByteCount ) {
	perror("thread: write error to proxy");
	break;
      }
    }
    
    if ( clientSocketId0==-1 && buffer[0]=='a' ) {
      int nBytesWritten;

      readByteCount = strlen(buffer);
      
      nBytesWritten = writebuffer(proxySocketId,buffer,readByteCount);
#ifdef DEBUG
      printf("wrote to proxy %d bytes\n",nBytesWritten);
#endif
      if ( nBytesWritten != readByteCount ) {
	perror("thread: write error to proxy");
	break;
      }
    }

    if ( FD_ISSET(proxySocketId,&readfds) ) {
#ifdef DEBUG
      printf("reading from proxy, writing to client\n");
#endif
      readByteCount = read(proxySocketId, buffer, MAXBUFFER);
      if ( readByteCount <= 0 ) {
#ifdef DEBUG
	printf("Did read nothing from proxy (%d)\n",readByteCount);
#endif
	break;
      }
      
      if ( writebuffer(clientSocketId0, buffer, readByteCount) != readByteCount ) {
	perror("thread: write error to client");
	break;
      }
    }
  }// end WHILE
  
  close(proxySocketId);
  // close(clientSocketId);
  return 0;
}

int main (int argc, char *argv[])
{
  char* portNrStr;
  int i;
  unsigned short myPort=PROXY_TCP_PORT;

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
 else {
   portNrStr = argv[1];
   myPort = atoi(portNrStr);
 }   

#ifdef DEBUG
 printf("Will do infinite loop\n");
#endif

 do {
   struct sockaddr_in clientAddressData;
   int clientAddressDataLength = sizeof(clientAddressData);
   int clientSocketId = -1;

   i = manageConnection(clientSocketId,"fuji",myPort);

#ifdef DEBUG
   printf("manageConnection DONE:%d\n",i);
#endif
 } while ( i<0 && i!=-2 );
}

