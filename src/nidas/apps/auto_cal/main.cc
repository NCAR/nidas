/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2010, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/
#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QLibraryInfo>

#include "Calibrator.h"
#include "CalibrationWizard.h"

//#include "logx/Logging.h"

void usage()
{
  cerr << "Usage: auto_cal [options]\n";
  cerr << "  --help,-h       This usage info.\n\n";
//logx::LogUsage(cerr);
}

/* --------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
//  logx::ParseLogArgs (argc, argv, true/*skip usage*/);

    // TODO find out how '.qrc' files are processed by 'qt4.py'
//  Q_INIT_RESOURCE(CalibrationWizard);

    // Create the application so qt can extract its options.
    QApplication app(argc, argv);

    // Parse arguments list
    std::vector<std::string> args(argv+1, argv+argc);
    unsigned int i = 0;
    while (i < args.size())
    {
        if (args[i] == "--help" || args[i] == "-h")
        {
            usage();
            ::exit(0);
        } 
    } 
    // Install international language translator
    QString translatorFileName = QLatin1String("qt_");
    translatorFileName += QLocale::system().name();
    QTranslator *translator = new QTranslator(&app);
    if (translator->load(translatorFileName, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        app.installTranslator(translator);
    
    AutoCalClient acc;

    Calibrator calibrator(&acc);

    CalibrationWizard wizard(&calibrator, &acc);

    wizard.show();

    return app.exec();
}
