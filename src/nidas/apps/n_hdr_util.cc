/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-06-09 17:41:23 -0600 (Sat, 09 Jun 2007) $

    $LastChangedRevision: 3899 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/apps/nidsmerge.cc $
 ********************************************************************

*/

#include <nidas/core/SampleInputHeader.h>
#include <nidas/core/UnixIOChannel.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

class HeaderUtil
{
public:

    HeaderUtil();

    int parseRunstring(int argc, char** argv) throw();

    int run() throw();

    void printHeader(const SampleInputHeader& header);

    static int main(int argc, char** argv) throw();

    static int usage(const char* argv0);

private:

    string fileName;

    string archiveVersion;

    string softwareVersion;

    string projectName;

    string systemName;

    string configName;

    string configVersion;

    bool update;

};

int HeaderUtil::parseRunstring(int argc, char** argv) throw()
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "a:s:p:y:c:v:")) != -1) {
	switch (opt_char) {
	case 'a':
            archiveVersion = optarg;
            update = true;
	    break;
	case 's':
            softwareVersion = optarg;
            update = true;
	    break;
	case 'p':
            projectName = optarg;
            update = true;
	    break;
	case 'y':
            systemName = optarg;
            update = true;
	    break;
	case 'c':
            configName = optarg;
            update = true;
	    break;
	case 'v':
            configVersion = optarg;
            update = true;
	    break;
	case '?':
	    return usage(argv[0]);
	}
    }
    if (optind < argc) fileName = argv[optind++];
    if (fileName.length() == 0) return usage(argv[0]);
    return 0;
}

/* static */
int HeaderUtil::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << "\n\
    [-a archive_version] [-s software_version] [-p project_name]\n\
    [-y system_name] [-c config_name] [-v config_version] filename\n\n\
The NIDAS header looks like so (with newlines between each line):\n\
NIDAS (ncar.ucar.edu)\n\
archive version: 1\n\
software version: 3893M\n\
project name: CHATS\n\
system name: ISFF\n\
config name: $ISFF/projects/$PROJECT/ISFF/config/low.xml\n\
\n\
config version: $LastChangedRevision: 511 $\n\
end header\n\
\n\n\
Use the above options to change the value of any of the header fields.\n\
The new header is then printed to stdout, or if no options are specified,\n\
the existing header is printed" << endl;
    return 1;
}

HeaderUtil::HeaderUtil():update(false)
{
}

/* static */
int HeaderUtil::main(int argc, char** argv) throw()
{
    HeaderUtil util;

    int res;
    
    if ((res = util.parseRunstring(argc,argv)) != 0) return res;

    return util.run();
}

void HeaderUtil::printHeader(const SampleInputHeader& header)
{
    cout << "ArchiveVersion:" << header.getArchiveVersion() << endl;
    cout << "SoftwareVersion:" << header.getSoftwareVersion() << endl;
    cout << "ProjectName:" << header.getProjectName() << endl;
    cout << "SystemName:" << header.getSystemName() << endl;
    cout << "ConfigName:" << header.getConfigName() << endl;
    cout << "ConfigVersion:" << header.getConfigVersion() << endl;
}

int HeaderUtil::run() throw()
{

    try {
        int fd = ::open(fileName.c_str(),
            (update ? O_RDWR : O_RDONLY) | O_LARGEFILE);
        if (fd < 0)
            throw n_u::IOException(fileName,"open",errno);

        UnixIOChannel io(fileName,fd);
        IOStream ios(io);

        SampleInputHeader header;

        header.check(&ios);

        if (update) {

            ios.skip(ios.available());

            if (lseek(fd,0,SEEK_SET) < 0) 
                throw n_u::IOException(fileName,"lseek",errno);

            if (archiveVersion.length() > 0)
                header.setArchiveVersion(archiveVersion);

            if (softwareVersion.length() > 0)
                header.setSoftwareVersion(softwareVersion);

            if (projectName.length() > 0)
                header.setProjectName(projectName);

            if (systemName.length() > 0)
                header.setSystemName(systemName);

            if (configName.length() > 0)
                header.setConfigName(configName);

            if (configVersion.length() > 0)
                header.setConfigVersion(configVersion);

            header.write(&ios);

            ios.flush();
        }

        printHeader(header);

        io.close();
    }
    catch (n_u::IOException& ioe) {
        cerr << ioe.what() << endl;
	return 1;
    }
    return 0;
}

/* static */
int main(int argc, char** argv)
{
    return HeaderUtil::main(argc,argv);
}

