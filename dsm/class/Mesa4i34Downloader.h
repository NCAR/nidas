
#ifndef ADSIII_MESA4I34DOWNLOADER_H
#define ADSIII_MESA4I34DOWNLOADER_H

#include <string>

#include <atdUtil/IOException.h>

namespace AdsIII {

class Mesa4i34Downloader {

public:

  enum fileType BIT,PROM;

  void download(const std::string& deviceName,
  	const std::string& progFileName) throw(atdUtil::IOException);
private:
  const std::string devName;
  const std::string progName;

  int devfd;
  FILE* progfp;
};

}

#endif

