/* Reflect - version 0.5
*/

#undef DOS_SPEC
#define DEBUG
#undef DEBUG

#define DEBUG_IFSHORT

#define HAS_FORK

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
#define PROXY_TCP_PORT 80
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

void ShowString (char* s, int byteCount)
{
  unsigned char* buf;
  static unsigned i, chr;

  if ( byteCount<0 ) return;
  buf = (char*)malloc(byteCount+1);
  strncpy(buf,s,byteCount);
  buf[byteCount] = 0;
  printf("<");
  for (i=0; i<byteCount; i++) {
    chr = (unsigned)buf[i];
    if ( chr<' ' || chr>=127 )
      printf("[%02X]\n",chr);
    else
      printf("%c",chr);
  }
  printf(">\n");
  free(buf);
}

int checkClient (unsigned char b1, unsigned char b2, unsigned char b3, unsigned char b4)
{
#ifdef DEBUG
  printf("Request_by:%u.%u.%u.%u\n",
	 b1, b2, b3, b4);
#endif
  return b1!=0;
}

int writebuffer (int sock, char* buffer, int count) {
  register const char *ptr = (const char *)buffer;
  register int bytesleft = count;

  do {
    register int rc;
    do {
      rc = write(sock, ptr, bytesleft);
    } while (rc < 0 && errno == EINTR);
    
    if (rc < 0) return -1;
    
    bytesleft -= rc;
    ptr += rc;
  } while (bytesleft);
  return count;
}

int manageConnection (int clientSocketId, unsigned long proxyAddressUlong, char* hostNameStr) {
  int proxySocketId;
  int readByteCount;
  char buffer[MAXBUFFER+1];
  fd_set readfds;
  fd_set writefds;
  int selectResult;
  int error;

  static struct hostent *hp = 0;

  struct sockaddr_in proxyAddressData;

#ifdef DEBUG
  printf("new connectionManager for socket %d\n", clientSocketId);
#endif

  memset( (char*)&proxyAddressData, 0, sizeof(proxyAddressData) );
  proxyAddressData.sin_family = AF_INET;

#ifdef KNOW_HOSTBYNAME
  hp = gethostbyname( hostNameStr );
#endif

  proxyAddressData.sin_addr.s_addr = proxyAddressUlong;
  if ( hp!=0 ) {
    memcpy( &proxyAddressData.sin_addr, hp->h_addr, hp->h_length );
  }
  proxyAddressData.sin_port = htons(PROXY_TCP_PORT);

#ifdef DEBUG
  printf("Proxy port is %u, [sinPort=%u]\n",PROXY_TCP_PORT,proxyAddressData.sin_port);
#ifdef KNOW_HOSTBYNAME
  printf("Addr1=%u (host=%s)\n",(unsigned)((proxyAddressUlong >> 24) & 0xFF),hostNameStr);
  printf("Addr4=%u\n",(unsigned)(proxyAddressUlong & 0xFF));
#else
  printf("Addr:%lu\n",proxyAddressUlong);
#endif KNOW_HOSTBYNAME
#endif

  if ( (proxySocketId = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    perror("thread: can't open stream socket to proxy");
    close(clientSocketId);
    return -1;
  }
#ifdef DEBUG
  printf("proxy socket OK: %d\n",proxySocketId);
#endif

  error = connect(proxySocketId,(struct sockaddr*)&proxyAddressData,sizeof(proxyAddressData));
  if ( error < 0 ) {
    perror("Thread: cannot connect to proxy");
    close(clientSocketId);
    close(proxySocketId);
    return -2;
  }

  while ( 1 ) {
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_SET(clientSocketId, &readfds);
    FD_SET(proxySocketId, &readfds);
    FD_SET(clientSocketId, &writefds);
    FD_SET(proxySocketId, &writefds);

    selectResult = select(FD_SETSIZE,&readfds,&writefds,0,0);
    if (selectResult < 0) {
#ifdef DEBUG
      printf("'select' failed\n");
#endif
      break;
    }

    if ( FD_ISSET(clientSocketId, &readfds) ) {
      int nBytesWritten;
#ifdef DEBUG
      printf("reading from client, writing to proxy\n");
#endif

      readByteCount = read(clientSocketId,buffer,MAXBUFFER);
#ifdef DEBUG_IFSHORT
      if ( readByteCount<150 )
	ShowString(buffer,readByteCount);
      else
	printf("Client requested %u bytes\n",readByteCount);
#else
#ifdef DEBUG
      ShowString(buffer,readByteCount);
#endif
      if (readByteCount <= 0) break;
#endif DEBUG_IFSHORT
      
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
      if (readByteCount <= 0) break;
      
      if ( writebuffer(clientSocketId, buffer, readByteCount) != readByteCount ) {
	perror("thread: write error to client");
	break;
      }

#ifdef DEBUG
      ShowString(buffer,readByteCount);
#endif
    }
  }

  close(proxySocketId);
  close(clientSocketId);

#ifdef DEBUG
   printf("connectionManager for socket %d ended\n", clientSocketId);
#endif

   return 0;
}

int main (int argc, char *argv[])
{
  struct sockaddr_in serverAddressData, keyData;
  char* portNrStr;
  int serverPort;
  int keySocketId;
  int error = 0;

  fd_set areadfds;
  fd_set awritefds;

  char shortBuffer[100];

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
   
   if ( bind(serverSocketId,(struct sockaddr*)&serverAddressData,sizeof(serverAddressData)) < 0 ) {
     perror("server: can't bind local address! retry...");
     close(serverSocketId);
     return 1;
   }
   else {
     printf("bind OK.\n");
   }
   if ( listen(serverSocketId,LISTENQUEUESIZE) ) {
     close(serverSocketId);
     return 1;
   }
   
   // Init the keyData socket
   if ( (keySocketId = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
     perror("server: can't open stream socket");
     return 1;
   }
   memset((char*)&keyData,0,sizeof(keyData));
   keyData.sin_family = AF_INET;
   keyData.sin_addr.s_addr = htonl(INADDR_ANY);
   keyData.sin_port = htons(1235);
   if ( bind(keySocketId,(struct sockaddr*)&keyData,sizeof(keyData)) < 0 ) {
     perror("server: can't bind local address of key socket");
     close(keySocketId);
     close(serverSocketId);
     return 1;
   }
   if ( listen(keySocketId,1) ) {
     close(keySocketId);
     close(serverSocketId);
     return 1;
   }

#ifdef DEBUG
   printf("Will do infinite loop\n");
#endif

   do {
     struct sockaddr_in clientAddressData;
     int clientAddressDataLength = sizeof(clientAddressData);
     int clientSocketId;

     FD_ZERO(&areadfds);
     FD_ZERO(&awritefds);
     FD_SET(serverSocketId, &areadfds);
     FD_SET(keySocketId, &areadfds);
     FD_SET(serverSocketId, &awritefds);
     FD_SET(keySocketId, &awritefds);

     shortBuffer[0] = 0;
     
#ifdef DEBUG
     printf("Waiting for 'select'\n");
#endif
     if ( select(FD_SETSIZE,&areadfds,0,0,0) < 0 ) {
       printf("Uops, select failed!\n");
       return 1;
     }

     if ( FD_ISSET(keySocketId,&areadfds) ) {
       struct sockaddr_in keyClientData;
       int
	 keyClientSocketId,
	 keyClientDataLength = sizeof(keyClientData),
	 keyClientBytes;

       printf("OK-read!!!\n");
       
       keyClientSocketId = accept(keySocketId,&keyClientData,&keyClientDataLength);

       printf("OK-read on keyClientSocketId=%d!!!\n",keyClientSocketId);

       keyClientBytes = read(keyClientSocketId,shortBuffer,30);
       
       if ( keyClientBytes < 0 ) {
	 printf("Uops, key read error!\n");
       }
       else {
	 shortBuffer[keyClientBytes] = 0;
	 fprintf(stderr,"Key=<%s>\n",shortBuffer);
       }
       shutdown(keyClientSocketId,2);
       close(keyClientSocketId);
       continue;
     }// end -> key-client


     if ( FD_ISSET(serverSocketId,&areadfds) ) {
       clientSocketId = accept(serverSocketId,(struct sockaddr*)&clientAddressData,&clientAddressDataLength);
     
#ifdef DOS_SPEC
       printf("accept OK.(%d.%d.%d.%d)\n",
	      clientAddressData.sin_addr.S_un.S_un_b.s_b1,
	      clientAddressData.sin_addr.S_un.S_un_b.s_b2,
	      clientAddressData.sin_addr.S_un.S_un_b.s_b3,
	      clientAddressData.sin_addr.S_un.S_un_b.s_b4);
#endif
       
#ifdef DEBUG
       if ( clientSocketId < 0 ) {
	 printf("Client-socket-id=%d\n",clientSocketId);
       }
#endif
       
       if (clientSocketId < 0) continue;
       
#ifdef DEBUG
       printf("Creating child for client socket %d\n", clientSocketId);
#endif
       {
	 int childPid = 0;
#ifdef HAS_FORK
#ifndef DOS_SPEC
	 childPid = fork();
#ifdef DEBUG
	 printf("Creating child PID=%d\n",childPid);
#endif
#endif DOS_SPEC
#endif HAS_FORK

	 if ( childPid < 0 ) continue;
	 
	 if (childPid == 0) {
	   close(serverSocketId);
	   error = manageConnection(clientSocketId,PROXY_HOST_ADDR_ULONG,"fuji");
#ifndef HAS_FORK
	   close(clientSocketId);
#endif
#ifdef DEBUG
	   printf("manageConnection -> DONE\n");
#endif
	   return 0;
	 }
	 
	 // parent
	 close(clientSocketId);
#ifdef DEBUG
	 printf("closed client socked %d\n",clientSocketId);
#endif
       }
     }// end -> is usual client
   } while ( error==0 );
}

