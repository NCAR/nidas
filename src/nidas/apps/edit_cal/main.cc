#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QUuid>
#include <QLibraryInfo>

#include <iostream>

#include "MainWindow.h"

//#include "logx/Logging.h"

void usage()
{
  std::cerr << "Usage: edit_cal [options]\n";
  std::cerr << "  --help,-h       This usage info.\n\n";
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

    // HACK - Prime the UUID generator by calling it a few times.
    // I noticed re-occurring UUIDs (between runs) being created by
    // MainWindow::cloneButtonClicked before this.
    for (int i=0; i<10; ++i)
        QUuid::createUuid();

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
    
    MainWindow window;
    window.show();
    int res = app.exec();
    std::cout << __PRETTY_FUNCTION__ << " EXITING" << std::endl;
    return res;
}
