/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <nidas/core/DSMServer.h>

using namespace nidas::core;

namespace n_u = nidas::util;

bool dsmServerIsAlreadyRunning()
{
  int runCnt = 0;
  char buffer[256];
  FILE *fp = popen("ps ax | grep \"dsm_server\" | grep -v grep", "r");

  while (fgets(buffer, 256, fp) != 0)
  {
    printf(buffer);
    runCnt++;
  }

  pclose(fp);
  return runCnt > 1 ? true : false;
}

int main(int argc, char** argv)
{
  if (dsmServerIsAlreadyRunning())
  {
    std::cerr << "dsm_server is already running, exiting.\n";
    exit(1);
  }

  DSMServer::main(argc,argv);
}
