/* RandomBuf.h
   Creates and manages a random access data buffer.

   Original Author: Jerry V. Pelk
   Copyright by the National Center for Atmospheric Research
 
   Revisions:

*/

#ifndef RANDOMBUF_H
#define RANDOMBUF_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <dsmctl.h>

class RandomBuf {
public:
	RandomBuf(const int size);
  int	putBuf(const char *const src, const int offset, const int len);
  const char *const getBuf() 	{ return(buf); }
  void	releaseBuf() 		{ data_len = 0; memset(buf, 0, buf_size); }
  int	getLen()		{ return(data_len); }
  void	setLen(const int len)	{ data_len = len; }

private:
  char	*buf;				// buffer
  int	buf_size;			// buffer size
  int	data_len;			// length of data in buffer
};

#endif
