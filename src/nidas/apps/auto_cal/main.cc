#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QLibraryInfo>

#include "Calibrator.h"
#include "CalibrationWizard.h"

int main(int argc, char *argv[])
{
    // TODO find out how '.qrc' files are processed by 'qt4.py'
//  Q_INIT_RESOURCE(CalibrationWizard);

    QApplication app(argc, argv);

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
