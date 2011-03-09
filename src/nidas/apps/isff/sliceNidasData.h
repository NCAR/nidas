/*
 * sliceNidasData.h
 *
 *  Created on: Mar 3, 2011
 *      Author: dongl
 */

#ifndef SLICENIDASDATA_H_
#define SLICENIDASDATA_H_

#include <nidas/dynld/RawSampleInputStream.h>
#include <nidas/dynld/RawSampleOutputStream.h>
#include <nidas/core/HeaderSource.h>
#include <nidas/core/FileSet.h>
#include <nidas/core/Bzip2FileSet.h>
#include <nidas/core/Socket.h>
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>
#include <nidas/util/EOFException.h>
#include <nidas/core/Sample.h>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

class sliceNidasData: public HeaderSource {
public:
    sliceNidasData();
    virtual ~sliceNidasData();

    int parseRunstring(int, char**);
    void help();
    void setInputFile(list<string> files);
    void setInputFile(string filein);
    void setOutputFile(string fileout);
    void assignTms(unsigned int t1, unsigned int t2);
    void assignTms(string t1, string t2);
    int run();
    unsigned int strToInt(string str);
    void sendHeader(dsm_time_t thead,SampleOutput* out)	throw(n_u::IOException);

private:
    vector<n_u::UTime> _tms; //beginning=_tms[0] and end=_tms[1] time
  //  string _strt1, _strt2; //beginning and end time in yyyy mm dd hh:mm:ss format
    list<string> _inputFileNames;
    string _outputFileName;
    SampleInputHeader _header;

};

#endif /* SLICENIDASDATA_H_ */
