/* RandomBuf.cc
   Creates and manages a random access data buffer.

   Original Author: Jerry V. Pelk 
   Copyright by the National Center for Atmospheric Research
 
   Revisions:

*/

#include <RandomBuf.h>

/******************************************************************************
** Public Functions
******************************************************************************/
RandomBuf::RandomBuf (int size) 
{
  if (!(int)(buf = (char*)malloc(size))) {
    perror ("RandomBuf::RandomBuf:malloc");
    exit(ERROR);
    }

  memset(buf, 0, size);
  buf_size = size;
  data_len = 0;

}

/*****************************************************************************/
int RandomBuf::putBuf (const char *const src, const int offset, const int len)

// Puts  len bytes from src into the buffer, beginning at the byte offset.
// Returns len, or 0 if buffer is already full.
{
  if (getLen() || ((offset + len) > buf_size)) {
    fprintf (stderr, "RandomBuf::putBuf: Buffer is full.\n");
    return FALSE;			// buffer full or not big enough
    }

  memcpy(buf + offset, src, len);
  return(TRUE);
}
