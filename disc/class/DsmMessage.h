/* DsmMessage.h
   Manages sending and receiving status and command messages.

   Original Author: Jerry V. Pelk
   Copyright by the National Center for Atmospheric Research
 
   Revisions:

*/

#ifndef DSMMESSAGE_H
#define DSMMESSAGE_H

#include <header.h>
#include <messageDefs.h>
#include <portable.h>

#ifndef VXWORKS
#include <time.h>
#include <DsmNet.h>			// Unix version runs with tcp sockets
#else
#include <string.h>
#include <MsgQueue.h>			// vxWorks version runs with message Q
#endif
 
class DsmMessage {
public:

#ifndef VXWORKS
  DsmMessage (DsmNet&);      				// Unix constructor
#else
  DsmMessage (MsgQueue& tx_msgQ, MsgQueue& rx_msgQ);	// vxWorks constructor
#endif

  int readMsg ();				// read a message from the net
  char *rxMessage()	{return rx_msg;}	// get full message str
  void relayMessage (char *msg) {strcpy (tx_msg, msg); sendMsg();}
  int rxLength()	{return strlen (rx_msg) + 1;}
  int type() 		{return rx_type;}	// get type field
  int action()		{return rx_action;}	// get action field
  int year()		{return rx_year;}	// get year field
  int month()		{return rx_month;}	// get month field
  int day()		{return rx_day;}	// get day field
  int hour()		{return rx_hour;}	// get hour field
  int minute()		{return rx_minute;}	// get minute field
  int second()		{return rx_second;}	// get second field
  int drive()		{return rx_drive;}	// get drive field
  int connector()	{return i_data1;}	// get port field
  int channel()		{return i_data2;}	// get bit # field
  int address()		{return i_data1;}	// get channel field
  int volt()		{return i_data2;}	// get voltage field
  int gain()		{return i_data3;}	// get gain field
  int offset()		{return i_data4;}	// get offset field
  int value()		{return rx_value;}	// get misc value field
  float altitude()	{return rx_alt;}	// get altitude field
  float tas()		{return rx_tas;}	// get true air speed 
  char *flight()	{return rx_flight;}	// get flight number 
  char *name()		{return rx_name;}	// get name field
  char *location()	{return rx_locn;}	// get location field
  char *string() 	{return rx_string;}	// get string field
  void sendAnalogMsg (int action, char *locn, int chan, int volt, 
                      int gain, int offset, char *msg_str);
  void sendFlightMsg (char*);			// send a flight number message
  void sendDSMTimeMsg(Hdr_blk *buf);
  void sendMcrMsg (int action, int value, char *locn, char *msg_str);
  void sendDigOutMsg (int action, char *locn, int connector,
			int channel, char *msg_str);
  void sendNetMsg (int action, char *name, char *msg_str);
  void sendPms1Msg (int action, char *name, char *locn, char *msg_str);
  void sendPms2Msg (int action, int value, char *name,char *locn,char *msg_str);
  void sendStatusMsg (char*);			// send a flight number message
  void sendTapeMsg (int action, int drive, char* msg_str);
  void sendTimeMsg(int year, int month, int day, int hour, int minute, int sec);
  void sendLogMsg (char *s, char *name);	// send a disk log message
  void logMsgToFile(char *s);			// log msg to disk file
 
private:
  void parseAnalogMsg();			// parse a date message string
  void parseDateMsg();				// parse a date message string
  void parseDSMTimeMsg();			// parse a time message string
  void parseFlightMsg();			// parse a flight num message
  void parseMcrMsg();				// parse a mcr message string
  void parseDigOutMsg();			// parse a DIGOUT message string
  void parseNetMsg();				// parse a net message string
  void parsePms1Msg();				// parse a pms 1d message 
  void parsePms2Msg();				// parse a pms 2d message 
  void parseStatusMsg();			// parse a status message 
  void parseTapeMsg();				// parse a tape message string
  void parseTasAltMsg();			// parse a tasalt message str
  void parseTimeMsg ();				// parse a time message string
  void parseLogMsg ();				// parse a disk log message

#ifndef VXWORKS
  inline void sendMsg(){dsm_net.writeNet(tx_msg,strlen(tx_msg)+1,DSM_MSG_DATA);}
  DsmNet &dsm_net;				// Unix network comm class
#else
  void sendMsg() {tx_msgQ.msgSend (tx_msg, strlen (tx_msg) + 1);}
  MsgQueue &tx_msgQ;				// vxWorks send message Q class
  MsgQueue &rx_msgQ;				// vxWorks recv message Q class
#endif

  int rx_type;					// message type
  int rx_action;				// message action
  int i_data1;					// integer data #1
  int i_data2;					// integer data #2
  int i_data3;					// integer data #3
  int i_data4;					// integer data #4
  int i_data5;					// integer data #5
  int i_data6;					// integer data #6
  int rx_year;					// time message year
  int rx_month;					// time message month
  int rx_day;					// time message day
  int rx_hour;					// time message hour
  int rx_minute;				// time message minute
  int rx_second;				// time message minute
  int rx_drive;					// tape drive number
  int rx_value;					// action values
  float rx_tas;					// true air speed
  float rx_alt;					// altitude
  str12 rx_flight;				// flight number
  str12 rx_name;				// dsm/device name
  str12 rx_locn;				// dsm/probe location
  char rx_string[DSM_MSG_STRING_SIZE];     	// message string data 
  char tx_msg[DSM_MSG_MAX_LEN];			// message to send
  char rx_msg[DSM_MSG_MAX_LEN];			// received message 

#ifndef VXWORKS
  FILE *logFP;
#endif

};

#endif

