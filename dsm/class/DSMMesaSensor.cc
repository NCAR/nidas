/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/


#include <mesa.h>
#include <DSMMesaSensor.h>
#include <RTL_DevIoctlStore.h>

#include <asm/ioctls.h>
#include <iostream>
#include <sstream>

static FILE* fdMesaFPGAfile;
static int fdMesaFPGAfifo;
static signed long ImageLen ; /* Number of bytes in device image portion of 
                                 file. */
using namespace std;
using namespace dsm;
using namespace xercesc;

CREATOR_ENTRY_POINT(DSMMesaSensor)

DSMMesaSensor::DSMMesaSensor()
{
  cerr << __PRETTY_FUNCTION__ << endl;
}

DSMMesaSensor::~DSMMesaSensor() {
}

/*---------------------------------------------------------------------------*/
/* Function: readbytesfromfile
   Purpose: Read the specified number of bytes from the specified offset in
         the specified open file.
   Used by: Local functions.
   Returns: The number of bytes read.
   Notes:
     -The file position is not restored to its original value.
     -Don't try to read 0 bytes.
*/

static size_t readbytesfromfile(FILE *f, long fromoffset, size_t numbytes,
                                unsigned char *bufptr)
{
  if(fseek(f, fromoffset, SEEK_SET) != 0)
  {
    return 0 ;
  }
  return fread(bufptr, 1, numbytes, f) ;
}
/*---------------------------------------------------------------------------*/

/* Function: outportbwswap
   Purpose: Send a byte to the 4I34 data port with bits swapped.
   Used by: Local functions.
   Returns: Nothing.
   Notes:
*/

static void outportbwswap(unsigned thebyte)
{
  unsigned char* ptr;
  static unsigned char swaptab[256] =
  {
    0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0,
    0x30, 0xB0, 0x70, 0xF0, 0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8,
    0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8, 0x04, 0x84, 0x44, 0xC4,
    0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
    0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC,
    0x3C, 0xBC, 0x7C, 0xFC, 0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2,
    0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2, 0x0A, 0x8A, 0x4A, 0xCA,
    0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
    0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6,
    0x36, 0xB6, 0x76, 0xF6, 0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE,
    0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE, 0x01, 0x81, 0x41, 0xC1,
    0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
    0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9,
    0x39, 0xB9, 0x79, 0xF9, 0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5,
    0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5, 0x0D, 0x8D, 0x4D, 0xCD,
    0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
    0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3,
    0x33, 0xB3, 0x73, 0xF3, 0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB,
    0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB, 0x07, 0x87, 0x47, 0xC7,
    0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
    0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF,
    0x3F, 0xBF, 0x7F, 0xFF
  };

  ptr = &swaptab[thebyte];
  write(fdMesaFPGAfifo, ptr, 1);
}
/*---------------------------------------------------------------------------*/
/* Function: selectfiletype
   Purpose: Set up for processing the input file.
   Used by: Local functions.
   Returns: Nothing.
*/
static void selectfiletype(void)
{
  unsigned char b[14] ;

  /* Read in the interesting parts of the file header. */
  if(readbytesfromfile(fdMesaFPGAfile, 0, sizeof(b), b) != sizeof(b))
  {
    printf( "Unexpected end of file.\n") ;
  }

  /* Figure out what kind of file we have. */
  if((b[0] == 0x00) && (b[1] == 0x09) && (b[11] == 0x00) && (b[12] == 0x01) &&
     (b[13] == 'a'))
  {       /* Looks like a .BIT file. */
    signed long base ;
    printf("\nLooks like a .BIT file:") ;
    base = 14 ; /* Offset of design name length field. */

    /* Display file particulars. */
/*
    printf("\nDesign name:          ") ; sayfileinfo(&base) ;
    printf("\nPart I.D.:            ") ; sayfileinfo(&base) ;
    printf("\nDesign date:          ") ; sayfileinfo(&base) ;
    printf("\nDesign time:          ") ; sayfileinfo(&base) ;
*/
    if(readbytesfromfile(fdMesaFPGAfile, base, 4, b) != 4)
    {
      printf("Base address error\n");
    }
    ImageLen = (((unsigned long)b[0] << 24) |
                ((unsigned long)b[1] << 16) |
                ((unsigned long)b[2] << 8) |
                (unsigned long)b[3]) ;
    printf("\nConfiguration length: %lu bytes", ImageLen) ;

    /* We leave the file position set to the next byte in the file,
       which should be the first byte of the body of the data image. */
  }
  else if((b[0] == 0xFF) && (b[4] == 0x55) && (b[5] == 0x99) &&
          (b[6] == 0xAA) && (b[7] == 0x66))
  {       /* Looks like a PROM file. */
    printf("\nLooks like a PROM file:") ;
  }
  else
  {       /* It isn't something we know about. */
    printf( "Unknown file type.\n") ;
  }
}
/*---------------------------------------------------------------------------*/
static long filelengthq(FILE *f)
{
  long curpos, len ;

  curpos = ftell(f) ;
  if(curpos < 0)
  {
    return -1 ;
  }
  if(fseek(f, 0, SEEK_END) != 0)
  {
    return -1 ;
  }
  len = ftell(f) ;
  if(len < 0)
  {
    return -1 ;
  }
  if(fseek(f, curpos, SEEK_SET) != 0)
  {
    return -1 ;
  }
  return len ;
}
/*---------------------------------------------------------------------------*/

void DSMMesaSensor::open(int flags) throw(atdUtil::IOException)
{
  char devstr[30];
  int fd_mesa_counter[3];  // file pointers 
  int fd_mesa_radar[1];  // file pointers 
  unsigned long len,total, i;
  unsigned long filesize;

  cerr << __PRETTY_FUNCTION__ << "mps-begin" << endl;

  RTL_DSMSensor::open(flags);

  // Open up the FPGA program FIFO to the driver...
  sprintf(devstr, "/dev/mesa_program_board");
  err("opening '%s'", devstr);
  fdMesaFPGAfifo = open(devstr, O_NONBLOCK | O_WRONLY);
  err("fdMesaFPGAfifo = 0x%x", fdMesaFPGAfifo);

  // Open up the FPGA program drom disk...
  sprintf(devstr, "/opt/mesa_fpga_file.bit");
  err("opening '%s'", devstr);
  fdMesaFPGAfile = fopen(devstr, "rb");

  err("bit file opened");
  filesize = filelengthq(fdMesaFPGAfile);
  err("file size: %lu", filesize);

  //Send the Load FPGA Program ioctl
  ioctl(MESA_LOAD, &filesize, sizeof(unsigned long));
  total = 0;
  err("go select file type");
  selectfiletype();
  err("file type selected");
  do {
    total += len = fread(&buffer, 1, MAX_BUFFER, fdMesaFPGAfile);
    for(i = 0; i < len; i++){
      outportbwswap(buffer[i]);
    }
//  }while ( total < filesize );
  }while(!feof(fdMesaFPGAfile));
  err("Done sending bit file down");
  fclose(fdMesaFPGAfile);
  close(fdMesaFPGAfifo);
  err(" opening sensors...");

  // open all of the counter FIFOs
  for (int ii=0; ii < counter_channels; ii++)
  {
    sprintf(devstr, "/dev/mesa_in_%d", ii);
    fd_mesa_counter[ii] = open(devstr, O_RDONLY);
    if (fd_mesa_counter[ii] < 0)
    {
      err("failed to open '%s'", devstr);
      return 0;
    }
    err("opened '%s' @ 0x%x", devstr, fd_mesa_counter[ii]);
  }
  // open the radar FIFO
  for (int ii=0; ii < radar_channels; ii++)
  {
    sprintf(devstr, "/dev/mesa_in_%d", ii + counter_channels);
    fd_mesa_radar[ii] = open(devstr, O_RDONLY);
    if (fd_mesa_radar[ii] < 0)
    {
      err("failed to open '%s'", devstr);
      return 0;
    }
    err("opened '%s' @ 0x%x", devstr, fd_mesa_radar[ii]);
  }

  // Note: fd_set is a 1024 bit mask.
  fd_set readfds;

  // Set the counters.   
  ioctl(COUNTERS_SET, &set_counter, sizeof(set_counter));

  // Set the counters.   
  ioctl(COUNTERS_SET, &set_counter, sizeof(set_counter));

  // Set the radar.   

  cerr << __PRETTY_FUNCTION__ << "mps-end" << endl;
}

void DSMMesaSensor::close() throw(atdUtil::IOException)
{
  cerr << __PRETTY_FUNCTION__ << endl;

  RTL_DSMSensor::close();
}

void DSMMesaSensor::fromDOMElement(const DOMElement* node)
  throw(atdUtil::InvalidParameterException)
{
  RTL_DSMSensor::fromDOMElement(node);
  XDOMElement xnode(node);

  cerr << __PRETTY_FUNCTION__ << ": xnode element name: " <<
    xnode.getNodeName() << endl;
	
  DOMNode* child;
  for (child = node->getFirstChild(); child != 0;
       child = child->getNextSibling())
  {
    if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
    XDOMElement xchild((DOMElement*) child);
    const string& elname = xchild.getNodeName();

    if (!elname.compare("mesacfg")) {

      if ( xchild.getAttributeValue("desc").c_str()[0] == 'C' ) {
        set_counter.rate = strtoul(xchild.getAttributeValue("rate").c_str(),
                              NULL,0);
        set_counter.channel = strtoul(xchild.getAttributeValue("chan").c_str(),
                              NULL,0);
        counter_channels = set_counter.channel;
      }
      else if ( xchild.getAttributeValue("desc").c_str()[0] == 'R' ) {
        set_radar.rate = strtoul(xchild.getAttributeValue("rate").c_str(),
                              NULL,0);
        set_radar.channel = strtoul(xchild.getAttributeValue("chan").c_str(),
                              NULL,0);
        radar_channels = set_radar.channel;
      }
      else if ( xchild.getAttributeValue("desc").c_str()[0] == 'P' ) {
        set_PMS260X.rate = strtoul(xchild.getAttributeValue("rate").c_str(),
                              NULL,0);
        set_PMS260X.channel = strtoul(xchild.getAttributeValue("chan").c_str(),
                              NULL,0);
      }
      else {
        throw atdUtil::InvalidParameterException (__PRETTY_FUNCTION__,
          "not a valid desc:", xchild.getAttributeValue("desc").c_str());

      cerr << " desc: " << xchild.getAttributeValue("desc")
           << " rate: " << xchild.getAttributeValue("rate")
           << " chan: " << xchild.getAttributeValue("chan")  << endl;
      }
    }
  }
}
