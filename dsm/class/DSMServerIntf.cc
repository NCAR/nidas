/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <DSMServerIntf.h>

#include <Project.h>
#include <Site.h>
#include <Datagrams.h> // defines ADS_XMLRPC_PORT

// #include <atdUtil/Logger.h>

#include <dirent.h>
#include <iostream>
#include <vector>

using namespace dsm;
using namespace std;
using namespace XmlRpc;


void GetProjectList::execute(XmlRpcValue& params, XmlRpcValue& result)
{
  if ( getenv("ADS3_DATA") == NULL ) {
    result = "ADS3_DATA environment variable not set!";
    return;
  }

  string ads3_data( getenv("ADS3_DATA") );
  DIR           *dp;
  struct dirent *dirp;
  struct stat   buf;
  int           cnt = 0;
  char          str[100];
  XmlRpcValue   temp;

  if (ads3_data[ads3_data.length()-1] != '/')
    ads3_data += '/';

  if ( (dp = opendir(ads3_data.c_str())) == NULL) {
    sprintf(str, "Can't open %s: %s", ads3_data.c_str(), strerror(errno));
    result = str;
    return;
  }

//     // TODO - use ftw() instead to obtain a full directory tree...
// #include <fts.h>
//     int  ftw(const char *dir,
//              int (*fn)(const char *file, const struct stat *sb, int flag),
//              int nopenfd);
//     int nftw(const char *dir,
//              int (*fn)(const char *file, const struct stat *sb, int flag, struct FTW *s),
//              int nopenfd, int flags);
//     FTS* dir = fts_open((char * const *)path_argv, FTS_LOGICAL, NULL);
//     (void)fts_read(dir);
//     FTSENT* ent = fts_children (dir, FTS_NAMEONLY);

  errno = 0;
  while ( (dirp = readdir(dp)) != NULL) {
    if (errno) {
      sprintf(str, "Error occured while reading %s: %s",
              ads3_data.c_str(), strerror(errno));
      result = str;
      return;
    }
    string path = ads3_data + string(dirp->d_name);

    if (stat(path.c_str(), &buf) < 0) {
      sprintf(str, "Can't determine the file status of %s: %s",
              path.c_str(), strerror(errno));
      result = str;
      return;
    }
    if (S_ISDIR(buf.st_mode))
      if (dirp->d_name[0] != '.')
        temp[cnt++] = dirp->d_name;
  }
  if (!cnt) {
    sprintf(str, "The directory %s is empty!", ads3_data.c_str());
    result = str;
    return;
  }

  result = temp;
  cerr << "GetProjectList::execute " << &result << endl;
}


void SetProject::execute(XmlRpcValue& params, XmlRpcValue& result)
{
  if ( getenv("ADS3_DATA") == NULL ) {
    result = "ADS3_DATA environment variable not set!";
    return;
  }

  string ads3_data( getenv("ADS3_DATA") );
  char   str[100];

  if (params.size() != 3) {
    sprintf(str, "expecting 3 parameters... got %d", params.size());
    result = str;
    return;
  }

  if (ads3_data[ads3_data.length()-1] != '/')
    ads3_data += '/';

  // project folder
  ads3_data += string(params[0]);
  ads3_data += '/';

  // aircraft folder
  ads3_data += string(params[1]);
  ads3_data += '/';

  // flight folder
  ads3_data += string(params[2]);
  ads3_data += '/';

  result = ads3_data;

  // TODO - jam this path into dsm_server's command line argument...
  // no not really but thats where it needs to go somehow.
  // BTW you need to re-write dsm_server to act as a loop pending
  // operation on reception of a valid project path from the web interface.

  cerr << "SetProject::execute " << &result << endl;
}


void GetDsmList::execute(XmlRpcValue& params, XmlRpcValue& result)
{
  Project*                project  = Project::getInstance();
  const list<Site*>&      sitelist = project->getSites();
  Site*                   site     = sitelist.front();
  const list<DSMConfig*>& dsms     = site->getDSMConfigs();

  list<DSMConfig*>::const_iterator di;
  for (di = dsms.begin(); di != dsms.end(); ++di) {
    DSMConfig* dsm = *di;
    result[dsm->getName()] = dsm->getLocation();
  }
  cerr << "GetDsmList::execute " << &result << endl;
}


int DSMServerIntf::run() throw(atdUtil::Exception)
{
  // Create an XMLRPC server
  _xmlrpc_server = new XmlRpcServer;

  // These constructors register methods with the XMLRPC server
  GetProjectList getprojectlist (_xmlrpc_server);
  SetProject     setproject     (_xmlrpc_server);
  GetDsmList     getdsmlist     (_xmlrpc_server);

  // DEBUG - set verbosity of the xmlrpc server HIGH...
  XmlRpc::setVerbosity(5);

  // Create the server socket on the specified port
  _xmlrpc_server->bindAndListen(ADS_XMLRPC_PORT);

  // Enable introspection
  _xmlrpc_server->enableIntrospection(true);

  // Wait for requests indefinitely
  _xmlrpc_server->work(-1.0);

  return RUN_OK;
}
