/* TcpSocket.h
   Creates and manages a TCP socket.

   Original Author: Jerry V. Pelk
   Copyright by the National Center for Atmospheric Research
 
   Revisions:

*/

#ifndef TCPSOCKET_H
#define TCPSOCKET_H

#define TCP_ACCEPT	0			// accept connections
#define TCP_CONNECT	1			// request connections

#include <stdio.h>
#include <string.h>
#include <dsmctl.h>
#include <stdlib.h>

#ifdef VXWORKS
#include <errnoLib.h>
#include <hostLib.h>
#include <in.h>
#include <ioLib.h>
#include <selectLib.h>
#include <sockLib.h>
#include <taskLib.h>
#include <sys/times.h>

#else
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#endif

class TcpSocket {
public:
  TcpSocket(int sock_num, const char *host);
  void	openSock(int accept);			// open the socket
  int	connectSock();				// make socket connection
  void	listenSock() 	{listen (tcp_conn, 1);}	// issue listen
  int	acceptSock();				// accept socket connection
  int	querySock();				// query for data received
  int	readSock(char *const dest, int len);	// read from the socket
  int	writeSock(const char *src, int length);	// write to the socket
  void	closeSock();				// close the socket

  void setRecvBufSize(int nBytes)
    {setsockopt(tcp_sock, SOL_SOCKET, SO_RCVBUF, (char*)&nBytes, sizeof(int));}

  void setSendBufSize(int nBytes)
    {setsockopt(tcp_sock, SOL_SOCKET, SO_SNDBUF, (char*)&nBytes, sizeof(int));}

  int sockId() 		{ return(tcp_sock); }	// get the socket handle

private:
  struct sockaddr_in sock_name;			// network name struct
  char tcp_host[25];				// target machine host name
  int tcp_num;					// port number
  int tcp_sock;					// socket fd
  int tcp_conn;					// connection socket fd

};

#endif
