/* SerialBuf.h
   Creates and manages a serial access data buffer.

   Original Author: Jerry V. Pelk
   Copyright by the National Center for Atmospheric Research
 
   Revisions:

*/
 
#ifndef SERIALBUF_H
#define SERIALBUF_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <dsmctl.h>

#define SERBUF_OVERFLOW	1024
 
class SerialBuf {
public:
	SerialBuf(int);
  inline char	*getBufPtr()		{ return(&buffer[buf_idx]); }
  void	incrBufIndex(int);
  int	copyToBuf(char*, int);
  inline char	*getBuf()		{ return(buffer); }
  inline void	releaseBuf()		{ buf_idx = 0; buf_full = false; }
  inline int	spaceAvail()		{ return(buf_size - buf_idx); }
  inline int	bufEmpty()		{ return(buf_idx == 0); }
  inline int	bufFull()		{ return(buf_full); }
  inline int	bufIndex()		{ return(buf_idx); }
  inline void	setBufFull()		{ buf_full = TRUE; }
 
private:
  char *buffer;            		// data buffer
  int buf_size;				// buffer size
  int buf_idx;				// current buffer index
  int buf_full;				// buffer full flag

};

#endif
