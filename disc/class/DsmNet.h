/* DsmNet.h
   General purpose socket communications class for a distributed sampling 
   module. 

   Original Author: Jerry V. Pelk
   Copyright by the National Center for Atmospheric Research
 
   Revisions:

*/

#ifndef DSMNET_H
#define DSMNET_H

#ifndef VXWORKS
#include <netdb.h>
#include <netinet/in.h>
#else
#include <sockLib.h>
#include <sys/times.h>
#endif

#include <time.h>
#include <dsmctl.h>
#include <header.h>
#include <messageDefs.h>

#include <SerialBuf.h>
#include <TcpSocket.h>

// Define the record id and types.
#define DSM_NET_ID		0x5555AAAA	// id word
#define DSM_NO_DATA		0		// No data
#define DSM_SYNC_DATA		1		// sync data type
#define DSM_P2D_DATA		2		// pms 2d data type
#define DSM_HVPS_DATA		3		// hvps data type
#define DSM_GREY_DATA		4		// greyscale data type
#define AVAPS_DATA		5		// avaps data type
#define DSM_MSG_DATA		8		// message data type
#define DSM_ACTIVITY_DATA	9		// network activity packet
#define DSM_MCR_DATA		6		// mcr data type
#define DSM_MASP_DATA		7		// masp data type
#define DSM_LHTDL_DATA		8		// LH TDL data type
#define DSM_MASP_HISTO		0xA		// masp histo type

// Define the header for each record passed across the network.
typedef struct {
  long	id;				// id word
  long	type;				// data type
  long	len;				// data length
} DsmNetHeader ;

class DsmNet {
public:
	DsmNet(int conn_type, int buf_size, int port, char* host, char* name, 
          void(*netMsg)(int action, char* name, char* msg_str));

  void	resetNet();			// reset the network for reconnection
  int	connectNet();			// request network connections
  int	acceptNet();			// accept network connections
  int	readNet();			// read data from the net
  int	writeNet(const char *const buf, int len, int type); // write data to net
  void	closeNet();			// close the net
  void	sendActivityPacket();		// send an activity packet	
  int	connected()	{ return(net_connect); }

  const char *netName()	{ return(net_name); }

  inline void enableNet() {net_enable = TRUE;}	// enable net data flow
  inline void suspendNet() {net_enable = FALSE;}// suspend net data flow

  bool	isDSM()		{ return(isDsm); }

  void	setRecvBufSize(int nBytes)	{ dsm_sock->setRecvBufSize(nBytes); }
  void	setSendBufSize(int nBytes)	{ dsm_sock->setSendBufSize(nBytes); }

  SerialBuf *dsm_buf;                   // sampled data buffer class

private:
  void (*netMessage)(int, char*, char*); // message handler function

  TcpSocket *dsm_sock;			// socket class

  fd_set read_mask;			// select read mask
  DsmNetHeader read_hdr;		// read data record header
  DsmNetHeader write_hdr;		// write data record header
  struct timeval sel_tout;		// select timeout struct
  str12	net_name;			// name of this net
  int	net_connect;			// network connected flag
  int	net_enable;			// data flow enabled flag
  bool	isDsm;
  bool	haveHdr;
  int	connect_type;			// TCP_CONNECT or TCP_ACCEPT 
  char	msg_str[DSM_MSG_STRING_SIZE];	// message string

  time_t	timeOfLastRcv;

};

#endif
