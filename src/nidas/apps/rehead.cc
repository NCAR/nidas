/* One file application.  Type 'g++ -o rehead rehead.cc' to build.
 *
 * Convert nids .ads file to nidas .ads file.  Converts ASCII header
 * and copies all data verbatim.
 */

//  This tool changes the ASCII header of a .ads files from this:
//
//    NCAR ADS3
//    archive version: 0
//    software version: 3288M
//    project name: TREX
//    site name: GV
//    observation period name: rf02
//    xml name: $HeadURL$
//    xml version: $LastChangedRevision$
//    end header
//
//  to this:
//
//    NIDAS (ncar.ucar.edu)
//    archive version: 1
//    software version: 3397M
//    project name: TREX
//    system name: GV_N677F
//    config name: $PROJ_DIR/TREX/GV_N677F/nidas/rf01.xml
//  
//    config version: $LastChangedRevision$
//    end header
//    
#define _LARGEFILE64_SOURCE
#include <fcntl.h>
#include <cerrno>
#include <string>
#include <map>

#define CHUNK SSIZE_MAX/512

using namespace std;

char lineBuf[CHUNK];

int main(int argc, char** argv)
{
  if (argc < 3) {
    fprintf (stderr, "Usage: %s infile outfile\n", argv[0]);
    return -EINVAL;
  }
  string ifName(argv[1]);
  string ofName(argv[2]);

//  GV_test/tf01/dsm_20050920_155055.ads GV_test/dsm_20050920_155055_tf01.ads
  fprintf (stderr, "%s\n  %s\n", argv[1], argv[2]);

  int ifPtr = open64(ifName.c_str(), O_RDONLY);

  if (ifPtr < 0) {
    fprintf (stderr, "Can't open input file %s\n", ifName.c_str());
    return -errno;
  }

  struct stat64 ifStat;
  fstat64(ifPtr, &ifStat);
  mode_t ifMode = ifStat.st_mode;
  int ofPtr = creat64(ofName.c_str(), ifMode);

  if (ofPtr < 0) {
    fprintf (stderr, "Can't open output file %s\n", ofName.c_str());
    return -errno;
  }

  char linePtr;
  int  nRead, nWrite;

  map<string,string> systemMap;
  systemMap["GV"]   = "GV_N677F";
  systemMap["C130"] = "C130_N130AR";
  string project, site, observ;
  int lineLen, labelLen;
  bool endHeader = false;

  do {
    linePtr = 0;
    do {
      nRead = read(ifPtr, &lineBuf[linePtr++], 1);
    } while ( (nRead > 0) && (lineBuf[linePtr-1] != '\n') );

    if ( nRead < 0 ) {
      fprintf (stderr, "error while reading: %s\n", strerror(errno));
      goto exit;
    }
    if ( nRead > 0 ) {
      lineBuf[linePtr] = 0;
      string line(lineBuf);

      if      ( !line.find("NCAR ADS3",0) ) {
        line = "NIDAS (ncar.ucar.edu)\n";
      }
      if      ( !line.find("archive version: 0",0) ) {
        line = "archive version: 1\n";
      }
      else if ( !line.find("project name: ",0) ) {
        lineLen  = line.length();
        labelLen = strlen("project name: ");
        project  = line.substr( labelLen, lineLen - labelLen - 1 );
        // line does not change...
      }
      else if ( !line.find("site name: ",0) ) {
        lineLen  = line.length();
        labelLen = strlen("site name: ");
        site     = line.substr( labelLen, lineLen - labelLen - 1 );
        line = "system name: " + systemMap[site] + "\n";
      }
      else if ( !line.find("site: ",0) ) {
        lineLen  = line.length();
        labelLen = strlen("site: ");
        site     = line.substr( labelLen, lineLen - labelLen - 1 );
        line = "system name: " + systemMap[site] + "\n";
      }
      else if ( !line.find("obsPeriod: ",0) ) {
        lineLen  = line.length();
        labelLen = strlen("obsPeriod: ");
        observ   = line.substr( labelLen, lineLen - labelLen - 1 );
        line = "";  // line is removed...
      }
      else if ( !line.find("observation period name: ",0) ) {
        lineLen  = line.length();
        labelLen = strlen("observation period name: ");
        observ   = line.substr( labelLen, lineLen - labelLen - 1 );
        line = "";  // line is removed...
      }
      else if ( !line.find("xml name: ",0) ) {
        line = "config name: $PROJ_DIR/" +
          project + "/" + systemMap[site] + "/nidas/" + observ + ".xml\n\n";
      }
      else if ( !line.find("xml version: ",0) ) {
        line.replace(0,3,string("config"));
      }
      else if ( !line.find("end header",0) ) {
        endHeader = true;
        // line does not change...
      }
//    printf("%s", line.c_str());
      nWrite = write(ofPtr, line.c_str(), line.length());

      if ( nWrite < 0 ) {
        fprintf (stderr, "error while writing: %s\n", strerror(errno));
        goto exit;
      }
    }
  } while ( !endHeader );

//printf("\ncopying raw data...\n");

  do {
    nRead = read(ifPtr, lineBuf, CHUNK);

    if ( nRead < 0 ) {
      fprintf (stderr, "error while reading: %s\n", strerror(errno));
      goto exit;
    }
    nWrite = write(ofPtr, lineBuf, nRead);

    if ( nWrite < 0 ) {
      fprintf (stderr, "error while writing: %s\n", strerror(errno));
      goto exit;
    }
  } while ( nWrite > 0 );

//printf("\ndone\n");

exit:
  close(ifPtr);
  close(ofPtr);
  return 0;
}
