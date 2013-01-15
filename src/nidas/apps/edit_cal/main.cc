#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QLibraryInfo>

#include <iostream>

#include "MainWindow.h"

//#include "logx/Logging.h"

QString siteNameFilter;
QString varNameFilter;

int usage(const char* argv0)
{
  std::cerr << "Usage: " << argv0 << " [options]\n";
  std::cerr << "  -s       Filter table by site name.\n";
  std::cerr << "  -v       Filter table by variable name.\n";
  std::cerr << "  -h       This usage info.\n\n";
//logx::LogUsage(cerr);
  return 1;
}

int parseRunstring(int argc, char** argv)
{
    extern char *optarg;     /* set by getopt() */
//  extern int optind;       /* "   "  "        */
    int opt_char;            /* option character */

    while ((opt_char = getopt(argc, argv, "hs:v:")) != -1) {
        switch (opt_char) {
        case 's':
            siteNameFilter = optarg;
            qDebug() << "Filter table by site name:" << siteNameFilter;
            break;
        case 'v':
            varNameFilter = optarg;
            qDebug() << "Filter table by variable name:" << varNameFilter;
            break;
        case 'h':
            return usage(argv[0]);
        }
    }
   return 0;
}

/* -------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
//  logx::ParseLogArgs (argc, argv, true/*skip usage*/);

    // TODO find out how '.qrc' files are processed by 'qt4.py'
//  Q_INIT_RESOURCE(CalibrationWizard);

    // Create the application so qt can extract its options.
    QApplication app(argc, argv);

    // Parse arguments list
    int res;
    if ((res = parseRunstring(argc,argv))) return res;

    // Install international language translator
    QString translatorFileName = QLatin1String("qt_");
    translatorFileName += QLocale::system().name();
    QTranslator *translator = new QTranslator(&app);
    if (translator->load(translatorFileName, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        app.installTranslator(translator);
    
    MainWindow window;
    window.setFilterFixedString( clm_site,     siteNameFilter );
    window.setFilterFixedString( clm_var_name, varNameFilter  );
    window.show();
    res = app.exec();
    std::cout << __PRETTY_FUNCTION__ << " EXITING" << std::endl;
    return res;
}
