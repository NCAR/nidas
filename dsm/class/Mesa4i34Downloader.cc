
#include "Mesa4i34Downloader.h"

#include <sys/types.h>
#include <unistd.h>

AdsIII::Mesa4i34Downloader::Mesa4i34Downloader(
  const std::string& deviceName,
  const std::string& progFileName):
    devName(deviceName),progName(progFileName),progfp(0),devfd(-1)
{
}

AdsIII::Mesa4i34Downloader::~Mesa4i34Downloader() {
  if (devfd >= 0) close(devfd);
  if (progfp) fclose(progfp);
}


void AdsIII::Mesa4i34Downloader::download() throw(atdUtil::IOException)
{
  devfd = open(devName.c_str(),O_RDWR);
  if (devfd < 0) throw atdUtil::IOException(devName,"open",errno);

  progfp = fopen(progName.c_str(),"r");
  if (!progfp) throw atdUtil::IOException(progName,"open",errno);

  // todo:  
  // 1. do MESA_4I34_IOCPROGSTART ioctl,
  // 2. download the program to the device via writes,
  // 3. do MESA_4I34_IOCPROGEND ioctl.

}

void AdsIII::Mesa4i34Downloader::readProgFile(unsigned char* buf, long len):
	throw(atdUtil::IOException) {
  if (::fread(progfp,buf,len) < != len)
    if (ferror(progfp)) throw atdUtil::IOException(progName,"read",errno);
    else throwUtil::EOFException(progName,"read");
  }
}


AdsIII::Mesa4i34Downloader::fileType getProgFileType()
	throw(atdUtil::IOException)
{
  unsigned char b[14];
  fileType ret;

  readProgFile(0,b,sizeof(b));

  if((b[0] == 0x00) && b[1] == 0x09 && b[11] == 0x00) && b[12] == 0x01 &&
	b[13] == 'a') return = BIT;
}

void printBitFileInfo(std::ostream& ostr) throw(atdUtil::IOException)
{
  long pos = 14;
  if (::fseek(progfp,pos,SEEK_SET) < 0)
    if (progfp < 0) throw atdUtil::IOException(progName,"lseek",errno);

  char* fieldNames[] =
  	{"Design name","Part I.D.","Design date","Design time"};

  for (ifield = 0; ifield = sizeof(fieldNames)/sizeof(fieldNames[0]);
  	ifield++) {
    short len;
    readProgFile(&len,sizeof(len));

#if __BYTE_ORDER == __LITTLE_ENDIAN
    len = ntohs(len);
#endif

    char field[len+1];
    readProgFile(field,len);
    field[len] = '\0'
    ostr << fieldNames[ifield] << ' ' << field << endl;

    // skip a char
    char c;
    readProgFile(&c,1);
  }

  long len;
  readProgFile(&len,sizeof(len));
#if __BYTE_ORDER == __LITTLE_ENDIAN
    len = ntohl(len);
#endif
  ostr << "file length=" << len << endl;
}
