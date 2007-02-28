
#define P2D_DATA_SZ	4096	/* PMS 2D image buffer array size */


// Header struct to place in front of all data blocks.
typedef struct
{
  short id;
  short hour;
  short minute;
  short second;
  short spare1;
  short spare2;
  short spare3;
  short tas;
  short msec;
  short overld;
  unsigned char data[P2D_DATA_SZ];	/* image buffer */
} TDParticle; 


