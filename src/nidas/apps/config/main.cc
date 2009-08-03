/*
 *********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate:  $

    $LastChangedRevision:  $

    $LastChangedBy: $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/apps/config/main.cc $

    An Qt based application that allows visualization of a nidas/nimbus 
    configuration (e.g. default.xml) file. 
 ********************************************************************
*/

#include <QApplication>
#include <iostream>
#include <fstream>

#include "configwindow.h"

int usage(const char* argv0)
{
    cerr << "Usage: " << argv0 << " [xml_file]" << endl;
    cerr << "   where xml_file is an optional argument." << endl;
    return 1;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    ConfigWindow * configWin = new ConfigWindow();
    QString filename;

    if (argc > 2 )
      return usage(argv[0]);
    if (argc == 2)
      filename.append(argv[1]);
    else
      filename = configWin->getFile();

    //configWin->parseFile(filename);
    //configWin->show();
    return app.exec();
}
