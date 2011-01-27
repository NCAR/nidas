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

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    ConfigWindow * configWin = new ConfigWindow();
    configWin->show();
    return app.exec();
}
