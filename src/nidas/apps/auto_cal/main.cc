#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QLibraryInfo>

#include "Calibrator.h"
#include "CalibrationWizard.h"


class AutoCal
{
public:
    AutoCal() {};
    int main(int argc, char** argv);
    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);
    static void setupSignals();

private:
    static Calibrator* calibrator;
};


/* static */
void AutoCal::sigAction(int sig, siginfo_t* siginfo, void* vptr) {
    cerr <<
        "received signal " << strsignal(sig) << '(' << sig << ')' <<
        ", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
        ", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
        ", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
                                                                  
    switch(sig) {                                                 
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            if (calibrator->isRunning()) {
                cout << "main: calibrator->cancel();" << endl;
                calibrator->cancel();
                calibrator->wait();
            }
            delete calibrator;
            exit(0);
    break;  
    }
}   


/* static */
Calibrator*         AutoCal::calibrator = 0;


/* static */
void AutoCal::setupSignals()
{
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset,SIGHUP);
    sigaddset(&sigset,SIGTERM);
    sigaddset(&sigset,SIGINT);
    sigprocmask(SIG_UNBLOCK,&sigset,(sigset_t*)0);
                                                  
    struct sigaction act;                         
    sigemptyset(&sigset);                         
    act.sa_mask = sigset;                         
    act.sa_flags = SA_SIGINFO;                    
    act.sa_sigaction = AutoCal::sigAction;       
    sigaction(SIGHUP,&act,(struct sigaction *)0); 
    sigaction(SIGINT,&act,(struct sigaction *)0); 
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}   


int AutoCal::main(int argc, char** argv)
{
    setupSignals();

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

    calibrator = new Calibrator(&acc);

    CalibrationWizard wizard(calibrator, &acc);

    wizard.show();

    return app.exec();
}


int main(int argc, char** argv)
{
    AutoCal autocal;
    return autocal.main(argc,argv);
}
