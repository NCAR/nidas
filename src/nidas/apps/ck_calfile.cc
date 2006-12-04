/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-05-23 11:30:55 -0700 (Tue, 23 May 2006) $

    $LastChangedRevision: 3364 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/nidas_reorg/src/nidas/apps/ck_config.cc $
 ********************************************************************
*/

#include <iostream>
#include <nidas/core/CalFile.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

int usage(const char* argv0)
{
    cerr << "Usage: " << argv0 << " cal_path cal_file" << endl;
    return 1;
}

int main(int argc, char** argv)
{
    if (argc < 3)
      return usage(argv[0]);

    CalFile cf;

    cf.setPath(argv[1]);
    cf.setFile(argv[2]);

    float data[10];
    n_u::UTime tlast((time_t)0);

    try {
        for (;;) {
            n_u::UTime t = cf.readTime();
            if (cf.eof()) break;
            int n = cf.readData(data,sizeof data/sizeof data[0]);
            cout << t.format(true,"%Y %m %d %H:%M:%S.%3f %Z ");
            for (int i = 0; i < n; i++) cout << data[i] << ' ';
            cout << endl;
            if (t < tlast) cerr << "backwards time at " <<
                t.format(true,"%Y %m %d %H:%M:%S.%3f %Z ") <<
                " nline=" << cf.getLineNumber() << endl;
            tlast = t;
        }
    }
    catch (const n_u::ParseException& e) {
        cerr << e.what() << endl;
    }
    catch (n_u::IOException& ioe) {
        cerr << ioe.what() << endl;
    }
    return 0;
}
